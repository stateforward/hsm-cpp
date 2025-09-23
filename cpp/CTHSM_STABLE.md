# CTHSM - Stable Implementation

## Overview
The stable Compile-Time Hierarchical State Machine (CTHSM) implementation is located in `include/cthsm.hpp`. This provides:

1. **Compile-time model definition** using C++20 features
2. **O(1) hierarchical transition lookup** via pre-computed hash maps
3. **Full HSM feature support** (entry/exit actions, guards, activities, timers, etc.)
4. **Clean state comparison** via helper functions

## Core Files

### Stable Implementation
- `include/cthsm.hpp` - The main stable CTHSM implementation
- `include/cthsm_final.hpp` - Helper functions for clean state comparison

### Examples
- `example_cthsm_final.cpp` - Working example with state comparison
- `test_cthsm_simple.cpp` - Basic functionality test
- `test_cthsm_working.cpp` - Comprehensive feature test

## Usage

### Basic State Machine Definition
```cpp
#include "include/cthsm.hpp"
using namespace cthsm;

constexpr auto model = define("MachineName",
    initial(target("initial_state")),
    state("state1", 
        entry([](Context& ctx, Instance& inst, Event& evt) { /* ... */ }),
        transition(on("EVENT"), target("state2"))
    ),
    state("state2", /* ... */)
);

auto sm = StateMachine(model);
sm.dispatch(Event("EVENT"));
```

### Clean State Comparison
```cpp
#include "include/cthsm_final.hpp"

// Check current state
if (check_state(sm, "state_name", "MachineName")) {
    // State matches
}
```

## Key Features

### O(1) Transition Lookup
The implementation pre-computes and flattens the transition hierarchy at compile time:
- Uses `std::unordered_map<uint32_t, std::unordered_map<uint32_t, TransitionInfo>>`
- First key: state hash
- Second key: event hash
- Value: transition information

### State Identification
States are identified by hash values combining:
- Model name hash
- State path hash
- Uses `constexpr uint32_t hash(std::string_view)` for compile-time hashing
- Hierarchical paths use `combine_hashes(parent, child)`

### Compile-Time Optimization
- All model structure is computed at compile time
- Transition maps are built during construction
- No runtime string comparisons for state/event matching

## Design Decisions

1. **Hash-based state representation**: States are represented as hash values in the runtime, enabling O(1) lookups
2. **Flattened transition inheritance**: Child states inherit parent transitions at construction time
3. **Virtual state() method**: Returns string representation of current state hash for debugging
4. **Helper functions for comparison**: Since we can't override virtual methods with different return types

## Limitations

- Cannot override `state()` method to return custom types due to C++ covariant return type restrictions
- State comparison requires knowing the model name (for proper hash computation)
- State names in debug output are hash values, not human-readable strings

## Future Improvements

Potential enhancements while maintaining stability:
1. Add compile-time state name table for debugging
2. Provide state comparison without requiring model name
3. Add more compile-time validation of state machine structure