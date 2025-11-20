#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <tuple>

#include "cthsm/detail/expressions.hpp"
#include "cthsm/detail/meta_model.hpp"
#include "cthsm/detail/structural_tuple.hpp"

namespace cthsm::detail {

// Helper templates for type checking
template <typename T> struct is_target : std::false_type {};
template <typename Path> struct is_target<target_expr<Path>> : std::true_type {};

template <typename T> struct is_on : std::false_type {};
template <typename Event> struct is_on<on_expr<Event>> : std::true_type {};

template <typename T> struct is_transition : std::false_type {};
template <typename... P> struct is_transition<transition_expr<P...>> : std::true_type {};

template <typename T> struct is_guard : std::false_type {};
template <typename C> struct is_guard<guard_expr<C>> : std::true_type {};

template <typename T> struct is_effect : std::false_type {};
template <typename... A> struct is_effect<effect_expr<A...>> : std::true_type {};

template <typename T> struct is_after : std::false_type {};
template <typename C> struct is_after<after_expr<C>> : std::true_type {};

template <typename T> struct is_every : std::false_type {};
template <typename C> struct is_every<every_expr<C>> : std::true_type {};

// Helper to check for direct target in tuple (for implicit initial transition)
template <typename Tuple, std::size_t I>
consteval bool has_direct_target_check() {
    if constexpr (I >= std::tuple_size_v<Tuple>) return false;
    else {
         using Type = std::decay_t<std::tuple_element_t<I, Tuple>>;
         if constexpr (is_target<Type>::value) return true;
         else return has_direct_target_check<Tuple, I+1>();
    }
}

// --- COUNTING ---

struct model_counts {
  std::size_t states = 0;
  std::size_t transitions = 0;
  std::size_t events = 0;
  std::size_t string_size = 0;
  std::size_t entries = 0;
  std::size_t exits = 0;
  std::size_t activities = 0;
  std::size_t guards = 0;
  std::size_t effects = 0;
  std::size_t timers = 0;
  std::size_t deferred_entries = 0;

  constexpr model_counts operator+(const model_counts& other) const {
    return {
        states + other.states,
        transitions + other.transitions,
        events + other.events,
        string_size + other.string_size,
        entries + other.entries,
        exits + other.exits,
        activities + other.activities,
        guards + other.guards,
        effects + other.effects,
        timers + other.timers,
        deferred_entries + other.deferred_entries
    };
  }
};

// Forward declarations
template <typename T>
consteval model_counts count_recursive(const T& node, std::size_t parent_path_len);

template <typename Tuple, std::size_t I>
consteval model_counts count_tuple(const Tuple& t, std::size_t parent_path_len);

template <typename Tuple>
consteval model_counts count_partials(const Tuple& t, std::size_t parent_path_len) {
  return count_tuple<Tuple, 0>(t, parent_path_len);
}

template <typename Tuple, std::size_t I>
consteval model_counts count_tuple(const Tuple& t, std::size_t parent_path_len) {
  if constexpr (I >= std::tuple_size_v<Tuple>) {
    return {};
  } else {
    return count_recursive(get<I>(t), parent_path_len) +
           count_tuple<Tuple, I + 1>(t, parent_path_len);
  }
}

// Default case
template <typename T>
consteval model_counts count_recursive(const T& node, std::size_t parent_path_len) {
  if constexpr (requires { node.elements; }) {
    return count_partials(node.elements, parent_path_len);
  } else {
    return {};
  }
}

// Model Expression (Root)
template <typename Name, typename... Partials>
consteval model_counts count_recursive(
    const model_expression<Name, Partials...>& node, std::size_t parent_path_len) {
  std::size_t current_len = parent_path_len + 1 + node.name.size();
  model_counts counts = count_partials(node.elements, current_len);
  counts.states += 1;
  counts.string_size += current_len;
  return counts;
}

// State Expression
template <typename Name, typename... Partials>
consteval model_counts count_recursive(
    const state_expr<Name, Partials...>& node, std::size_t parent_path_len) {
  std::size_t current_len = parent_path_len + 1 + node.name.size();
  model_counts counts = count_partials(node.elements, current_len);
  counts.states += 1;
  counts.string_size += current_len;
  return counts;
}

// Choice Expression
template <typename Name, typename... Partials>
consteval model_counts count_recursive(
    const choice_expr<Name, Partials...>& node, std::size_t parent_path_len) {
  std::size_t current_len = parent_path_len + 1 + node.name.size();
  model_counts counts = count_partials(node.elements, current_len);
  counts.states += 1;
  counts.string_size += current_len;
  return counts;
}

// Final Expression
template <typename Name>
consteval model_counts count_recursive(
    const final_expr<Name>& node, std::size_t parent_path_len) {
  std::size_t current_len = parent_path_len + 1 + node.name.size();
  model_counts counts{};
  counts.states += 1;
  counts.string_size += current_len;
  return counts;
}

// Transition Expression
template <typename... Partials>
consteval model_counts count_recursive(
    const transition_expr<Partials...>& node, std::size_t parent_path_len) {
  model_counts counts = count_partials(node.elements, parent_path_len);
  counts.transitions += 1;
  return counts;
}

// On (Event) Expression
template <typename Event>
consteval model_counts count_recursive(const on_expr<Event>& node, std::size_t) {
  model_counts c{};
  c.events = 1;
  c.string_size = node.name.size();
  return c;
}

// Defer Expression
template <typename Tuple, std::size_t I>
consteval model_counts count_defer_tuple(const Tuple& t) {
  if constexpr (I >= std::tuple_size_v<Tuple>) {
    return {};
  } else {
    model_counts c{};
    c.events = 1;
    c.string_size = get<I>(t).size();
    return c + count_defer_tuple<Tuple, I + 1>(t);
  }
}

template <typename... Events>
consteval model_counts count_recursive(const defer_expr<Events...>& node, std::size_t) {
  model_counts c = count_defer_tuple<decltype(node.event_names), 0>(node.event_names);
  c.deferred_entries = sizeof...(Events);
  return c;
}

template <typename... Actions>
consteval model_counts count_recursive(const entry_expr<Actions...>&, std::size_t) {
  return model_counts{.entries = sizeof...(Actions)};
}

template <typename... Actions>
consteval model_counts count_recursive(const exit_expr<Actions...>&, std::size_t) {
  return model_counts{.exits = sizeof...(Actions)};
}

template <typename... Actions>
consteval model_counts count_recursive(const activity_expr<Actions...>&, std::size_t) {
  return model_counts{.activities = sizeof...(Actions)};
}

template <typename... Actions>
consteval model_counts count_recursive(const effect_expr<Actions...>&, std::size_t) {
  return model_counts{.effects = sizeof...(Actions)};
}

template <typename Callable>
consteval model_counts count_recursive(const guard_expr<Callable>&, std::size_t) {
  return model_counts{.guards = 1};
}

template <typename Callable>
consteval model_counts count_recursive(const after_expr<Callable>&, std::size_t) {
  return model_counts{.timers = 1};
}

template <typename Callable>
consteval model_counts count_recursive(const every_expr<Callable>&, std::size_t) {
  return model_counts{.timers = 1};
}

// Initial Expression
template <typename... Partials>
consteval model_counts count_recursive(
    const initial_expr<Partials...>& node, std::size_t parent_path_len) {
    
    model_counts c = count_partials(node.elements, parent_path_len);
    
    // Check if we need implicit transition (if target is present directly)
    if constexpr (has_direct_target_check<decltype(node.elements), 0>()) {
        c.transitions += 1;
    }
    return c;
}


// --- POPULATION ---

template <typename ModelData>
struct populate_ctx {
    std::size_t state_idx = 0;
    std::size_t transition_idx = 0;
    std::size_t event_idx = 0;
    std::size_t string_cursor = 0;
    
    std::size_t entry_idx = 0;
    std::size_t exit_idx = 0;
    std::size_t activity_idx = 0;
    std::size_t guard_idx = 0;
    std::size_t effect_idx = 0;
    std::size_t timer_idx = 0;
    std::size_t defer_idx = 0;
    
    constexpr void append_string(ModelData& data, std::string_view str) {
        for (char c : str) {
            data.string_buffer[string_cursor++] = c;
        }
    }
};

// Helper to find state ID by absolute path
template <typename ModelData>
constexpr std::size_t find_state_id(const ModelData& data, std::string_view path) {
    for (std::size_t i = 0; i < data.state_count; ++i) {
        if (data.get_state_name(i) == path) return i;
    }
    return invalid_index;
}

// Check if state matches parent + "/" + name
template <typename ModelData>
constexpr std::size_t find_child_state(const ModelData& data, std::string_view parent_path, std::string_view name) {
    for (std::size_t i = 0; i < data.state_count; ++i) {
        std::string_view candidate = data.get_state_name(i);
        
        // Check length
        if (candidate.size() != parent_path.size() + 1 + name.size()) continue;
        
        // Check parent prefix
        if (!candidate.starts_with(parent_path)) continue;
        
        // Check separator
        if (candidate[parent_path.size()] != '/') continue;
        
        // Check suffix
        if (candidate.substr(parent_path.size() + 1) != name) continue;
        
        return i;
    }
    return invalid_index;
}

// Helper to resolve target ID
template <typename ModelData>
constexpr std::size_t resolve_target(const ModelData& data, std::string_view target_path, std::size_t source_id) {
    // 1. Exact match (Absolute)
    std::size_t id = find_state_id(data, target_path);
    if (id != invalid_index) return id;
    
    // 2. Relative resolution
    if (source_id != invalid_index) {
        std::string_view source_path = data.get_state_name(source_id);
        
        // 2a. Check Child (source/target)
        id = find_child_state(data, source_path, target_path);
        if (id != invalid_index) return id;
        
        // 2b. Check Sibling (parent(source)/target)
        std::size_t parent_id = data.states[source_id].parent_id;
        std::string_view parent_path = (parent_id != invalid_index) ? data.get_state_name(parent_id) : "";
        
        // If parent is invalid (source is root), parent_path is "".
        // If source is root, 2b checks root's siblings? Root has no siblings.
        // But if source is root, 2a checked root's children.
        // If parent is invalid, we already checked children. 
        // But find_child_state("", "name") checks for "/name".
        // Our states are absolute paths starting with /.
        // Root name is "/root". parent is invalid.
        
        if (parent_id != invalid_index) {
             id = find_child_state(data, parent_path, target_path);
             if (id != invalid_index) return id;
        } else {
             // Source is root?
             // Special case: parent of source is invalid.
             // We can try to resolve against ROOT siblings? No.
        }
    }
    return invalid_index;
}

// Forward decls for population
template <typename ModelData, typename T>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const T& node, std::string_view parent_path, std::size_t parent_id);

template <typename ModelData, typename Tuple, std::size_t I>
constexpr void collect_states_tuple(ModelData& data, populate_ctx<ModelData>& ctx, 
                                    const Tuple& t, std::string_view parent_path, std::size_t parent_id);

template <typename ModelData, typename T>
constexpr void collect_transitions(ModelData& data, populate_ctx<ModelData>& ctx,
                                   const T& node, std::size_t current_state_id);

template <typename ModelData, typename Tuple, std::size_t I>
constexpr void collect_transitions_tuple(ModelData& data, populate_ctx<ModelData>& ctx,
                                         const Tuple& t, std::size_t current_state_id);


// --- Implementations: Collect States ---

template <typename ModelData, typename Tuple>
constexpr void collect_states_partials(ModelData& data, populate_ctx<ModelData>& ctx, 
                                       const Tuple& t, std::string_view parent_path, std::size_t parent_id) {
    collect_states_tuple<ModelData, Tuple, 0>(data, ctx, t, parent_path, parent_id);
}

template <typename ModelData, typename Tuple, std::size_t I>
constexpr void collect_states_tuple(ModelData& data, populate_ctx<ModelData>& ctx, 
                                    const Tuple& t, std::string_view parent_path, std::size_t parent_id) {
    if constexpr (I < std::tuple_size_v<Tuple>) {
        collect_states(data, ctx, get<I>(t), parent_path, parent_id);
        collect_states_tuple<ModelData, Tuple, I + 1>(data, ctx, t, parent_path, parent_id);
    }
}

// Default
template <typename ModelData, typename T>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const T& node, std::string_view parent_path, std::size_t parent_id) {
    if constexpr (requires { node.elements; }) {
        collect_states_partials(data, ctx, node.elements, parent_path, parent_id);
    }
}

// Defer tuple collector
template <typename ModelData, typename Tuple, std::size_t I>
constexpr void collect_defer_tuple(ModelData& data, populate_ctx<ModelData>& ctx, const Tuple& t) {
    if constexpr (I < std::tuple_size_v<Tuple>) {
        std::string_view name = get<I>(t).view();
        std::size_t id = ctx.event_idx++;
        std::size_t offset = ctx.string_cursor;
        ctx.append_string(data, name);
        data.events[id] = event_desc{
            .id = id,
            .name_offset = offset,
            .name_length = name.size()
        };
        data.deferred_events[ctx.defer_idx++] = id;
        collect_defer_tuple<ModelData, Tuple, I + 1>(data, ctx, t);
    }
}

template <typename ModelData, typename... Events>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const defer_expr<Events...>& node, std::string_view, std::size_t state_id) {
    data.states[state_id].defer_start = ctx.defer_idx;
    data.states[state_id].defer_count = sizeof...(Events);
    collect_defer_tuple<ModelData, decltype(node.event_names), 0>(data, ctx, node.event_names);
}

template <typename ModelData, typename... Actions>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const entry_expr<Actions...>&, std::string_view, std::size_t state_id) {
    if (data.states[state_id].entry_start == invalid_index) {
        data.states[state_id].entry_start = ctx.entry_idx;
    }
    data.states[state_id].entry_count += sizeof...(Actions);
    ctx.entry_idx += sizeof...(Actions);
}

template <typename ModelData, typename... Actions>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const exit_expr<Actions...>&, std::string_view, std::size_t state_id) {
    if (data.states[state_id].exit_start == invalid_index) {
        data.states[state_id].exit_start = ctx.exit_idx;
    }
    data.states[state_id].exit_count += sizeof...(Actions);
    ctx.exit_idx += sizeof...(Actions);
}

template <typename ModelData, typename... Actions>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const activity_expr<Actions...>&, std::string_view, std::size_t state_id) {
    if (data.states[state_id].activity_start == invalid_index) {
        data.states[state_id].activity_start = ctx.activity_idx;
    }
    data.states[state_id].activity_count += sizeof...(Actions);
    ctx.activity_idx += sizeof...(Actions);
}

// Helper for State-like nodes
template <typename ModelData, typename T>
constexpr void collect_state_node(ModelData& data, populate_ctx<ModelData>& ctx, 
                                  const T& node, std::string_view parent_path, std::size_t parent_id,
                                  state_flags flags = state_flags::none) {
    std::size_t id = ctx.state_idx++;
    std::size_t offset = ctx.string_cursor;
    
    if (!parent_path.empty()) {
        ctx.append_string(data, parent_path);
    }
    
    ctx.append_string(data, "/");
    ctx.append_string(data, node.name.view());
    
    std::size_t length = ctx.string_cursor - offset;
    
    data.states[id] = state_desc{
        .id = id,
        .name_offset = offset,
        .name_length = length,
        .parent_id = parent_id,
        .flags = flags
    };
    
    std::string_view current_path = data.get_state_name(id);
    collect_states_partials(data, ctx, node.elements, current_path, id);
}

template <typename ModelData, typename Name, typename... Partials>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const model_expression<Name, Partials...>& node, std::string_view parent_path, std::size_t parent_id) {
    collect_state_node(data, ctx, node, parent_path, parent_id);
}

template <typename ModelData, typename Name, typename... Partials>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const state_expr<Name, Partials...>& node, std::string_view parent_path, std::size_t parent_id) {
    collect_state_node(data, ctx, node, parent_path, parent_id);
}

template <typename ModelData, typename Name, typename... Partials>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const choice_expr<Name, Partials...>& node, std::string_view parent_path, std::size_t parent_id) {
    collect_state_node(data, ctx, node, parent_path, parent_id, state_flags::choice);
}

template <typename ModelData, typename Name>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const final_expr<Name>& node, std::string_view parent_path, std::size_t parent_id) {
    std::size_t id = ctx.state_idx++;
    std::size_t offset = ctx.string_cursor;
    
    if (!parent_path.empty()) {
        ctx.append_string(data, parent_path);
    }
    
    ctx.append_string(data, "/");
    ctx.append_string(data, node.name.view());
    
    std::size_t length = ctx.string_cursor - offset;
    
    data.states[id] = state_desc{
        .id = id,
        .name_offset = offset,
        .name_length = length,
        .parent_id = parent_id,
        .flags = state_flags::final
    };
}

template <typename ModelData, typename... Partials>
constexpr void collect_states(ModelData& data, populate_ctx<ModelData>& ctx, 
                              const initial_expr<Partials...>& node, std::string_view parent_path, std::size_t parent_id) {
    collect_states_partials(data, ctx, node.elements, parent_path, parent_id);
}

// --- Implementations: Collect Transitions ---

template <typename ModelData, typename Tuple>
constexpr void collect_transitions_partials(ModelData& data, populate_ctx<ModelData>& ctx, 
                                            const Tuple& t, std::size_t current_state_id) {
    collect_transitions_tuple<ModelData, Tuple, 0>(data, ctx, t, current_state_id);
}

template <typename ModelData, typename Tuple, std::size_t I>
constexpr void collect_transitions_tuple(ModelData& data, populate_ctx<ModelData>& ctx,
                                         const Tuple& t, std::size_t current_state_id) {
    if constexpr (I < std::tuple_size_v<Tuple>) {
        collect_transitions(data, ctx, get<I>(t), current_state_id);
        collect_transitions_tuple<ModelData, Tuple, I + 1>(data, ctx, t, current_state_id);
    }
}

// Default
template <typename ModelData, typename T>
constexpr void collect_transitions(ModelData& data, populate_ctx<ModelData>& ctx,
                                   const T& node, std::size_t current_state_id) {
    if constexpr (requires { node.elements; }) {
        collect_transitions_partials(data, ctx, node.elements, current_state_id);
    }
}

// State/Model: update current_state_id
template <typename ModelData, typename Name, typename... Partials>
constexpr void collect_transitions(ModelData& data, populate_ctx<ModelData>& ctx,
                                   const model_expression<Name, Partials...>& node, std::size_t) {
    std::size_t my_id = ctx.state_idx++;
    collect_transitions_partials(data, ctx, node.elements, my_id);
}

template <typename ModelData, typename Name, typename... Partials>
constexpr void collect_transitions(ModelData& data, populate_ctx<ModelData>& ctx,
                                   const state_expr<Name, Partials...>& node, std::size_t) {
    std::size_t my_id = ctx.state_idx++;
    collect_transitions_partials(data, ctx, node.elements, my_id);
}

template <typename ModelData, typename Name, typename... Partials>
constexpr void collect_transitions(ModelData& data, populate_ctx<ModelData>& ctx,
                                   const choice_expr<Name, Partials...>& node, std::size_t) {
    std::size_t my_id = ctx.state_idx++;
    collect_transitions_partials(data, ctx, node.elements, my_id);
}

template <typename ModelData, typename Name>
constexpr void collect_transitions(ModelData& /*data*/, populate_ctx<ModelData>& ctx,
                                   const final_expr<Name>& /*node*/, std::size_t) {
    // Final states have no children/transitions to collect
    ctx.state_idx++;
}

template <typename ModelData, typename... Actions>
constexpr void collect_transitions(ModelData&, populate_ctx<ModelData>&,
                                   const entry_expr<Actions...>&, std::size_t) {}

template <typename ModelData, typename... Actions>
constexpr void collect_transitions(ModelData&, populate_ctx<ModelData>&,
                                   const exit_expr<Actions...>&, std::size_t) {}

template <typename ModelData, typename... Actions>
constexpr void collect_transitions(ModelData&, populate_ctx<ModelData>&,
                                   const activity_expr<Actions...>&, std::size_t) {}

template <typename ModelData, typename... Events>
constexpr void collect_transitions(ModelData&, populate_ctx<ModelData>&,
                                   const defer_expr<Events...>&, std::size_t) {}

template <typename ModelData, typename Callable>
constexpr void collect_transitions(ModelData&, populate_ctx<ModelData>&,
                                   const after_expr<Callable>&, std::size_t) {}

template <typename ModelData, typename Callable>
constexpr void collect_transitions(ModelData&, populate_ctx<ModelData>&,
                                   const every_expr<Callable>&, std::size_t) {}

// Helper to find target/event in partials
template <typename Tuple, std::size_t I>
constexpr std::string_view get_target_path(const Tuple& t) {
    if constexpr (I >= std::tuple_size_v<Tuple>) return {};
    else {
        using Type = std::decay_t<decltype(get<I>(t))>;
        if constexpr (is_target<Type>::value) {
            return get<I>(t).path.view();
        } else {
            return get_target_path<Tuple, I+1>(t);
        }
    }
}

template <typename Tuple, std::size_t I>
constexpr std::string_view get_event_name(const Tuple& t) {
    if constexpr (I >= std::tuple_size_v<Tuple>) return {};
    else {
        using Type = std::decay_t<decltype(get<I>(t))>;
        if constexpr (is_on<Type>::value) {
            return get<I>(t).name.view();
        } else {
            return get_event_name<Tuple, I+1>(t);
        }
    }
}

template <typename Tuple, std::size_t I>
constexpr std::size_t get_guard_idx(const Tuple& t, std::size_t& current_idx) {
    if constexpr (I >= std::tuple_size_v<Tuple>) return invalid_index;
    else {
        using Type = std::decay_t<decltype(get<I>(t))>;
        if constexpr (is_guard<Type>::value) {
             return current_idx++;
        } else {
             return get_guard_idx<Tuple, I+1>(t, current_idx);
        }
    }
}

template <typename... Actions>
constexpr void get_effect_details(const effect_expr<Actions...>&, std::size_t& start, std::size_t& count, std::size_t& current_idx) {
    start = current_idx;
    count = sizeof...(Actions);
    current_idx += count;
}

template <typename Tuple, std::size_t I>
constexpr void get_effect_info(const Tuple& t, std::size_t& start, std::size_t& count, std::size_t& current_idx) {
    if constexpr (I < std::tuple_size_v<Tuple>) {
        using Type = std::decay_t<decltype(get<I>(t))>;
        if constexpr (is_effect<Type>::value) {
             if (start == invalid_index) start = current_idx;
             
             std::size_t dummy_start = 0;
             std::size_t this_count = 0;
             get_effect_details(get<I>(t), dummy_start, this_count, current_idx);
             count += this_count;
        } else {
             // Recurse for other types but don't consume effect indices
             // But other types don't consume indices anyway.
        }
        get_effect_info<Tuple, I+1>(t, start, count, current_idx);
    }
}

template <typename Tuple, std::size_t I>
constexpr void get_timer_info(const Tuple& t, timer_kind& kind, std::size_t& idx, std::size_t& current_idx) {
    if constexpr (I < std::tuple_size_v<Tuple>) {
        using Type = std::decay_t<decltype(get<I>(t))>;
        if constexpr (is_after<Type>::value) {
            kind = timer_kind::after;
            idx = current_idx++;
        } else if constexpr (is_every<Type>::value) {
            kind = timer_kind::every;
            idx = current_idx++;
        } else {
            get_timer_info<Tuple, I+1>(t, kind, idx, current_idx);
        }
    }
}

template <typename ModelData, typename... Partials>
constexpr void collect_transitions(ModelData& data, populate_ctx<ModelData>& ctx,
                                   const transition_expr<Partials...>& node, std::size_t current_state_id) {
    std::size_t id = ctx.transition_idx++;
    
    std::string_view target_path = get_target_path<decltype(node.elements), 0>(node.elements);
    std::string_view event_name = get_event_name<decltype(node.elements), 0>(node.elements);
    
    std::size_t target_id = invalid_index;
    if (!target_path.empty()) {
        target_id = resolve_target(data, target_path, current_state_id);
    }
    
    std::size_t event_id = invalid_index;
    if (!event_name.empty()) {
        event_id = ctx.event_idx++;
        std::size_t offset = ctx.string_cursor;
        ctx.append_string(data, event_name);
        data.events[event_id] = event_desc{
            .id = event_id,
            .name_offset = offset,
            .name_length = event_name.size()
        };
    }
    
    std::size_t guard = get_guard_idx<decltype(node.elements), 0>(node.elements, ctx.guard_idx);
    
    std::size_t effect_start = invalid_index;
    std::size_t effect_count = 0;
    get_effect_info<decltype(node.elements), 0>(node.elements, effect_start, effect_count, ctx.effect_idx);
    
    timer_kind t_kind = timer_kind::none;
    std::size_t t_idx = invalid_index;
    get_timer_info<decltype(node.elements), 0>(node.elements, t_kind, t_idx, ctx.timer_idx);

    data.transitions[id] = transition_desc{
        .id = id,
        .source_id = current_state_id,
        .target_id = target_id,
        .kind = transition_kind::external,
        .event_id = event_id,
        .guard_idx = guard,
        .effect_start = effect_start,
        .effect_count = effect_count,
        .timer_type = t_kind,
        .timer_idx = t_idx
    };
}


// Helper to process initial element recursively
template <typename ModelData, typename Tuple, std::size_t I>
constexpr void process_initial_elements(ModelData& data, populate_ctx<ModelData>& ctx, 
                                        const Tuple& t, std::size_t current_state_id, std::size_t& transition_id) {
    if constexpr (I < std::tuple_size_v<Tuple>) {
        using Type = std::decay_t<decltype(get<I>(t))>;
        if constexpr (is_transition<Type>::value) {
             std::size_t next_id = ctx.transition_idx;
             collect_transitions(data, ctx, get<I>(t), current_state_id);
             if (ctx.transition_idx > next_id) {
                 transition_id = next_id;
                 // Also ensure the transition has NO event_id if it was external
                 // Initial transitions are automatic.
                 data.transitions[transition_id].event_id = invalid_index;
             }
        }
        process_initial_elements<ModelData, Tuple, I+1>(data, ctx, t, current_state_id, transition_id);
    }
}


// Initial Expression Handler (Pass 2 - Transition Collection)
template <typename ModelData, typename... Partials>
constexpr void collect_transitions(ModelData& data, populate_ctx<ModelData>& ctx,
                                   const initial_expr<Partials...>& node, std::size_t current_state_id) {
    
    std::size_t transition_id = invalid_index;

    if constexpr (has_direct_target_check<decltype(node.elements), 0>()) {
        // Implicit transition from target(...)
        transition_id = ctx.transition_idx++;
        
        std::string_view target_path = get_target_path<decltype(node.elements), 0>(node.elements);
        std::size_t target_id = resolve_target(data, target_path, current_state_id);
        
        std::size_t effect_start = invalid_index;
        std::size_t effect_count = 0;
        get_effect_info<decltype(node.elements), 0>(node.elements, effect_start, effect_count, ctx.effect_idx);
        
        data.transitions[transition_id] = transition_desc{
            .id = transition_id,
            .source_id = current_state_id, 
            .target_id = target_id,
            .kind = transition_kind::local,
            .event_id = invalid_index,
            .effect_start = effect_start,
            .effect_count = effect_count
        };
        
        // Recurse for other things?
        collect_transitions_partials(data, ctx, node.elements, current_state_id);
        
    } else {
        // Explicit transition(s) inside initial
        process_initial_elements<ModelData, decltype(node.elements), 0>(data, ctx, node.elements, current_state_id, transition_id);
    }
    
    if (transition_id != invalid_index) {
        data.states[current_state_id].initial_transition_id = transition_id;
    }
}

// Main Normalize Function
template <auto Model>
consteval auto normalize() {
  constexpr model_counts counts = count_recursive(Model, 0);

  using ModelDataType = normalized_model_data<counts.states, counts.transitions,
                               counts.events, counts.timers, counts.deferred_entries, counts.string_size>;
  ModelDataType data{};
  
  populate_ctx<ModelDataType> ctx{};
  
  // Pass 1: States
  collect_states(data, ctx, Model, "", invalid_index);
  
  // Pass 2: Transitions
  ctx.state_idx = 0; 
  collect_transitions(data, ctx, Model, invalid_index);
  
  return data;
}

}  // namespace cthsm::detail
