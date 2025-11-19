# HSM: Hierarchical State Machine for C++23

A modern, header-only C++23 library for building hierarchical state machines (HSM) with a focus on clean syntax, type safety, and performance.

The library provides two implementations to suit different needs:
- **`hsm`**: A dynamic, runtime-configurable state machine library suited for applications requiring flexibility.
- **`cthsm`**: A compile-time, template-based state machine library (`constexpr`) that offers maximum performance and build-time validation.

## Features

*   **Hierarchical States**: Support for nested states with proper entry/exit cascading.
*   **Clean DSL**: Declarative syntax for defining states, transitions, and events.
*   **Thread Safety**: Built-in support for concurrent execution and thread-safe event dispatching (`hsm`).
*   **Compile-Time Validation**: `cthsm` catches invalid transitions and state configurations at build time.
*   **Zero External Dependencies**: Designed as a standalone, header-only library (tests use `doctest`).
*   **Rich Transition Support**: Guards, effects (actions), automatic path computation (LCA), and self/internal transitions.
*   **Event Deferral**: Mechanism to defer events for processing in different states.
*   **Concurrency**: Support for concurrent regions (orthogonal states) and asynchronous behaviors.

## Requirements

*   **C++ Compiler**: Requires a compiler with **C++23** support (e.g., Clang 16+, GCC 13+, MSVC 2022 17.6+).
*   **CMake**: Version 3.16 or higher.

## Installation

Since `hsm` is a header-only library, you can simply include the `include` directory in your project.

### Using CMake (FetchContent)

You can integrate `hsm` into your CMake project using `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(
    hsm
    GIT_REPOSITORY https://github.com/yourusername/hsm.git
    GIT_TAG main
)
FetchContent_MakeAvailable(hsm)

# Link against the interface library
target_link_libraries(your_target PRIVATE hsm) # or cthsm
```

## Usage

### Runtime HSM (`hsm`)

The `hsm` namespace provides a flexible, runtime-defined state machine.

```cpp
#include <iostream>
#include "hsm.hpp"

struct MyInstance : public hsm::Instance {
    int counter = 0;
};

int main() {
    // Define the state machine structure
    auto model = hsm::define("MyMachine",
        hsm::initial(hsm::target("idle")),
        
        hsm::state("idle",
            hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
                std::cout << "Entering idle\n";
            }),
            hsm::transition(
                hsm::on("START"),
                hsm::target("running"),
                hsm::effect([](hsm::Context&, hsm::Instance& i, hsm::Event&) {
                    std::cout << "Starting...\n";
                })
            )
        ),

        hsm::state("running",
            hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
                std::cout << "Running\n";
            }),
            hsm::transition(
                hsm::on("STOP"), 
                hsm::target("idle")
            )
        )
    );

    // Instantiate and start
    MyInstance instance;
    hsm::start(instance, model);

    // Dispatch events
    hsm::Context& ctx = instance.dispatch(hsm::Event("START"));
    ctx.wait(); // Wait for processing to complete

    instance.dispatch(hsm::Event("STOP")).wait();
    
    hsm::stop(instance).wait();

    return 0;
}
```

### Compile-Time HSM (`cthsm`)

The `cthsm` namespace leverages C++20/23 features to build the state machine at compile time.

```cpp
#include <iostream>
#include "cthsm/cthsm.hpp"

// Define behaviors
void on_enter_idle(cthsm::Context&, cthsm::Instance&, const cthsm::Event&) {
    std::cout << "Idle\n";
}

int main() {
    // Model definition is constexpr
    constexpr auto model = cthsm::define("CompileTimeMachine",
        cthsm::initial(cthsm::target("idle")),
        
        cthsm::state("idle",
            cthsm::entry(on_enter_idle),
            cthsm::transition(
                cthsm::on("NEXT"), 
                cthsm::target("active")
            )
        ),
        
        cthsm::state("active",
            cthsm::transition(
                cthsm::on("BACK"), 
                cthsm::target("idle")
            )
        )
    );

    // Compile the machine type
    cthsm::compile<model> machine;
    cthsm::Instance instance;

    machine.start(instance);
    
    // String-based dispatch with compile-time mapping
    machine.dispatch(instance, "NEXT");
    
    return 0;
}
```

## API Reference

### Common Elements

*   **`define(name, ...)`**: Creates the root state machine model.
*   **`state(name, ...)`**: Defines a state. Can contain transitions, actions, and nested states.
*   **`initial(target(...))`**: Defines the starting state for a region.
*   **`transition(...)`**: Defines a state transition.
    *   **`on(event_name)`**: The trigger event.
    *   **`target(state_name)`**: The destination state.
    *   **`guard(predicate)`**: A boolean function to approve/deny the transition.
    *   **`effect(action)`**: Code to run during the transition.
*   **`entry(action)`** / **`exit(action)`**: Actions to run when entering or leaving a state.

### `hsm` (Runtime) Specifics

*   **`hsm::Instance`**: Base class for your state machine instance data.
*   **`hsm::Event`**: Runtime event object carrying a name and optional `std::any` data.
*   **`hsm::start(instance, model)`**: Initializes and starts the machine.
*   **`hsm::stop(instance)`**: Gracefully stops the machine.
*   **`dispatch(event)`**: Thread-safe event queueing. Returns a `Context&` for synchronization.

### `cthsm` (Compile-Time) Specifics

*   **`cthsm::compile<model>`**: Generates the state machine type.
*   **`machine.start(instance)`**: Starts the machine.
*   **`machine.dispatch(instance, "event_name")`**: Dispatches an event.

## Building and Testing

The project uses CMake. To build the library and run tests:

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

### Running Examples

Examples are built when `BUILD_EXAMPLES` is ON (default).

```bash
./examples/cthsm_example
./examples/variadic_entry_example
```

## License

This project is available under the MIT License.