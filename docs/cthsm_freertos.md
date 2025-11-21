# Integrating cthsm with FreeRTOS

`cthsm` is designed to be platform-agnostic by decoupling its threading and timing logic via the `TaskProvider` policy. This guide demonstrates how to implement a **statically allocated** `TaskProvider` that offloads asynchronous activities and timers to FreeRTOS tasks without using the heap.

## The TaskProvider Concept

To use `cthsm` in a multi-threaded environment (like an RTOS), you must supply a `TaskProvider` struct to the `compile` template.

```cpp
template <auto Model, typename Instance, typename TaskProvider>
struct compile;
```

The `TaskProvider` must implement:
1.  `create_task`: Spawns a new thread/task.
2.  `sleep_for`: Blocks execution for a duration (cancellable via `Context`).

## Implementation (Static Allocation)

Here is a reference implementation of a `StaticFreeRTOSTaskProvider` that avoids dynamic allocation (`new`/`malloc`) by using a fixed-size object pool.

### 1. Dependencies

Ensure you have the FreeRTOS headers available and C++20 support enabled.

```cpp
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "cthsm/cthsm.hpp"
#include <chrono>
#include <array>
#include <cstddef>
#include <new> // For placement new
```

### 2. The Provider Struct

```cpp
template<size_t MaxTasks = 8, size_t StackSize = configMINIMAL_STACK_SIZE * 2>
struct StaticFreeRTOSTaskProvider {
    
    // Context for a single task
    struct TaskContext {
        // Storage for the lambda. 
        // Must be large enough to hold cthsm's captured variables (pointers to instance, context, etc).
        // 64 bytes is generally sufficient for cthsm internals.
        alignas(std::max_align_t) std::byte storage[64]; 
        
        // Function pointer to invoke/destroy the stored lambda
        void (*invoker)(void* storage) = nullptr;
        
        // FreeRTOS primitives
        StaticSemaphore_t sem_buffer;
        SemaphoreHandle_t completion_sem;
        
        StaticTask_t task_buffer;
        StackType_t stack[StackSize];
        TaskHandle_t task_handle;
        
        bool in_use = false;
    };

    // Fixed pool of task contexts
    std::array<TaskContext, MaxTasks> pool;

    StaticFreeRTOSTaskProvider() {
        for(auto& ctx : pool) {
            // Initialize binary semaphore statically
            ctx.completion_sem = xSemaphoreCreateBinaryStatic(&ctx.sem_buffer);
            ctx.in_use = false;
        }
    }

    // Handle returned to cthsm
    struct TaskHandle {
        TaskContext* context;

        void join() {
            if (context && context->completion_sem) {
                // Wait for the task to signal completion
                xSemaphoreTake(context->completion_sem, portMAX_DELAY);
                
                // Mark slot as free
                // Note: synchronization needed if create_task is called from multiple threads
                context->in_use = false;
            }
        }

        bool joinable() const {
            return context != nullptr;
        }
    };

    // Trampoline function to bridge C API to C++ Lambda
    static void task_entry_point(void* params) {
        auto* ctx = static_cast<TaskContext*>(params);
        
        if (ctx->invoker) {
            ctx->invoker(ctx->storage); // Execute the stored lambda
        }
        
        // Signal completion
        xSemaphoreGive(ctx->completion_sem);
        
        // Delete self (FreeRTOS task cleanup)
        // Note: For static tasks, vTaskDelete does not free memory, which is what we want.
        vTaskDelete(NULL);
    }

    template <typename F>
    TaskHandle create_task(F&& f, const char* name = "hsm_task", 
                           size_t /*stack_size*/ = 0, 
                           int priority = tskIDLE_PRIORITY + 1) {
        
        // 1. Find a free slot
        TaskContext* slot = nullptr;
        
        taskENTER_CRITICAL();
        for(auto& ctx : pool) {
            if (!ctx.in_use) {
                slot = &ctx;
                slot->in_use = true; // Claim immediately
                break;
            }
        }
        taskEXIT_CRITICAL();

        if (!slot) {
            // Error: Pool exhausted. 
            // In a real system, handle this gracefully or assertion failure.
            return TaskHandle{nullptr};
        }
        
        // 2. Verify storage size
        static_assert(sizeof(F) <= sizeof(slot->storage), 
            "Lambda is too large for StaticFreeRTOSTaskProvider storage");

        // 3. Move-construct lambda into storage (Placement new)
        new (slot->storage) F(std::forward<F>(f));
        
        // 4. Set type-erased invoker
        slot->invoker = [](void* storage) {
            auto& func = *reinterpret_cast<F*>(storage);
            func();
            func.~F(); // Destruct lambda after execution
        };

        // 5. Create Static Task
        slot->task_handle = xTaskCreateStatic(
            task_entry_point,
            name,
            StackSize,
            slot, // Pass context as parameter
            priority,
            slot->stack,
            &slot->task_buffer
        );

        return TaskHandle{slot};
    }

    // Cancellable Sleep using Task Notifications
    void sleep_for(std::chrono::milliseconds duration, auto* ctx) {
        const TickType_t ticks_to_wait = pdMS_TO_TICKS(duration.count());
        
        if (ctx) {
            // If using FreeRTOSContext, register this task for notifications
            if constexpr (requires { ctx->register_task(xTaskGetCurrentTaskHandle()); }) {
                ctx->register_task(xTaskGetCurrentTaskHandle());
                
                // Wait for notification (cancellation) or timeout
                uint32_t ulNotifiedValue = 0;
                xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, ticks_to_wait);
                
                // Check cancellation
                if (ulNotifiedValue > 0 || ctx->is_set()) {
                    return; // Cancelled
                }
            } else {
                // Fallback for standard Context: Polling
                // We sleep in small chunks to check the flag reasonably often
                const TickType_t poll_interval = pdMS_TO_TICKS(10); 
                TickType_t remaining = ticks_to_wait;
                
                while (remaining > 0) {
                    if (ctx->is_set()) return;
                    
                    TickType_t step = (remaining > poll_interval) ? poll_interval : remaining;
                    vTaskDelay(step);
                    remaining -= step;
                }
            }
        } else {
            // Simple delay if no context to check
            vTaskDelay(ticks_to_wait);
        }
    }
};
```

## Optimizing Cancellation with ContextPolicy

By default, `cthsm` uses `cthsm::Context`, which relies on an atomic flag. This requires the `TaskProvider` to poll the flag during sleep, which is inefficient.

You can provide a custom `ContextType` policy to use FreeRTOS task notifications for immediate, event-driven cancellation.

### 1. The FreeRTOS Context

```cpp
struct FreeRTOSContext {
    // Standard atomic flag (kept for compatibility/double-checking)
    std::atomic_bool flag_{false};
    
    // Handle of the task waiting on this context (e.g., the timer task)
    // Using void* to avoid including FreeRTOS.h in headers if desired, 
    // but here we assume access to TaskHandle_t.
    std::atomic<TaskHandle_t> task_handle_{nullptr};

    constexpr FreeRTOSContext() = default;

    // Called by cthsm to signal cancellation
    void set() {
        flag_.store(true, std::memory_order_release);
        
        // Notify the registered task if any
        TaskHandle_t handle = task_handle_.load(std::memory_order_acquire);
        if (handle) {
            // Send a notification value (e.g., 1) to wake the task
            // Note: xTaskNotify is safe to call from ISRs if using FromISR variant,
            // but cthsm usually dispatches from a task. 
            xTaskNotify(handle, 1, eSetBits);
        }
    }

    [[nodiscard]] bool is_set() const {
        return flag_.load(std::memory_order_acquire);
    }

    void reset() {
        flag_.store(false, std::memory_order_release);
        task_handle_.store(nullptr, std::memory_order_release);
    }
    
    // Custom method for our TaskProvider to use
    void register_task(TaskHandle_t h) {
        task_handle_.store(h, std::memory_order_release);
    }
};
```

### 2. Usage

Pass the custom context type as the 5th template argument to `cthsm::compile`.

```cpp
using MyHSM = cthsm::compile<
    model, 
    MyInstance, 
    StaticFreeRTOSTaskProvider<4, 512>, 
    FreeRTOSClock,
    FreeRTOSContext // <--- Injected here
>;
```

When using this configuration:
1. `cthsm` creates a `FreeRTOSContext` for the timer/activity.
2. `StaticFreeRTOSTaskProvider::sleep_for` detects the `register_task` method.
3. It registers the current task handle.
4. It calls `xTaskNotifyWait`, blocking efficiently.
5. If the HSM cancels the task (e.g., state exit), it calls `ctx.set()`.
6. `FreeRTOSContext::set()` notifies the task, waking it immediately.

This eliminates polling and ensures instant response to state transitions.

## Usage in Code

Define your state machine using the provider. You must estimate the maximum number of concurrent tasks (activities + timers) required.

```cpp
#include "cthsm/cthsm.hpp"
#include "StaticFreeRTOSTaskProvider.hpp"

// ... define model ...
constexpr auto model = cthsm::define("MyMachine", ...);

struct MyInstance : public cthsm::Instance {
    // ...
};

// Configure provider:
// MaxTasks = 4 (Allows up to 4 concurrent activities/timers)
// StackSize = 512 words
using MyProvider = StaticFreeRTOSTaskProvider<4, 512>;

// Use the provider in the template arguments
using MyHSM = cthsm::compile<model, MyInstance, MyProvider>;

void hsm_task(void* pvParameters) {
    // The provider is part of the HSM, so its pool is allocated 
    // wherever MyHSM is allocated (stack or static BSS).
    MyHSM hsm; 
    MyInstance instance;
    
    hsm.start(instance);
    
    // Dispatch events loop
    for(;;) {
        // Get event from queue...
        // hsm.dispatch(instance, event);
    }
}
```

## Key Considerations

### Static Allocation
This implementation uses `xTaskCreateStatic` and `xSemaphoreCreateBinaryStatic`. This requires `configSUPPORT_STATIC_ALLOCATION` to be set to 1 in `FreeRTOSConfig.h`.

### Memory Alignment
The `storage` buffer uses `alignas(std::max_align_t)` to ensure strict alignment requirements for any captured pointers or data types in the lambda are met.

### Concurrency Limits
The template parameter `MaxTasks` defines the hard limit on concurrent asynchronous operations. If you try to start an activity when the pool is full, `create_task` returns an empty handle, and the activity will simply not run (or you can add error handling).

### Critical Sections
If your HSM dispatches events from multiple threads (triggering transitions that spawn tasks), the loop finding a free slot in `create_task` needs to be protected by a critical section (`taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`) to prevent race conditions on `in_use` flags.

## Integrating the FreeRTOS Clock

(Same as before)

By default, `cthsm` uses `std::chrono::steady_clock`. For accurate timing on an embedded system, you should replace this with a clock that wraps the FreeRTOS tick counter.

### 1. The Clock Struct

Create a class that satisfies the C++ named requirement for a [Clock](https://en.cppreference.com/w/cpp/named_req/Clock).

```cpp
struct FreeRTOSClock {
    // Type definitions required by std::chrono
    using duration = std::chrono::milliseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<FreeRTOSClock>;
    static constexpr bool is_steady = true;

    static time_point now() noexcept {
        // xTaskGetTickCount returns TickType_t (uint32_t or uint64_t)
        TickType_t ticks = xTaskGetTickCount();
        
        // Convert ticks to milliseconds
        // Note: Be careful of overflow if using 32-bit TickType_t and high tick rates
        uint64_t ms = (static_cast<uint64_t>(ticks) * 1000ULL) / configTICK_RATE_HZ;
        
        return time_point(duration(ms));
    }
};
```

### 2. Usage

Pass the clock as the 4th template argument to `cthsm::compile`.

```cpp
using MyHSM = cthsm::compile<
    model, 
    MyInstance, 
    FreeRTOSTaskProvider, 
    FreeRTOSClock // <--- Injected here
>;
```

### 3. Implications for `at()` Timers

If you use the `at()` timer type in your model (e.g., `transition(at(get_deadline), ...)`), your handler **must** return a `FreeRTOSClock::time_point`.

```cpp
// Correct handler for FreeRTOSClock
auto get_deadline(cthsm::Context&, MyInstance&, const cthsm::EventBase&) {
    // Deadline is 5 seconds from now
    return FreeRTOSClock::now() + std::chrono::seconds(5);
}
```

Standard `std::chrono::system_clock::now()` would not be compatible.
