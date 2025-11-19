#pragma once

#include <tuple>
#include <utility>

#include "cthsm/detail/expressions.hpp"
#include "cthsm/detail/structural_tuple.hpp"

namespace cthsm::detail {

// --- Tuple Concatenation Helper ---
template <typename... Tuples>
constexpr auto tuple_cat_constexpr(Tuples&&... tuples) {
    return std::tuple_cat(std::forward<Tuples>(tuples)...);
}

// --- Behavior Extraction (Pass 1: States -> Entry, Exit, Activity) ---

// Default: recurse if elements present
template <typename T>
constexpr auto extract_entries(const T& node) {
    if constexpr (requires { node.elements; }) {
        return extract_entries_tuple(node.elements);
    } else {
        return std::tuple<>{};
    }
}

template <typename Tuple, std::size_t... Is>
constexpr auto extract_entries_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_entries(get<Is>(t))...);
}

template <typename Tuple>
constexpr auto extract_entries_tuple(const Tuple& t) {
    return extract_entries_tuple_impl(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Specializations
template <typename... Actions>
constexpr auto extract_entries(const entry_expr<Actions...>& node) {
    return to_std_tuple(node.actions);
}
template <typename... Actions>
constexpr auto extract_entries(const exit_expr<Actions...>&) { return std::tuple<>{}; }
template <typename... Actions>
constexpr auto extract_entries(const activity_expr<Actions...>&) { return std::tuple<>{}; }
// Skip transitions in Pass 1
template <typename... Partials>
constexpr auto extract_entries(const transition_expr<Partials...>&) { return std::tuple<>{}; }


// Exits
template <typename T>
constexpr auto extract_exits(const T& node) {
    if constexpr (requires { node.elements; }) {
        return extract_exits_tuple(node.elements);
    } else {
        return std::tuple<>{};
    }
}
template <typename Tuple, std::size_t... Is>
constexpr auto extract_exits_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_exits(get<Is>(t))...);
}
template <typename Tuple>
constexpr auto extract_exits_tuple(const Tuple& t) {
    return extract_exits_tuple_impl(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}
template <typename... Actions>
constexpr auto extract_exits(const exit_expr<Actions...>& node) {
    return to_std_tuple(node.actions);
}
template <typename... Actions>
constexpr auto extract_exits(const entry_expr<Actions...>&) { return std::tuple<>{}; }
template <typename... Actions>
constexpr auto extract_exits(const activity_expr<Actions...>&) { return std::tuple<>{}; }
template <typename... Partials>
constexpr auto extract_exits(const transition_expr<Partials...>&) { return std::tuple<>{}; }


// Activities
template <typename T>
constexpr auto extract_activities(const T& node) {
    if constexpr (requires { node.elements; }) {
        return extract_activities_tuple(node.elements);
    } else {
        return std::tuple<>{};
    }
}
template <typename Tuple, std::size_t... Is>
constexpr auto extract_activities_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_activities(get<Is>(t))...);
}
template <typename Tuple>
constexpr auto extract_activities_tuple(const Tuple& t) {
    return extract_activities_tuple_impl(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}
template <typename... Actions>
constexpr auto extract_activities(const activity_expr<Actions...>& node) {
    return to_std_tuple(node.actions);
}
template <typename... Actions>
constexpr auto extract_activities(const entry_expr<Actions...>&) { return std::tuple<>{}; }
template <typename... Actions>
constexpr auto extract_activities(const exit_expr<Actions...>&) { return std::tuple<>{}; }
template <typename... Partials>
constexpr auto extract_activities(const transition_expr<Partials...>&) { return std::tuple<>{}; }


// --- Behavior Extraction (Pass 2: Transitions -> Guard, Effect, Timer) ---

// Helper for transition partials
template <typename Tuple, std::size_t... Is>
constexpr auto extract_guards_from_transition(const Tuple& t, std::index_sequence<Is...>);
template <typename Tuple>
constexpr auto extract_guards_from_transition_tuple(const Tuple& t) {
    return extract_guards_from_transition(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename T>
constexpr auto extract_guards_item(const T& node) {
    if constexpr (requires { node.predicate; }) { // guard_expr
        return std::make_tuple(node.predicate);
    } else {
        return std::tuple<>{};
    }
}
template <typename Tuple, std::size_t... Is>
constexpr auto extract_guards_from_transition(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_guards_item(get<Is>(t))...);
}


// Main Pass 2 recursion (States -> Transitions)
template <typename T>
constexpr auto extract_guards(const T& node);

template <typename Tuple, std::size_t... Is>
constexpr auto extract_guards_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_guards(get<Is>(t))...);
}
template <typename Tuple>
constexpr auto extract_guards_tuple(const Tuple& t) {
    return extract_guards_tuple_impl(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Default recurse (States, Model)
template <typename T>
constexpr auto extract_guards(const T& node) {
    if constexpr (requires { node.elements; }) {
        return extract_guards_tuple(node.elements);
    } else {
        return std::tuple<>{};
    }
}

// Transition Handler
template <typename... Partials>
constexpr auto extract_guards(const transition_expr<Partials...>& node) {
    return extract_guards_from_transition_tuple(node.elements);
}

// Initial Handler (needs to recurse into elements looking for transitions)
template <typename... Partials>
constexpr auto extract_guards(const initial_expr<Partials...>& node) {
    return extract_guards_tuple(node.elements); // Recursion handles transition_expr children
}


// --- Effects ---
template <typename T>
constexpr auto extract_effects_item(const T& node) {
    if constexpr (requires { node.actions; } && !requires { node.elements; }) { // effect_expr (has actions, not elements)
        // Need to distinguish effect from others if they look similar?
        // extract_effects_from_transition is only called on transition partials.
        // So only effect_expr can appear there? No, target, guard, on are there too.
        // We can match type.
        return std::tuple<>{}; 
    } else {
        return std::tuple<>{};
    }
}
// Specialization for effect_expr
template <typename... Actions>
constexpr auto extract_effects_item(const effect_expr<Actions...>& node) {
    return to_std_tuple(node.actions);
}

template <typename Tuple, std::size_t... Is>
constexpr auto extract_effects_from_transition(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_effects_item(get<Is>(t))...);
}
template <typename Tuple>
constexpr auto extract_effects_from_transition_tuple(const Tuple& t) {
    return extract_effects_from_transition(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename T>
constexpr auto extract_effects(const T& node) {
    if constexpr (requires { node.elements; }) {
        return extract_effects_tuple(node.elements);
    } else {
        return std::tuple<>{};
    }
}
template <typename Tuple, std::size_t... Is>
constexpr auto extract_effects_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_effects(get<Is>(t))...);
}
template <typename Tuple>
constexpr auto extract_effects_tuple(const Tuple& t) {
    return extract_effects_tuple_impl(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename... Partials>
constexpr auto extract_effects(const transition_expr<Partials...>& node) {
    return extract_effects_from_transition_tuple(node.elements);
}

template <typename... Actions>
constexpr auto extract_effects(const effect_expr<Actions...>& node) {
    return to_std_tuple(node.actions);
}

template <typename... Partials>
constexpr auto extract_effects(const initial_expr<Partials...>& node) {
    return extract_effects_tuple(node.elements);
}


// --- Timers ---
template <typename T>
constexpr auto extract_timers_item(const T& node) {
    if constexpr (requires { node.duration; }) { // after/every
        return std::make_tuple(node.duration);
    } else {
        return std::tuple<>{};
    }
}

template <typename Tuple, std::size_t... Is>
constexpr auto extract_timers_from_transition(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_timers_item(get<Is>(t))...);
}
template <typename Tuple>
constexpr auto extract_timers_from_transition_tuple(const Tuple& t) {
    return extract_timers_from_transition(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename T>
constexpr auto extract_timers(const T& node) {
    if constexpr (requires { node.elements; }) {
        return extract_timers_tuple(node.elements);
    } else {
        return std::tuple<>{};
    }
}
template <typename Tuple, std::size_t... Is>
constexpr auto extract_timers_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    return tuple_cat_constexpr(extract_timers(get<Is>(t))...);
}
template <typename Tuple>
constexpr auto extract_timers_tuple(const Tuple& t) {
    return extract_timers_tuple_impl(t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

template <typename... Partials>
constexpr auto extract_timers(const transition_expr<Partials...>& node) {
    return extract_timers_from_transition_tuple(node.elements);
}
template <typename... Partials>
constexpr auto extract_timers(const initial_expr<Partials...>& node) {
    return extract_timers_tuple(node.elements);
}

} // namespace cthsm::detail
