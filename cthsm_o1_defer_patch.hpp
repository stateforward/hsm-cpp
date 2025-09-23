// Patch to add O(1) deferred event lookup to cthsm.hpp

// Changes needed in cthsm.hpp for O(1) deferred event lookup:

// 1. Change StateInfo structure (around line 415):
/*
struct StateInfo {
  uint32_t hash;
  uint32_t parent_hash;
  std::function<void(Context&, T&, Event&)> entry;
  std::function<void(Context&, T&, Event&)> exit;
  std::vector<std::function<void(Context&, T&, Event&)>> activities;
  std::vector<uint32_t> deferred_event_hashes;  // CHANGE TO: std::unordered_set<uint32_t>
};
*/

// 2. Add hierarchical deferred set (add after StateInfo):
/*
// O(1) hierarchical deferred event lookup
std::unordered_map<uint32_t, std::unordered_set<uint32_t>> state_deferred_events_;
*/

// 3. Update process_state to use unordered_set (around line 569):
/*
} else if constexpr (is_defer<ChildType>::value) {
  child.events([&](const auto&... events) {
    ((info.deferred_event_hashes.insert(events.event_hash)), ...);  // CHANGE from push_back to insert
  }, std::tuple<>{});
}
*/

// 4. Add build_deferred_inheritance method (after build_transition_inheritance):
/*
void build_deferred_inheritance() {
  // Build hierarchical deferred event sets
  for (const auto& [state_hash, state_info] : states_) {
    auto& deferred_set = state_deferred_events_[state_hash];
    
    // Add own deferred events
    deferred_set = state_info.deferred_event_hashes;
    
    // Inherit from all ancestors
    uint32_t current = state_info.parent_hash;
    while (current != 0) {
      auto parent_it = states_.find(current);
      if (parent_it != states_.end()) {
        // Add parent's deferred events
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
*/

// 5. Update is_event_deferred to use O(1) lookup (around line 1011):
/*
bool is_event_deferred(uint32_t event_hash) {
  auto deferred_it = state_deferred_events_.find(current_state_hash_);
  if (deferred_it == state_deferred_events_.end()) return false;
  
  // O(1) lookup
  return deferred_it->second.count(event_hash) > 0;
}
*/

// 6. Call build_deferred_inheritance in constructor after build_transition_inheritance