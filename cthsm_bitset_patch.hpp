// Minimal changes to cthsm.hpp for O(1) deferred event lookup using bitsets

// Key insight: We can use a hybrid approach
// - Keep dynamic event queues (std::queue<Event>)
// - Use bitsets for O(1) deferred event checking
// - Allocate bitset size based on runtime event discovery

// Changes needed in cthsm.hpp:

// 1. Add to includes:
// #include <bitset>

// 2. Add to StateMachine private members (after line ~440):
/*
  // Event index mapping for bitset operations
  std::unordered_map<uint32_t, size_t> event_to_index_;
  size_t next_event_index_ = 0;
  static constexpr size_t MAX_UNIQUE_EVENTS = 256; // Reasonable max
  
  // O(1) deferred lookup: state_hash -> bitset of deferred events
  using DeferredBitset = std::bitset<MAX_UNIQUE_EVENTS>;
  std::unordered_map<uint32_t, DeferredBitset> state_deferred_bitsets_;
*/

// 3. Add helper method to get/create event index:
/*
  size_t get_or_create_event_index(uint32_t event_hash) {
    auto it = event_to_index_.find(event_hash);
    if (it != event_to_index_.end()) {
      return it->second;
    }
    
    if (next_event_index_ >= MAX_UNIQUE_EVENTS) {
      throw std::runtime_error("Too many unique events (max 256)");
    }
    
    size_t index = next_event_index_++;
    event_to_index_[event_hash] = index;
    return index;
  }
*/

// 4. Add method to build deferred bitsets (call from constructor):
/*
  void build_deferred_bitsets() {
    // First pass: build bitsets for own deferred events
    for (const auto& [state_hash, state_info] : states_) {
      auto& bitset = state_deferred_bitsets_[state_hash];
      
      for (uint32_t deferred_hash : state_info.deferred_event_hashes) {
        size_t index = get_or_create_event_index(deferred_hash);
        bitset.set(index);
      }
    }
    
    // Second pass: inherit from parents (hierarchical)
    for (const auto& [state_hash, state_info] : states_) {
      if (state_info.parent_hash != 0) {
        auto& bitset = state_deferred_bitsets_[state_hash];
        
        // Walk up parent chain and OR their bitsets
        uint32_t current = state_info.parent_hash;
        while (current != 0) {
          auto parent_it = states_.find(current);
          if (parent_it != states_.end()) {
            auto parent_bitset_it = state_deferred_bitsets_.find(current);
            if (parent_bitset_it != state_deferred_bitsets_.end()) {
              bitset |= parent_bitset_it->second;
            }
            current = parent_it->second.parent_hash;
          } else {
            break;
          }
        }
      }
    }
  }
*/

// 5. Replace is_event_deferred (line ~1011) with O(1) version:
/*
  bool is_event_deferred(uint32_t event_hash) {
    // Find state's deferred bitset
    auto bitset_it = state_deferred_bitsets_.find(current_state_hash_);
    if (bitset_it == state_deferred_bitsets_.end()) {
      return false;
    }
    
    // Find event index
    auto index_it = event_to_index_.find(event_hash);
    if (index_it == event_to_index_.end()) {
      return false;  // Event never deferred anywhere
    }
    
    // O(1) bitset test
    return bitset_it->second.test(index_it->second);
  }
*/

// 6. In constructor, after build_transition_inheritance(), add:
// build_deferred_bitsets();

// Summary of changes:
// - Added bitset-based deferred event tracking
// - Maintains hierarchical inheritance of deferred events
// - O(1) lookup instead of O(n) vector iteration
// - Still uses dynamic queues for flexibility
// - Limited to 256 unique events (can be increased)