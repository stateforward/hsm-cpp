# Integrating cthsm with FreeRTOS

`cthsm` is designed to be platform-agnostic by decoupling its threading and timing logic via the `TaskProvider` policy. This guide demonstrates how to implement a `TaskProvider` that offloads asynchronous activities and timers to FreeRTOS tasks.

## The TaskProvider Concept

To use `cthsm` in a multi-threaded environment (like an RTOS), you must supply a `TaskProvider` struct to the `compile` template.

```cpp
template <auto Model, typename Instance, typename TaskProvider>
struct compile;
```

The `TaskProvider` must implement:
1.  `create_task`: Spawns a new thread/task.
2.  `sleep_for`: Blocks execution for a duration (cancellable via `Context`).

## Implementation

Here is a complete reference implementation of a `FreeRTOSTaskProvider`.

### 1. Dependencies

Ensure you have the FreeRTOS headers available and C++17/20 support enabled.

```cpp
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "cthsm/cthsm.hpp"
#include <chrono>
#include <functional>
```

### 2. The Provider Struct

```cpp
struct FreeRTOSTaskProvider {
    
    // Data shared between the task and the handle
    struct SharedState {
        std::function<void()> func;
        SemaphoreHandle_t completion_sem;
        
        SharedState(std::function<void()> f) 
            : func(std::move(f)) {
            completion_sem = xSemaphoreCreateBinary();
        }
        
        ~SharedState() {
            if (completion_sem) {
                vSemaphoreDelete(completion_sem);
            }
        }
    };

    // The handle returned to cthsm
    struct TaskHandle {
        std::shared_ptr<SharedState> state;

        void join() {
            if (state && state->completion_sem) {
                // Wait for the task to signal completion
                xSemaphoreTake(state->completion_sem, portMAX_DELAY);
            }
        }

        bool joinable() const {
            return state != nullptr;
        }
    };

    // Trampoline function to bridge C API to C++ Lambda
    static void task_entry_point(void* params) {
        // Take ownership of the shared state pointer increment
        // In a real impl, you need to be careful about lifetime. 
        // Here we assume the SharedState is kept alive by the std::shared_ptr in TaskHandle
        // held by cthsm until join() is called.
        // However, create_task needs to pass something the task can use.
        
        auto* state = static_cast<SharedState*>(params);
        
        if (state->func) {
            state->func();
        }
        
        // Signal completion
        if (state->completion_sem) {
            xSemaphoreGive(state->completion_sem);
        }
        
        // Delete the task itself
        vTaskDelete(NULL);
    }

    template <typename F>
    TaskHandle create_task(F&& f, const char* name = "hsm_task", 
                           size_t stack_size = configMINIMAL_STACK_SIZE, 
                           int priority = tskIDLE_PRIORITY + 1) {
        
        // Create shared state
        // note: strict lifecycle management required here.
        // For simplicity, we allocate a raw pointer managed manually or use a simpler scheme.
        // Since 'f' captures cthsm internals, it must be executed before cthsm is destroyed.
        
        // A robust pattern involves allocating a context on heap that the task owns/frees,
        // but for join() support, we need a handle.
        
        auto state = std::make_shared<SharedState>(std::forward<F>(f));
        
        // We pass the raw pointer to the task. 
        // CAUTION: This requires 'state' to live as long as the task runs.
        // cthsm guarantees to hold TaskHandle (and thus shared_ptr) until join() returns.
        
        BaseType_t res = xTaskCreate(
            task_entry_point,
            name,
            stack_size,
            state.get(),
            priority,
            nullptr
        );

        if (res != pdPASS) {
            // Handle error (throw or log)
        }

        return TaskHandle{state};
    }

    // Cancellable Sleep
    // cthsm passes a Context* which has an atomic flag is_set().
    // We must return early if it becomes true.
    void sleep_for(std::chrono::milliseconds duration, cthsm::Context* ctx) {
        const TickType_t total_ticks = pdMS_TO_TICKS(duration.count());
        const TickType_t step = pdMS_TO_TICKS(10); // Poll every 10ms
        
        TickType_t elapsed = 0;
        
        while (elapsed < total_ticks) {
            if (ctx && ctx->is_set()) {
                return; // Cancelled
            }
            
            TickType_t remaining = total_ticks - elapsed;
            TickType_t sleep_ticks = (remaining > step) ? step : remaining;
            
            vTaskDelay(sleep_ticks);
            elapsed += sleep_ticks;
        }
    }
};
```

## Usage in Code

Define your state machine using the provider.

```cpp
#include "cthsm/cthsm.hpp"
#include "FreeRTOSTaskProvider.hpp"

// ... define model ...
constexpr auto model = cthsm::define("MyMachine", ...);

struct MyInstance : public cthsm::Instance {
    // ...
};

// Use the provider in the template arguments
using MyHSM = cthsm::compile<model, MyInstance, FreeRTOSTaskProvider>;

void hsm_task(void* pvParameters) {
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

### Stack Size
The `create_task` method takes a stack size. In `cthsm`, you can specify this when creating activities, but the current `cthsm` API defaults to 0. You may want to modify `FreeRTOSTaskProvider::create_task` to enforce a reasonable minimum (e.g., `configMINIMAL_STACK_SIZE * 2`) if the passed size is small.

### Context Implementation
`cthsm::Context` uses `std::atomic` and a spin-wait loop for its `wait()` method.
1. **`set()`**: Safe to call from any task/interrupt (uses `memory_order_release`).
2. **`wait()`**: Uses a busy loop. **Warning**: `cthsm` only calls `wait()` inside `~Context` or explicitly in user code. The framework internals generally use `join()` on the task handle instead. As long as you implement `TaskHandle::join` correctly (blocking on semaphore), the busy-wait in `Context` is rarely used.

### Memory Allocation
This implementation uses `std::make_shared` and `std::function`. Ensure your heap configuration (`configTOTAL_HEAP_SIZE`) is sufficient. If dynamic allocation is forbidden, you will need a static pool allocator for the task wrappers.

## Integrating the FreeRTOS Clock

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
