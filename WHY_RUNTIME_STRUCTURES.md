# Why Runtime Structures from Compile-Time Model?

## The Compile-Time Model

The compile-time model is a **type**, not a data structure:

```cpp
constexpr auto model = define("TrafficLight",
    initial(target("red")),
    state("red", transition(on("TIMER"), target("green"))),
    state("green", transition(on("TIMER"), target("yellow")))
);
// model is type: model_t<initial_t<...>, state_t<...>, state_t<...>>
```

## Why We Can't Use It Directly

### 1. **Template Type vs Runtime Data**
```cpp
// Compile-time: Each state is a different TYPE
state_t<entry_t<Lambda1>, transition_t<...>>  // red state TYPE
state_t<entry_t<Lambda2>, transition_t<...>>  // green state TYPE

// Runtime: We need a uniform CONTAINER
std::unordered_map<uint32_t, StateInfo>  // All states in one map
```

### 2. **No Runtime Polymorphism with Templates**
```cpp
// IMPOSSIBLE - can't store different template types in one container
std::vector<state_t<???>> states;  // What goes in ???

// POSSIBLE - uniform runtime structure
struct StateInfo {
    uint32_t hash;
    std::function<void(...)> entry;  // Type-erased function
};
std::vector<StateInfo> states;  // ✓ Works
```

### 3. **Can't Index Templates at Runtime**
```cpp
// Compile-time dispatch (template)
if constexpr (event == "TIMER") {  // Must be compile-time constant
    // handle transition
}

// Runtime dispatch (hash map)
auto transition = state_transitions_[current_state][event_hash];  // ✓ Dynamic
```

### 4. **Hierarchical Lookups Need Mutable Structures**
```cpp
// Building inheritance at compile-time would require:
template<typename State>
constexpr auto find_parent_transitions() {
    // Would need compile-time recursion through type hierarchy
    // Cannot modify/combine at compile time
}

// At runtime we can:
for (auto& [state, info] : states_) {
    inherit_from_parent(state, info.parent);  // ✓ Dynamic inheritance
}
```

## What We DO Get at Compile-Time

1. **Structure Validation** - Invalid transitions caught at compile
2. **Hash Computation** - State/event names → hash values
3. **Type Safety** - Lambda signatures verified
4. **Zero Overhead** - No virtual functions or RTTI

## The Hybrid Approach

```cpp
// Compile-time: Define structure as types
constexpr auto model = define(...);  // Pure types, zero runtime cost

// Runtime: Build efficient lookup structures
StateMachine sm(model);  // Extracts info from types into hash maps
                        // O(1) lookups, but built at runtime
```

## Could We Go Fully Compile-Time?

Theoretically yes, but it would require:

1. **Fixed-size arrays** instead of maps
2. **Perfect hash functions** computed at compile-time
3. **All states/events known** at compile-time indices
4. **No dynamic behavior** - everything predetermined

Example of what full compile-time might look like:
```cpp
template<size_t N>
struct CompileTimeStateMachine {
    // Every state/event gets compile-time index
    static constexpr size_t RED_STATE = 0;
    static constexpr size_t GREEN_STATE = 1;
    static constexpr size_t TIMER_EVENT = 0;
    
    // Fixed-size lookup table
    std::array<std::array<TransitionInfo, MAX_EVENTS>, MAX_STATES> transitions;
    
    // But this is inflexible and requires manual indexing!
};
```

## Current Design Benefits

1. **Flexibility** - Can handle any model structure
2. **Efficiency** - O(1) lookups via hash maps
3. **Type Safety** - Compile-time validation
4. **Runtime Dynamism** - Can modify behavior, defer events, etc.
5. **Best of Both** - Compile-time safety + runtime flexibility

The runtime structure building is a one-time cost at construction, giving us dynamic hash maps for O(1) operation during the state machine's lifetime.