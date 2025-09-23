# Why Not std::variant?

Great question! While `std::variant` could theoretically store different state types, it has fundamental limitations for our use case:

## The Variant Approach

```cpp
// Each state has a unique type
using RedState = state_t<entry_t<Lambda1>, transition_t<...>>;
using GreenState = state_t<entry_t<Lambda2>, transition_t<...>>;

// Could use variant
using TrafficLightVariant = std::variant<RedState, GreenState, YellowState>;
```

## Critical Problems

### 1. **No O(1) Lookup**
```cpp
// Current approach: O(1) hash map
auto state = states_[hash("red")];  // Instant lookup

// Variant approach: O(n) linear search
for (const auto& v : variant_states) {
    std::visit([](auto& state) {
        if (state.name_hash == target) { /* found */ }
    }, v);
}
```

### 2. **Type Explosion**
```cpp
// Every unique state machine needs a unique variant
using TrafficLightSM = StateMachine<std::variant<Red, Green, Yellow>>;
using DoorSM = StateMachine<std::variant<Open, Closed, Locked>>;
using ComplexSM = StateMachine<std::variant<S1, S2, S3, ..., S50>>;

// Can't have a generic StateMachine interface!
void process(StateMachine<???> sm);  // What type goes here?
```

### 3. **Static Size Limits**
```cpp
// Variant must know ALL types at compile time
template<typename Model>
using StateVariant = std::variant<???>;  // How to extract all state types from Model?

// Would need complex template metaprogramming to extract types
template<typename Model>
using StateVariant = typename extract_state_types<Model>::type;
// Results in: variant<State1, State2, ..., StateN>
```

### 4. **Memory Overhead**
```cpp
// Variant size = size of largest state + discriminator
sizeof(std::variant<State1, State2, ..., State50>);  // Could be huge!

// Current approach: uniform small structs
sizeof(StateInfo);  // Fixed, small size
```

### 5. **No Runtime Construction**
```cpp
// Can't build variant from runtime type info
uint32_t state_hash = get_next_state();  // Runtime value
auto next_state = ???;  // Can't construct variant from hash!

// Current approach works fine
auto next_state = states_[state_hash];  // ✓
```

## Why Runtime Structures Win

| Feature | Variant Approach | Runtime Structures |
|---------|-----------------|-------------------|
| Lookup Speed | O(n) | O(1) |
| Memory | Size of largest state × n | Fixed StateInfo × n |
| Generic Interface | No - each SM has unique type | Yes - all use same base |
| Runtime Flexibility | Limited | Full |
| Type Safety | Compile-time | Compile-time validation + runtime |

## The Real Insight

The compile-time model serves a different purpose:
- **Validation** - Ensure model is well-formed
- **Type Safety** - Check function signatures
- **Hash Generation** - Convert names to IDs

The runtime structures provide:
- **O(1) Performance** - Hash-based lookups
- **Flexibility** - Dynamic behavior
- **Uniformity** - Generic interfaces

We get the best of both worlds: compile-time safety with runtime efficiency.