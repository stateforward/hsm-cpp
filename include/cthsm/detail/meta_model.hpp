#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "cthsm/detail/fixed_string.hpp"

namespace cthsm::detail {

inline constexpr std::size_t invalid_index = static_cast<std::size_t>(-1);

enum class state_flags : unsigned {
  none = 0,
  initial = 1U << 0U,
  final = 1U << 1U,
  choice = 1U << 2U,
};

constexpr state_flags operator|(state_flags lhs, state_flags rhs) noexcept {
  return static_cast<state_flags>(static_cast<unsigned>(lhs) |
                                  static_cast<unsigned>(rhs));
}

constexpr state_flags operator&(state_flags lhs, state_flags rhs) noexcept {
  return static_cast<state_flags>(static_cast<unsigned>(lhs) &
                                  static_cast<unsigned>(rhs));
}

constexpr bool any(state_flags flags, state_flags mask) noexcept {
  return static_cast<unsigned>(flags & mask) != 0U;
}

enum class transition_kind : unsigned { external, internal, self, local };

enum class timer_kind : unsigned { none, after, every, when, at };

struct state_desc {
  std::size_t id{invalid_index};
  std::size_t name_offset{0};
  std::size_t name_length{0};
  std::size_t parent_id{invalid_index};
  state_flags flags{state_flags::none};
  
  // ID of the initial transition (if any)
  std::size_t initial_transition_id{invalid_index};
  
  // Behavior indices (start index + count)
  std::size_t entry_start{invalid_index};
  std::size_t entry_count{0};
  
  std::size_t exit_start{invalid_index};
  std::size_t exit_count{0};
  
  std::size_t activity_start{invalid_index};
  std::size_t activity_count{0};
  
  // Deferred events (indices into global deferred_events array)
  std::size_t defer_start{invalid_index};
  std::size_t defer_count{0};
};

struct transition_desc {
  std::size_t id{invalid_index};
  std::size_t source_id{invalid_index};
  std::size_t target_id{invalid_index};
  transition_kind kind{transition_kind::external};
  std::size_t event_id{invalid_index};
  
  std::size_t guard_idx{invalid_index};
  
  std::size_t effect_start{invalid_index};
  std::size_t effect_count{0};
  
  timer_kind timer_type{timer_kind::none};
  std::size_t timer_idx{invalid_index};
};

struct event_desc {
  std::size_t id{invalid_index};
  std::size_t name_offset{0};
  std::size_t name_length{0};
};

template <std::size_t StateCount, std::size_t TransitionCount,
          std::size_t EventCount, std::size_t TimerCount, std::size_t DeferredCount, std::size_t StringBufferSize>
struct normalized_model_data {
  static constexpr std::size_t state_count = StateCount;
  static constexpr std::size_t transition_count = TransitionCount;
  static constexpr std::size_t event_count = EventCount;
  static constexpr std::size_t timer_count = TimerCount;
  static constexpr std::size_t deferred_count = DeferredCount;
  static constexpr std::size_t string_buffer_size = StringBufferSize;

  std::array<state_desc, StateCount> states{};
  std::array<transition_desc, TransitionCount> transitions{};
  std::array<event_desc, EventCount> events{};
  std::array<std::size_t, DeferredCount> deferred_events{}; // Stores event IDs
  std::array<char, StringBufferSize> string_buffer{};

  constexpr std::string_view get_state_name(std::size_t index) const {
    if (index >= StateCount) return {};
    const auto& s = states[index];
    return {string_buffer.data() + s.name_offset, s.name_length};
  }

  constexpr std::string_view get_event_name(std::size_t index) const {
    if (index >= EventCount) return {};
    const auto& e = events[index];
    return {string_buffer.data() + e.name_offset, e.name_length};
  }
};

}  // namespace cthsm::detail
