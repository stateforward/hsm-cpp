#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include "cthsm/detail/meta_model.hpp"

namespace cthsm::detail {

template <std::size_t StateCount, std::size_t TransitionCount, std::size_t EventCount, std::size_t TimerCount>
struct lookup_tables {
  struct event_entry {
      std::size_t id;
      std::string_view name;
      
      constexpr auto operator<=>(const event_entry&) const = default;
  };
  
  std::array<event_entry, EventCount> sorted_events{};
  std::array<std::array<std::size_t, EventCount>, StateCount> transition_table{};
  
  // Chain of transitions for the same event (for guard fallbacks)
  std::array<std::size_t, TransitionCount> next_candidate{};
  
  // Timer support
  // Map timer_idx -> transition_id
  std::array<std::size_t, TimerCount> timer_transition_map{};
  
  // Map state_id -> list of timers
  struct timer_ref {
      std::size_t timer_idx;
      timer_kind kind;
  };
  
  // Flattened list of timers per state.
  // We need to know size. Upper bound is TimerCount.
  // state_timer_offsets[state] -> {start, count} in state_timer_list
  struct range { std::size_t start; std::size_t count; };
  std::array<range, StateCount> state_timer_ranges{};
  std::array<timer_ref, TimerCount> state_timer_list{};
  
  // Completion transitions (eventless, non-timer)
  // Map state_id -> {start, count} in completion_transitions_list
  std::array<range, StateCount> completion_transitions_ranges{};
  // Indices into normalized_model.transitions
  std::array<std::size_t, TransitionCount> completion_transitions_list{};
  
  constexpr std::size_t get_event_id(std::string_view name) const {
      // Binary search
      auto it = std::lower_bound(sorted_events.begin(), sorted_events.end(), name, 
        [](const event_entry& e, std::string_view n) { return e.name < n; });
      
      if (it != sorted_events.end() && it->name == name) {
          return it->id;
      }
      return invalid_index;
  }
  
  constexpr std::size_t get_transition_id(std::size_t state_id, std::size_t event_id) const {
      if (state_id >= StateCount || event_id >= EventCount) return invalid_index;
      return transition_table[state_id][event_id];
  }
};

template <typename ModelData>
consteval auto build_tables(const ModelData& data) {
    constexpr auto SC = ModelData::state_count;
    constexpr auto EC = ModelData::event_count;
    constexpr auto TC = ModelData::transition_count;
    constexpr auto TmrC = ModelData::timer_count;
    
    lookup_tables<SC, TC, EC, TmrC> tables{};
    
    // 1. Build sorted event list
    for (std::size_t i = 0; i < EC; ++i) {
        tables.sorted_events[i] = { i, data.get_event_name(i) };
    }
    
    std::sort(tables.sorted_events.begin(), tables.sorted_events.end(), 
        [](const auto& a, const auto& b) { return a.name < b.name; });
        
    // 2. Build transition table
    for (auto& row : tables.transition_table) {
        row.fill(invalid_index);
    }
    tables.next_candidate.fill(invalid_index);
    
    for (std::size_t i = 0; i < EC; ) {
        std::string_view name = tables.sorted_events[i].name;
        std::size_t canonical_id = tables.sorted_events[i].id;
        
        for (std::size_t s = 0; s < SC; ++s) {
            std::size_t curr = s;
            std::size_t* link_ptr = &tables.transition_table[s][canonical_id];
            
            while (curr != invalid_index) {
                for (std::size_t t = 0; t < TC; ++t) {
                    const auto& trans = data.transitions[t];
                    if (trans.source_id == curr && 
                        trans.event_id != invalid_index &&
                        data.get_event_name(trans.event_id) == name) {
                        
                        // Found a candidate
                        if (*link_ptr == invalid_index) {
                            *link_ptr = t;
                            link_ptr = &tables.next_candidate[t];
                        }
                    }
                }
                curr = data.states[curr].parent_id;
            }
        }
        
        std::size_t next_i = i + 1;
        while (next_i < EC && tables.sorted_events[next_i].name == name) {
            next_i++;
        }
        i = next_i;
    }

    // 3. Build Timer Tables
    if constexpr (TmrC > 0) {
        std::size_t current_list_idx = 0;
        
        // For each state, find timers
        for (std::size_t s = 0; s < SC; ++s) {
            std::size_t start = current_list_idx;
            
            // Scan transitions for this state
            for (std::size_t t = 0; t < TC; ++t) {
                const auto& trans = data.transitions[t];
                if (trans.source_id == s && trans.timer_type != timer_kind::none) {
                    tables.timer_transition_map[trans.timer_idx] = t;
                    tables.state_timer_list[current_list_idx++] = { trans.timer_idx, trans.timer_type };
                }
            }
            tables.state_timer_ranges[s] = { start, current_list_idx - start };
        }
    }

    // 4. Build Completion/Choice Transition Tables
    {
        std::size_t current_list_idx = 0;
        for (std::size_t s = 0; s < SC; ++s) {
            std::size_t start = current_list_idx;
            
            for (std::size_t t = 0; t < TC; ++t) {
                const auto& trans = data.transitions[t];
                if (trans.source_id == s && 
                    trans.event_id == invalid_index && 
                    trans.timer_type == timer_kind::none &&
                    trans.kind != transition_kind::local) { // Exclude initial transitions if they are marked local?
                    // Initial transitions are usually stored in initial_transition_id, 
                    // but they might appear in the transition list too?
                    // In normalize.hpp, collect_transitions for initial() sets initial_transition_id.
                    // But collect_transitions also adds them to the transitions array.
                    // initial() transitions have kind=local.
                    // Choice transitions have kind=external (default).
                    
                    tables.completion_transitions_list[current_list_idx++] = t;
                }
            }
            tables.completion_transitions_ranges[s] = { start, current_list_idx - start };
        }
    }
    
    return tables;
}

} // namespace cthsm::detail
