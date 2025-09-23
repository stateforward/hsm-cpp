#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <unordered_map>
#include <queue>
#include <unordered_set>
#include <bitset>

namespace cthsm {

// Forward declarations
struct Event;
struct Context;
struct Instance;
class TaskProvider;

// Compile-time string hashing
constexpr uint32_t hash(std::string_view str) {
  uint32_t h = 5381;
  for (char c : str) {
    h = ((h << 5) + h) + static_cast<uint32_t>(c);
  }
  return h;
}

// Event class
struct Event {
  uint32_t hash_id;
  std::any data;
  
  Event() : hash_id(0) {}
  explicit Event(std::string_view n) : hash_id(hash(n)) {}
  
  template<typename T>
  Event(std::string_view n, T&& d) : hash_id(hash(n)), data(std::forward<T>(d)) {}
};

// Context for cancellation
struct Context {
  std::atomic<bool> cancelled{false};
  
  void cancel() { cancelled.store(true); }
  bool done() const { return cancelled.load(); }
  void wait() {}
  void reset() { cancelled.store(false); }
  
  // Deprecated aliases
  [[deprecated("Use cancel() instead")]] void set() { cancel(); }
  [[deprecated("Use done() instead")]] bool is_set() const { return done(); }
};

// TaskProvider interface
struct TaskProvider {
  virtual ~TaskProvider() = default;
  virtual void sleep_for(std::chrono::milliseconds duration) = 0;
};

// Default TaskProvider
class DefaultTaskProvider : public TaskProvider {
public:
  void sleep_for(std::chrono::milliseconds duration) override {
    std::this_thread::sleep_for(duration);
  }
};

// Instance base class
struct Instance {
  virtual ~Instance() = default;
  virtual Context& dispatch(Event event) = 0;
  virtual std::string_view state() const = 0;
  virtual TaskProvider& task_provider() = 0;
};

// Action types
template <typename T = Instance>
using Action = void (*)(Context&, T&, Event&);

template <typename T = Instance>
using Condition = bool (*)(Context&, T&, Event&);

template <typename D, typename T = Instance>
using TimeExpression = D (*)(Context&, T&, Event&);

// ============================================================================
// Compile-Time DSL Types
// ============================================================================

struct on_t {
  uint32_t event_hash;
  constexpr on_t(std::string_view e) : event_hash(hash(e)) {}
};

struct target_t {
  uint32_t state_hash;
  constexpr target_t(std::string_view s) : state_hash(hash(s)) {}
};

template <typename... Funcs>
struct entry_t {
  std::tuple<Funcs...> funcs;
  constexpr entry_t(Funcs... f) : funcs(f...) {}
};

template <typename... Funcs>
struct exit_t {
  std::tuple<Funcs...> funcs;
  constexpr exit_t(Funcs... f) : funcs(f...) {}
};

template <typename Func>
struct effect_t {
  Func func;
  constexpr effect_t(Func f) : func(f) {}
};

template <typename Func>
struct guard_t {
  Func func;
  constexpr guard_t(Func f) : func(f) {}
};

template <typename Func>
struct activity_t {
  Func func;
  constexpr activity_t(Func f) : func(f) {}
};

template <typename... Events>
struct defer_t {
  std::tuple<Events...> events;
  constexpr defer_t(Events... e) : events(e...) {}
};

template <typename D, typename Func>
struct after_t {
  Func duration_func;
  constexpr after_t(Func f) : duration_func(f) {}
};

template <typename D, typename Func>
struct every_t {
  Func duration_func;
  constexpr every_t(Func f) : duration_func(f) {}
};

template <typename... Elements>
struct transition_t {
  std::tuple<Elements...> elements;
  constexpr transition_t(Elements... e) : elements(e...) {}
};

template <typename... Elements>
struct initial_t {
  std::tuple<Elements...> elements;
  constexpr initial_t(Elements... e) : elements(e...) {}
};

template <typename... Transitions>
struct choice_t {
  uint32_t name_hash;
  std::tuple<Transitions...> transitions;
  constexpr choice_t(std::string_view n, Transitions... t) 
    : name_hash(hash(n)), transitions(t...) {}
};

struct final_t {
  uint32_t name_hash;
  constexpr final_t(std::string_view n) : name_hash(hash(n)) {}
};

template <typename... Children>
struct state_t {
  uint32_t name_hash;
  std::tuple<Children...> children;
  constexpr state_t(std::string_view n, Children... c) 
    : name_hash(hash(n)), children(c...) {}
};

// ============================================================================
// Compile-Time Model Type
// ============================================================================

template <typename... Elements>
struct model_t {
  uint32_t name_hash;
  std::tuple<Elements...> elements;
  
  constexpr model_t(std::string_view n, Elements... e) 
    : name_hash(hash(n)), elements(e...) {}
};

// ============================================================================
// DSL Functions
// ============================================================================

constexpr on_t on(std::string_view event) { return on_t{event}; }
constexpr target_t target(std::string_view state) { return target_t{state}; }

template <typename... Funcs>
constexpr auto entry(Funcs... funcs) { return entry_t<Funcs...>{funcs...}; }

template <typename... Funcs>
constexpr auto exit(Funcs... funcs) { return exit_t<Funcs...>{funcs...}; }

template <typename Func>
constexpr auto effect(Func func) { return effect_t<Func>{func}; }

template <typename Func>
constexpr auto guard(Func func) { return guard_t<Func>{func}; }

template <typename Func>
constexpr auto activity(Func func) { return activity_t<Func>{func}; }

template <typename... Events>
constexpr auto defer(Events... events) { return defer_t<Events...>{events...}; }

template <typename D, typename T = Instance>
constexpr auto after(TimeExpression<D, T> duration_func) {
  return after_t<D, TimeExpression<D, T>>{duration_func};
}

template <typename D, typename T = Instance>
constexpr auto every(TimeExpression<D, T> duration_func) {
  return every_t<D, TimeExpression<D, T>>{duration_func};
}

template <typename... Elements>
constexpr auto transition(Elements... elements) {
  return transition_t<Elements...>{elements...};
}

template <typename... Elements>
constexpr auto initial(Elements... elements) {
  return initial_t<Elements...>{elements...};
}

template <typename... Transitions>
constexpr auto choice(std::string_view name, Transitions... transitions) {
  return choice_t<Transitions...>{name, transitions...};
}

constexpr auto final(std::string_view name) { return final_t{name}; }

template <typename... Children>
constexpr auto state(std::string_view name, Children... children) {
  return state_t<Children...>{name, children...};
}

// ============================================================================
// Main define function - returns compile-time model
// ============================================================================

template <typename... Elements>
constexpr auto define(std::string_view name, Elements... elements) {
  return model_t<Elements...>{name, elements...};
}

// ============================================================================
// Compile-time path computation
// ============================================================================

constexpr uint32_t combine_hashes(uint32_t parent, uint32_t child) {
  return parent ^ (child + 0x9e3779b9 + (parent << 6) + (parent >> 2));
}

// ============================================================================
// Type traits for compile-time introspection
// ============================================================================

template <typename T>
struct is_entry : std::false_type {};

template <typename... Funcs>
struct is_entry<entry_t<Funcs...>> : std::true_type {};

template <typename T>
struct is_exit : std::false_type {};

template <typename... Funcs>
struct is_exit<exit_t<Funcs...>> : std::true_type {};

template <typename T>
struct is_defer : std::false_type {};

template <typename... Events>
struct is_defer<defer_t<Events...>> : std::true_type {};

template <typename T>
struct is_transition : std::false_type {};

template <typename... Elements>
struct is_transition<transition_t<Elements...>> : std::true_type {};

template <typename T>
struct is_state : std::false_type {};

template <typename... Children>
struct is_state<state_t<Children...>> : std::true_type {};

template <typename T>
struct is_choice : std::false_type {};

template <typename... Transitions>
struct is_choice<choice_t<Transitions...>> : std::true_type {};

template <typename T>
struct is_final : std::false_type {};

template <>
struct is_final<final_t> : std::true_type {};

template <typename T>
struct is_initial : std::false_type {};

template <typename... Elements>
struct is_initial<initial_t<Elements...>> : std::true_type {};

template <typename T>
struct is_activity : std::false_type {};

template <typename Func>
struct is_activity<activity_t<Func>> : std::true_type {};

template <typename T>
struct is_after : std::false_type {};

template <typename D, typename Func>
struct is_after<after_t<D, Func>> : std::true_type {};

template <typename T>
struct is_every : std::false_type {};

template <typename D, typename Func>
struct is_every<every_t<D, Func>> : std::true_type {};

// ============================================================================
// Compile-time state counting
// ============================================================================

template <typename... Elements>
struct count_states_impl;

template <typename... Elements>
struct count_states_impl<model_t<Elements...>> {
  template <typename T>
  static constexpr size_t count_element() {
    if constexpr (is_state<T>::value || is_final<T>::value || is_choice<T>::value) {
      return 1 + count_children<T>();
    } else {
      return 0;
    }
  }
  
  template <typename T>
  static constexpr size_t count_children() {
    if constexpr (is_state<T>::value) {
      size_t count = 0;
      std::apply([&count](auto... children) {
        ((count += count_element<decltype(children)>()), ...);
      }, T{}.children);
      return count;
    }
    return 0;
  }
  
  static constexpr size_t count() {
    size_t total = 1; // Count the root model
    std::apply([&total](auto... elems) {
      ((total += count_element<decltype(elems)>()), ...);
    }, model_t<Elements...>{"", Elements{}...}.elements);
    return total;
  }
  
  static constexpr size_t value = count();
};

// ============================================================================
// Compile-time transition analysis
// ============================================================================

template <typename... Elements>
struct transition_analyzer;

template <typename... Elements>
struct transition_analyzer<model_t<Elements...>> {
  // Count transitions at compile time
  template <typename Element>
  static constexpr size_t count_transitions() {
    if constexpr (is_transition<Element>::value) {
      return 1;
    } else if constexpr (is_state<Element>::value) {
      size_t count = 0;
      std::apply([&count](auto... children) {
        ((count += count_transitions<decltype(children)>()), ...);
      }, Element{}.children);
      return count;
    } else if constexpr (is_choice<Element>::value) {
      return std::tuple_size_v<decltype(Element{}.transitions)>;
    }
    return 0;
  }
  
  static constexpr size_t count() {
    size_t total = 0;
    std::apply([&total](auto... elems) {
      ((total += count_transitions<decltype(elems)>()), ...);
    }, model_t<Elements...>{"", Elements{}...}.elements);
    return total;
  }
  
  static constexpr size_t total_transitions = count();
};

// ============================================================================
// Compile-time state machine execution
// ============================================================================

template <typename Model, typename T = Instance>
class StateMachine : public T {
private:
  // Runtime state
  Context context_;
  std::shared_ptr<TaskProvider> task_provider_;
  uint32_t current_state_hash_ = 0;
  std::queue<Event> event_queue_;
  std::queue<Event> deferred_queue_;
  std::atomic<bool> completed_{false};
  
  // Activities and timers
  struct Activity {
    std::thread thread;
    std::shared_ptr<Context> signal;
  };
  std::unordered_map<uint32_t, Activity> activities_;
  
  struct Timer {
    std::thread thread;
    std::shared_ptr<Context> signal;
    bool is_repeating;
  };
  std::unordered_map<uint32_t, Timer> timers_;
  
  mutable std::mutex dispatch_mutex_;
  
  // The compile-time model
  Model model_;
  
  // Runtime structures built from compile-time model
  struct StateInfo {
    uint32_t hash;
    uint32_t parent_hash;
    std::function<void(Context&, T&, Event&)> on_entry;
    std::function<void(Context&, T&, Event&)> on_exit;
    std::vector<std::function<void(Context&, T&, Event&)>> activities;
    std::vector<uint32_t> deferred_event_hashes;
    bool is_final = false;
    bool is_choice = false;
  };
  std::unordered_map<uint32_t, StateInfo> states_;
  
  // O(1) deferred event lookup structures
  static constexpr size_t MAX_UNIQUE_EVENTS = 256;
  std::unordered_map<uint32_t, size_t> event_to_index_;
  size_t next_event_index_ = 0;
  using DeferredBitset = std::bitset<MAX_UNIQUE_EVENTS>;
  std::unordered_map<uint32_t, DeferredBitset> state_deferred_bitsets_;
  
  struct TransitionInfo {
    uint32_t source_hash;
    uint32_t target_hash;
    uint32_t event_hash;
    std::function<bool(Context&, T&, Event&)> guard;
    std::function<void(Context&, T&, Event&)> effect;
    bool is_internal = false;
    bool is_timer = false;
    bool is_repeating = false;
    std::function<std::chrono::milliseconds(Context&, T&, Event&)> timer_duration;
  };
  
  // Optimized transition lookup structures
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, TransitionInfo>> state_transitions_;
  std::unordered_map<uint32_t, std::vector<TransitionInfo>> state_choice_transitions_;
  std::unordered_map<uint32_t, std::vector<TransitionInfo>> state_timer_transitions_;

  // Build the runtime structures from compile-time model
  void build_model() {
    // Process model elements
    process_elements(model_.elements, model_.name_hash, 0);
    
    // Build hierarchical transition inheritance
    build_transition_inheritance();
    
    // Build O(1) deferred event lookup
    build_deferred_bitsets();
    
    // Execute initial transition
    execute_initial_transition();
  }
  
  // Process model elements recursively
  template <typename... Elements>
  void process_elements(const std::tuple<Elements...>& elements, uint32_t parent_hash, uint32_t parent_parent_hash) {
    std::apply([this, parent_hash, parent_parent_hash](const auto&... elem) {
      (process_element(elem, parent_hash, parent_parent_hash), ...);
    }, elements);
  }
  
  template <typename Element>
  void process_element(const Element& elem, uint32_t parent_hash, uint32_t parent_parent_hash) {
    using ElemType = std::decay_t<Element>;
    
    if constexpr (is_state<ElemType>::value) {
      process_state(elem, parent_hash);
    } else if constexpr (is_final<ElemType>::value) {
      process_final(elem, parent_hash);
    } else if constexpr (is_choice<ElemType>::value) {
      process_choice(elem, parent_hash);
    } else if constexpr (is_transition<ElemType>::value) {
      process_transition(elem, parent_hash, parent_parent_hash);
    } else if constexpr (is_initial<ElemType>::value) {
      process_initial(elem, parent_hash);
    }
  }
  
  // Process state
  template <typename... Children>
  void process_state(const state_t<Children...>& state, uint32_t parent_hash) {
    uint32_t state_hash = combine_hashes(parent_hash, state.name_hash);
    
    StateInfo& info = states_[state_hash];
    info.hash = state_hash;
    info.parent_hash = parent_hash;
    
    // Process state children
    process_state_children(state.children, state_hash, parent_hash, info);
  }
  
  template <typename... Children>
  void process_state_children(const std::tuple<Children...>& children, 
                             uint32_t state_hash, uint32_t parent_hash, StateInfo& info) {
    std::apply([this, state_hash, parent_hash, &info](const auto&... child) {
      (process_state_child(child, state_hash, parent_hash, info), ...);
    }, children);
  }
  
  template <typename Child>
  void process_state_child(const Child& child, uint32_t state_hash, uint32_t parent_hash, StateInfo& info) {
    using ChildType = std::decay_t<Child>;
    
    if constexpr (is_entry<ChildType>::value) {
      info.on_entry = [funcs = child.funcs](Context& ctx, T& inst, Event& evt) {
        std::apply([&ctx, &inst, &evt](auto... f) {
          (f(ctx, inst, evt), ...);
        }, funcs);
      };
    } else if constexpr (is_exit<ChildType>::value) {
      info.on_exit = [funcs = child.funcs](Context& ctx, T& inst, Event& evt) {
        std::apply([&ctx, &inst, &evt](auto... f) {
          (f(ctx, inst, evt), ...);
        }, funcs);
      };
    } else if constexpr (is_activity<ChildType>::value) {
      info.activities.push_back(child.func);
    } else if constexpr (is_defer<ChildType>::value) {
      std::apply([&info](auto... events) {
        ((info.deferred_event_hashes.push_back(hash(events))), ...);
      }, child.events);
    } else if constexpr (is_transition<ChildType>::value || is_state<ChildType>::value || 
                        is_final<ChildType>::value || is_choice<ChildType>::value) {
      process_element(child, state_hash, parent_hash);
    }
  }
  
  // Process final state
  void process_final(const final_t& final, uint32_t parent_hash) {
    uint32_t state_hash = combine_hashes(parent_hash, final.name_hash);
    
    StateInfo& info = states_[state_hash];
    info.hash = state_hash;
    info.parent_hash = parent_hash;
    info.is_final = true;
  }
  
  // Process choice state
  template <typename... Transitions>
  void process_choice(const choice_t<Transitions...>& choice, uint32_t parent_hash) {
    uint32_t choice_hash = combine_hashes(parent_hash, choice.name_hash);
    
    StateInfo& info = states_[choice_hash];
    info.hash = choice_hash;
    info.parent_hash = parent_hash;
    info.is_choice = true;
    
    // Process choice transitions
    std::apply([this, choice_hash, parent_hash](const auto&... trans) {
      (process_transition(trans, choice_hash, parent_hash), ...);
    }, choice.transitions);
  }
  
  // Process initial
  template <typename... Elements>
  void process_initial(const initial_t<Elements...>& init, uint32_t parent_hash) {
    TransitionInfo trans;
    trans.source_hash = parent_hash;
    trans.event_hash = hash("__initial__");
    
    std::apply([this, &trans, parent_hash](const auto&... elem) {
      (process_initial_element(elem, trans, parent_hash), ...);
    }, init.elements);
    
    // Store as special initial transition
    state_transitions_[parent_hash][trans.event_hash] = trans;
  }
  
  template <typename Element>
  void process_initial_element(const Element& elem, TransitionInfo& trans, uint32_t parent_hash) {
    if constexpr (std::is_same_v<std::decay_t<Element>, target_t>) {
      trans.target_hash = resolve_target_hash(elem.state_hash, parent_hash, parent_hash);
    }
  }
  
  // Process transition
  template <typename... Elements>
  void process_transition(const transition_t<Elements...>& transition, uint32_t source_hash, uint32_t parent_hash) {
    TransitionInfo trans;
    trans.source_hash = source_hash;
    
    std::apply([this, &trans, source_hash, parent_hash](const auto&... elem) {
      (process_transition_element(elem, trans, source_hash, parent_hash), ...);
    }, transition.elements);
    
    // If no target specified, it's an internal transition
    if (trans.target_hash == 0) {
      trans.is_internal = true;
      trans.target_hash = source_hash;
    }
    
    // Store transition
    if (trans.is_timer) {
      state_timer_transitions_[source_hash].push_back(trans);
    } else if (states_[source_hash].is_choice) {
      state_choice_transitions_[source_hash].push_back(trans);
    } else {
      state_transitions_[source_hash][trans.event_hash] = trans;
    }
  }
  
  template <typename Element>
  void process_transition_element(const Element& elem, TransitionInfo& trans, uint32_t source_hash, uint32_t parent_hash) {
    using ElemType = std::decay_t<Element>;
    
    if constexpr (std::is_same_v<ElemType, on_t>) {
      trans.event_hash = elem.event_hash;
    } else if constexpr (std::is_same_v<ElemType, target_t>) {
      trans.target_hash = resolve_target_hash(elem.state_hash, source_hash, parent_hash);
    } else if constexpr (std::is_same_v<ElemType, guard_t<decltype(elem.func)>>) {
      trans.guard = elem.func;
    } else if constexpr (std::is_same_v<ElemType, effect_t<decltype(elem.func)>>) {
      trans.effect = elem.func;
    } else if constexpr (is_after<ElemType>::value) {
      trans.is_timer = true;
      trans.is_repeating = false;
      trans.timer_duration = [func = elem.duration_func](Context& ctx, T& inst, Event& evt) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(func(ctx, inst, evt));
      };
      trans.event_hash = hash("__timer__");
    } else if constexpr (is_every<ElemType>::value) {
      trans.is_timer = true;
      trans.is_repeating = true;
      trans.timer_duration = [func = elem.duration_func](Context& ctx, T& inst, Event& evt) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(func(ctx, inst, evt));
      };
      trans.event_hash = hash("__timer__");
    }
  }
  
  // Resolve target hash based on string
  uint32_t resolve_target_hash(uint32_t target_name_hash, uint32_t source_hash, uint32_t parent_hash) {
    // For now, assume target is a sibling state
    return combine_hashes(parent_hash, target_name_hash);
  }
  
  
  // Build transition inheritance for hierarchical states
  void build_transition_inheritance() {
    // For each state, check if parent states have transitions this state doesn't
    for (auto& [state_hash, state_info] : states_) {
      if (state_info.parent_hash != 0) {
        inherit_transitions_from_parent(state_hash, state_info.parent_hash);
      }
    }
  }
  
  void inherit_transitions_from_parent(uint32_t child_hash, uint32_t parent_hash) {
    // Inherit regular transitions
    auto parent_trans_it = state_transitions_.find(parent_hash);
    if (parent_trans_it != state_transitions_.end()) {
      for (const auto& [event_hash, trans] : parent_trans_it->second) {
        // Only inherit if child doesn't have this event
        if (state_transitions_[child_hash].find(event_hash) == state_transitions_[child_hash].end()) {
          state_transitions_[child_hash][event_hash] = trans;
        }
      }
    }
    
    // Inherit timer transitions
    auto parent_timer_it = state_timer_transitions_.find(parent_hash);
    if (parent_timer_it != state_timer_transitions_.end()) {
      for (const auto& timer_trans : parent_timer_it->second) {
        // Check if child already has this timer
        bool has_timer = false;
        for (const auto& child_timer : state_timer_transitions_[child_hash]) {
          if (child_timer.event_hash == timer_trans.event_hash) {
            has_timer = true;
            break;
          }
        }
        if (!has_timer) {
          state_timer_transitions_[child_hash].push_back(timer_trans);
        }
      }
    }
    
    // Recursively inherit from grandparents
    auto parent_info_it = states_.find(parent_hash);
    if (parent_info_it != states_.end() && parent_info_it->second.parent_hash != 0) {
      inherit_transitions_from_parent(child_hash, parent_info_it->second.parent_hash);
    }
  }
  
  // Get or create event index for bitset operations
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
  
  // Build deferred event bitsets with hierarchical inheritance
  void build_deferred_bitsets() {
    // First pass: build bitsets for each state's own deferred events
    for (const auto& [state_hash, state_info] : states_) {
      auto& bitset = state_deferred_bitsets_[state_hash];
      
      // Set bits for this state's deferred events
      for (uint32_t deferred_hash : state_info.deferred_event_hashes) {
        size_t index = get_or_create_event_index(deferred_hash);
        bitset.set(index);
      }
    }
    
    // Second pass: inherit deferred events from parents
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
  
  // Execute initial transition
  void execute_initial_transition() {
    current_state_hash_ = model_.name_hash;
    
    // Find and execute initial transition
    auto trans_map_it = state_transitions_.find(current_state_hash_);
    if (trans_map_it != state_transitions_.end()) {
      auto init_trans_it = trans_map_it->second.find(hash("__initial__"));
      if (init_trans_it != trans_map_it->second.end()) {
        Event init_event("__initial__");
        execute_transition(init_trans_it->second, init_event);
      }
    }
  }
  
  // Execute a transition
  void execute_transition(const TransitionInfo& trans, Event& event) {
    // Check guard
    if (trans.guard && !trans.guard(context_, *this, event)) {
      return;
    }
    
    // Internal transition - just execute effect
    if (trans.is_internal) {
      if (trans.effect) {
        trans.effect(context_, *this, event);
      }
      return;
    }
    
    // External transition - exit current, effect, enter target
    exit_states_to_lca(current_state_hash_, trans.target_hash, event);
    
    if (trans.effect) {
      trans.effect(context_, *this, event);
    }
    
    enter_states_from_lca(trans.target_hash, current_state_hash_, event);
    
    current_state_hash_ = trans.target_hash;
    
    // Check if target is choice or final
    auto target_info_it = states_.find(trans.target_hash);
    if (target_info_it != states_.end()) {
      if (target_info_it->second.is_choice) {
        evaluate_choice_state(trans.target_hash, event);
      } else if (target_info_it->second.is_final) {
        completed_.store(true);
      }
    }
  }
  
  // Find LCA and exit states
  void exit_states_to_lca(uint32_t from_hash, uint32_t to_hash, Event& event) {
    // Find LCA
    uint32_t lca = find_lca(from_hash, to_hash);
    
    // Exit states from current up to (but not including) LCA
    uint32_t current = from_hash;
    while (current != lca && current != 0) {
      exit_state(current, event);
      
      auto state_it = states_.find(current);
      if (state_it != states_.end()) {
        current = state_it->second.parent_hash;
      } else {
        break;
      }
    }
  }
  
  // Enter states from LCA
  void enter_states_from_lca(uint32_t to_hash, uint32_t from_hash, Event& event) {
    // Find LCA
    uint32_t lca = find_lca(from_hash, to_hash);
    
    // Build path from LCA to target
    std::vector<uint32_t> entry_path;
    uint32_t current = to_hash;
    
    while (current != lca && current != 0) {
      entry_path.push_back(current);
      
      auto state_it = states_.find(current);
      if (state_it != states_.end()) {
        current = state_it->second.parent_hash;
      } else {
        break;
      }
    }
    
    // Enter states in reverse order (from LCA down to target)
    for (auto it = entry_path.rbegin(); it != entry_path.rend(); ++it) {
      enter_state(*it, event);
    }
  }
  
  // Find least common ancestor
  uint32_t find_lca(uint32_t state1, uint32_t state2) {
    // Build ancestor set for state1
    std::unordered_set<uint32_t> ancestors;
    uint32_t current = state1;
    
    while (current != 0) {
      ancestors.insert(current);
      auto state_it = states_.find(current);
      if (state_it != states_.end()) {
        current = state_it->second.parent_hash;
      } else {
        break;
      }
    }
    
    // Walk up from state2 until we find common ancestor
    current = state2;
    while (current != 0) {
      if (ancestors.count(current)) {
        return current;
      }
      auto state_it = states_.find(current);
      if (state_it != states_.end()) {
        current = state_it->second.parent_hash;
      } else {
        break;
      }
    }
    
    return model_.name_hash; // Root as fallback
  }
  
  // Enter a state
  void enter_state(uint32_t state_hash, Event& event) {
    auto state_it = states_.find(state_hash);
    if (state_it == states_.end()) return;
    
    auto& state_info = state_it->second;
    
    // Execute entry action
    if (state_info.on_entry) {
      state_info.on_entry(context_, *this, event);
    }
    
    // Start activities
    for (size_t i = 0; i < state_info.activities.size(); ++i) {
      auto activity_ctx = std::make_shared<Context>();
      auto activity_func = state_info.activities[i];
      
      std::thread activity_thread([this, activity_func, activity_ctx, event]() mutable {
        activity_func(*activity_ctx, *this, event);
      });
      
      uint32_t activity_key = combine_hashes(state_hash, static_cast<uint32_t>(i));
      activities_[activity_key] = Activity{std::move(activity_thread), activity_ctx};
    }
    
    // Start timers
    start_state_timers(state_hash, event);
  }
  
  // Exit a state
  void exit_state(uint32_t state_hash, Event& event) {
    auto state_it = states_.find(state_hash);
    if (state_it == states_.end()) return;
    
    auto& state_info = state_it->second;
    
    // Stop activities
    for (size_t i = 0; i < state_info.activities.size(); ++i) {
      uint32_t activity_key = combine_hashes(state_hash, static_cast<uint32_t>(i));
      auto activity_it = activities_.find(activity_key);
      if (activity_it != activities_.end()) {
        activity_it->second.signal->cancel();
        if (activity_it->second.thread.joinable()) {
          activity_it->second.thread.join();
        }
        activities_.erase(activity_it);
      }
    }
    
    // Stop timers
    stop_state_timers(state_hash);
    
    // Execute exit action
    if (state_info.on_exit) {
      state_info.on_exit(context_, *this, event);
    }
  }
  
  // Start timers for a state
  void start_state_timers(uint32_t state_hash, Event& event) {
    auto timer_trans_it = state_timer_transitions_.find(state_hash);
    if (timer_trans_it == state_timer_transitions_.end()) return;
    
    for (const auto& timer_trans : timer_trans_it->second) {
      if (!timer_trans.timer_duration) continue;
      
      auto duration = timer_trans.timer_duration(context_, *this, event);
      auto timer_ctx = std::make_shared<Context>();
      
      std::thread timer_thread([this, timer_trans, timer_ctx, duration]() {
        task_provider_->sleep_for(duration);
        
        if (!timer_ctx->done()) {
          Event timer_event("__timer__");
          dispatch(std::move(timer_event));
          
          // Handle repeating timers
          if (timer_trans.is_repeating && !timer_ctx->done()) {
            // Would need more logic for proper repeating
          }
        }
      });
      
      uint32_t timer_key = combine_hashes(state_hash, timer_trans.event_hash);
      timers_[timer_key] = Timer{std::move(timer_thread), timer_ctx, timer_trans.is_repeating};
    }
  }
  
  // Stop timers for a state
  void stop_state_timers(uint32_t state_hash) {
    // Find all timers for this state
    auto timer_trans_it = state_timer_transitions_.find(state_hash);
    if (timer_trans_it == state_timer_transitions_.end()) return;
    
    for (const auto& timer_trans : timer_trans_it->second) {
      uint32_t timer_key = combine_hashes(state_hash, timer_trans.event_hash);
      auto timer_it = timers_.find(timer_key);
      if (timer_it != timers_.end()) {
        timer_it->second.signal->cancel();
        if (timer_it->second.thread.joinable()) {
          timer_it->second.thread.join();
        }
        timers_.erase(timer_it);
      }
    }
  }
  
  // Evaluate choice state
  void evaluate_choice_state(uint32_t choice_hash, Event& event) {
    auto choice_trans_it = state_choice_transitions_.find(choice_hash);
    if (choice_trans_it == state_choice_transitions_.end()) return;
    
    // Evaluate transitions in order
    for (const auto& trans : choice_trans_it->second) {
      if (!trans.guard || trans.guard(context_, *this, event)) {
        execute_transition(trans, event);
        return;
      }
    }
  }
  
  // Process event queue
  void process_queue() {
    while (!event_queue_.empty()) {
      Event event = std::move(event_queue_.front());
      event_queue_.pop();
      
      // Check if deferred
      if (is_event_deferred(event.hash_id)) {
        deferred_queue_.push(std::move(event));
        continue;
      }
      
      // Find transition
      auto trans_map_it = state_transitions_.find(current_state_hash_);
      if (trans_map_it != state_transitions_.end()) {
        auto trans_it = trans_map_it->second.find(event.hash_id);
        if (trans_it != trans_map_it->second.end()) {
          execute_transition(trans_it->second, event);
          reprocess_deferred_events();
        }
      }
    }
  }
  
  // Check if event is deferred - O(1) using bitsets
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
  
  // Reprocess deferred events
  void reprocess_deferred_events() {
    std::queue<Event> temp_queue;
    
    while (!deferred_queue_.empty()) {
      Event event = std::move(deferred_queue_.front());
      deferred_queue_.pop();
      
      if (!is_event_deferred(event.hash_id)) {
        event_queue_.push(std::move(event));
      } else {
        temp_queue.push(std::move(event));
      }
    }
    
    deferred_queue_ = std::move(temp_queue);
  }

public:
  template<typename... Args>
  StateMachine(const Model& m, std::shared_ptr<TaskProvider> tp = nullptr, Args&&... args)
    : T(std::forward<Args>(args)...),
      task_provider_(tp ? std::move(tp) : std::make_shared<DefaultTaskProvider>()),
      model_(m) {
    build_model();
  }
  
  Context& dispatch(Event event) override {
    std::lock_guard<std::mutex> lock(dispatch_mutex_);
    event_queue_.push(std::move(event));
    process_queue();
    return context_;
  }
  
  std::string_view state() const override {
    // For now, return hash as string
    static thread_local std::string state_str;
    state_str = std::to_string(current_state_hash_);
    return state_str;
  }
  
  TaskProvider& task_provider() override {
    return *task_provider_;
  }
  
  bool is_completed() const {
    return completed_.load();
  }
};

// ============================================================================
// Factory function to create state machine from compile-time model
// ============================================================================

template <typename Model, typename T = Instance, typename... Args>
auto create(const Model& model, std::shared_ptr<TaskProvider> tp = nullptr, Args&&... args) {
  return StateMachine<Model, T>(model, tp, std::forward<Args>(args)...);
}

// Convenience function for compatibility
template <typename Model, typename T = Instance, typename... Args>
auto compile(const Model& model, std::shared_ptr<TaskProvider> tp = nullptr, Args&&... args) {
  return create<Model, T>(model, tp, std::forward<Args>(args)...);
}

} // namespace cthsm