# CTHSM Feature Parity Analysis

## Current Implementation Status

### ✅ Already Implemented

1. **O(1) Hierarchical Transition Lookup**
   - Implementation: `std::unordered_map<uint32_t, std::unordered_map<uint32_t, TransitionInfo>>`
   - Location: `state_transitions_` member in `cthsm.hpp`
   - Method: `build_transition_inheritance()` flattens parent transitions into children
   - Result: Any state can find applicable transitions in O(1) time

2. **Compile-Time Model Definition**
   - Full C++20 constexpr model construction
   - Hash-based state and event identification
   - Type-safe DSL with compile-time validation

3. **Core HSM Features**
   - Entry/exit actions
   - Guards with conditions
   - Effects (synchronous actions)
   - Activities (asynchronous, cancellable)
   - Timers (after/every)
   - Choice states (with required default)
   - Final states
   - Hierarchical states
   - Internal vs external transitions

### ❌ Missing for Feature Parity

1. **O(1) Hierarchical Deferred Event Lookup**
   - Current: O(n) using `std::vector<uint32_t> deferred_event_hashes`
   - Needed: O(1) using `std::unordered_set<uint32_t>`
   - Also need: Hierarchical inheritance of deferred events
   
   ```cpp
   // Current (O(n)):
   for (uint32_t deferred_hash : state_it->second.deferred_event_hashes) {
     if (deferred_hash == event_hash) return true;
   }
   
   // Needed (O(1)):
   return state_deferred_events_[current_state_hash_].count(event_hash) > 0;
   ```

2. **History States**
   - Shallow history (remember last child state)
   - Deep history (remember full state hierarchy)
   - Not currently implemented

3. **Parallel/Orthogonal Regions**
   - Multiple active states in different regions
   - Synchronized transitions
   - Not currently implemented

## Implementation Plan for O(1) Deferred Events

### Step 1: Modify StateInfo Structure
```cpp
struct StateInfo {
  uint32_t hash;
  uint32_t parent_hash;
  std::function<void(Context&, T&, Event&)> entry;
  std::function<void(Context&, T&, Event&)> exit;
  std::vector<std::function<void(Context&, T&, Event&)>> activities;
  std::unordered_set<uint32_t> deferred_event_hashes;  // Changed from vector
};
```

### Step 2: Add Hierarchical Deferred Map
```cpp
// Add to StateMachine class
std::unordered_map<uint32_t, std::unordered_set<uint32_t>> state_deferred_events_;
```

### Step 3: Build Deferred Inheritance
```cpp
void build_deferred_inheritance() {
  for (const auto& [state_hash, state_info] : states_) {
    auto& deferred_set = state_deferred_events_[state_hash];
    
    // Add own deferred events
    deferred_set = state_info.deferred_event_hashes;
    
    // Inherit from all ancestors
    uint32_t current = state_info.parent_hash;
    while (current != 0) {
      auto parent_it = states_.find(current);
      if (parent_it != states_.end()) {
        deferred_set.insert(
          parent_it->second.deferred_event_hashes.begin(),
          parent_it->second.deferred_event_hashes.end()
        );
        current = parent_it->second.parent_hash;
      } else {
        break;
      }
    }
  }
}
```

### Step 4: Update is_event_deferred
```cpp
bool is_event_deferred(uint32_t event_hash) {
  auto deferred_it = state_deferred_events_.find(current_state_hash_);
  if (deferred_it == state_deferred_events_.end()) return false;
  
  // O(1) lookup
  return deferred_it->second.count(event_hash) > 0;
}
```

## Performance Impact

### Current O(n) Performance
- For a state with 20 deferred events
- Each event dispatch checks all 20 events
- Total complexity: O(n × m) where n = deferred events, m = dispatched events

### Optimized O(1) Performance
- Single hash lookup regardless of deferred event count
- Constant time for any number of deferred events
- Total complexity: O(m) where m = dispatched events

## Summary

The CTHSM implementation in `cthsm.hpp` already has:
- ✅ O(1) hierarchical transition lookup
- ❌ O(n) hierarchical deferred event lookup (needs optimization)
- ❌ History states (not implemented)
- ❌ Parallel regions (not implemented)

The most critical optimization needed is converting deferred event lookup from O(n) to O(1), which requires:
1. Changing storage from vector to unordered_set
2. Building hierarchical deferred event inheritance
3. Using hash-based lookup instead of iteration