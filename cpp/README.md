# StateForward C++

A C++ finite state machine (FSM) library that provides a flexible and type-safe way to define and execute state machines.

## Features

- Hierarchical state machines
- Entry/exit/activity actions
- Guards and effects on transitions
- Choice pseudo-states
- Local and external transitions
- Event-based transitions
- Storage for user data

## Requirements

- C++20 compiler
- CMake 3.15 or higher
- spdlog
- stduuid
- Google Test (for running tests)

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running Tests

```bash
cd build
ctest
```

## Usage Example

```cpp
#include "stateforward.hpp"
#include <iostream>

using namespace stateforward;

int main() {
    // Create a simple state machine
    auto model = make_model({
        initial("foo"),
        state("foo"),
        state("bar"),
        transition({
            on({"foo"}),
            source("foo"),
            target("bar"),
            effect([](const Context& ctx, const Event& event) {
                std::cout << "Transitioning from foo to bar\n";
            })
        })
    });

    // Create FSM instance
    auto fsm = std::make_unique<FSM>(std::move(model));

    // Initial state should be "foo"
    std::cout << "Current state: " << fsm->state() << "\n";

    // Dispatch event to trigger transition
    fsm->dispatch(event_kind("foo"));

    // State should now be "bar"
    std::cout << "Current state: " << fsm->state() << "\n";

    return 0;
}
```

## License

MIT License 