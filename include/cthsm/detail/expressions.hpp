#pragma once

#include <tuple>
#include <utility>

#include "cthsm/detail/structural_tuple.hpp"

namespace cthsm::detail {

template <typename Name, typename... Partials>
struct model_expression {
  using name_type = Name;
  using partials_type = structural_tuple<Partials...>;

  Name name{};
  structural_tuple<Partials...> elements;
};

template <typename Name, typename... Partials>
struct state_expr {
  Name name;
  structural_tuple<Partials...> elements;
};

template <typename... Partials>
struct transition_expr {
  structural_tuple<Partials...> elements;
};

template <typename... Partials>
struct initial_expr {
  structural_tuple<Partials...> elements;
};

template <typename Name, typename... Partials>
struct choice_expr {
  Name name;
  structural_tuple<Partials...> elements;
};

template <typename Name>
struct final_expr {
  Name name;
};

template <typename... Actions>
struct entry_expr {
  structural_tuple<Actions...> actions;
};

template <typename... Actions>
struct exit_expr {
  structural_tuple<Actions...> actions;
};

template <typename... Actions>
struct effect_expr {
  structural_tuple<Actions...> actions;
};

template <typename... Actions>
struct activity_expr {
  structural_tuple<Actions...> actions;
};

template <typename Callable>
struct guard_expr {
  Callable predicate;
};

template <typename Event>
struct on_expr {
  Event name;
};

template <typename Path>
struct target_expr {
  Path path;
};

template <typename Path>
struct source_expr {
  Path path;
};

template <typename... Events>
struct defer_expr {
  structural_tuple<Events...> event_names;
};

template <typename Callable>
struct after_expr {
  Callable duration;
};

template <typename Callable>
struct every_expr {
  Callable duration;
};

template <typename Callable>
struct when_expr {
  Callable predicate;
};

template <typename Callable>
struct at_expr {
  Callable time_point;
};

}  // namespace cthsm::detail
