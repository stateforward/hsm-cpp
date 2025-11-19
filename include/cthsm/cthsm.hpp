#pragma once

#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <atomic>
#include <vector>
#include <optional>
#include <chrono>
#include <array>

#include "cthsm/detail/expressions.hpp"
#include "cthsm/detail/normalize.hpp"
#include "cthsm/detail/behaviors.hpp"
#include "cthsm/detail/structural_tuple.hpp"
#include "cthsm/detail/tables.hpp"

namespace cthsm {

struct Context {
  constexpr Context() = default;
  ~Context() = default;

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;

  void set() {
    flag_.store(true, std::memory_order_release);
  }

  [[nodiscard]] bool is_set() const { return flag_.load(std::memory_order_acquire); }

  void wait() {
    while (!flag_.load(std::memory_order_acquire)) {
        // Spin/yield hint could go here
    }
  }

  void reset() { flag_.store(false, std::memory_order_release); }

 private:
  std::atomic_bool flag_{false};
};

// Default sequential provider (no threading dependencies)
struct SequentialTaskProvider {
  struct TaskHandle {
    void join() {} // Already finished
    bool joinable() const { return false; }
  };

  template <typename F>
  TaskHandle create_task(F&& f, const char* /*name*/ = nullptr, size_t /*stack*/ = 0, int /*prio*/ = 0) {
    // execute immediately (sequential)
    f();
    return TaskHandle{};
  }

  void sleep_for(std::chrono::milliseconds /*duration*/) {
    // No-op in sequential default
  }
};

struct Event {
  constexpr Event() noexcept = default;
  constexpr explicit Event(std::string_view name) noexcept : name_(name) {}

  [[nodiscard]] constexpr std::string_view name() const noexcept {
    return name_;
  }

 private:
  std::string_view name_{};
};

struct Instance {
  constexpr Instance() = default;
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;
  Instance(Instance&&) = delete;
  Instance& operator=(Instance&&) = delete;
  virtual ~Instance() = default;
};

namespace detail {

template <typename... Ts>
[[nodiscard]] constexpr auto make_node_tuple(Ts&&... values) {
  return detail::make_structural_tuple(std::forward<Ts>(values)...);
}

}  // namespace detail

template <typename Name, typename... Partials>
[[nodiscard]] constexpr auto state(Name name, Partials&&... partials) {
  using name_type = std::decay_t<Name>;
  return detail::state_expr<name_type, std::decay_t<Partials>...>{
      name_type{name},
      detail::make_node_tuple(std::forward<Partials>(partials)...)};
}

template <std::size_t N, typename... Partials>
[[nodiscard]] constexpr auto state(const char (&name)[N],
                                   Partials&&... partials) {
  return state(detail::make_fixed_string(name),
               std::forward<Partials>(partials)...);
}

template <typename... Partials>
[[nodiscard]] constexpr auto transition(Partials&&... partials) {
  return detail::transition_expr<std::decay_t<Partials>...>{
      detail::make_node_tuple(std::forward<Partials>(partials)...)};
}

template <typename... Partials>
[[nodiscard]] constexpr auto initial(Partials&&... partials) {
  return detail::initial_expr<std::decay_t<Partials>...>{
      detail::make_node_tuple(std::forward<Partials>(partials)...)};
}

template <typename Name, typename... Partials>
[[nodiscard]] constexpr auto choice(Name name, Partials&&... partials) {
  using name_type = std::decay_t<Name>;
  return detail::choice_expr<name_type, std::decay_t<Partials>...>{
      name_type{name},
      detail::make_node_tuple(std::forward<Partials>(partials)...)};
}

template <std::size_t N, typename... Partials>
[[nodiscard]] constexpr auto choice(const char (&name)[N],
                                    Partials&&... partials) {
  return choice(detail::make_fixed_string(name),
                std::forward<Partials>(partials)...);
}

template <typename Name>
[[nodiscard]] constexpr auto final(Name name) {
  using name_type = std::decay_t<Name>;
  return detail::final_expr<name_type>{name_type{name}};
}

template <std::size_t N>
[[nodiscard]] constexpr auto final(const char (&name)[N]) {
  return final(detail::make_fixed_string(name));
}

template <typename... Actions>
[[nodiscard]] constexpr auto entry(Actions&&... actions) {
  return detail::entry_expr<std::decay_t<Actions>...>{
      detail::make_node_tuple(std::forward<Actions>(actions)...)};
}

template <typename... Actions>
[[nodiscard]] constexpr auto exit(Actions&&... actions) {
  return detail::exit_expr<std::decay_t<Actions>...>{
      detail::make_node_tuple(std::forward<Actions>(actions)...)};
}

template <typename... Actions>
[[nodiscard]] constexpr auto effect(Actions&&... actions) {
  return detail::effect_expr<std::decay_t<Actions>...>{
      detail::make_node_tuple(std::forward<Actions>(actions)...)};
}

template <typename... Actions>
[[nodiscard]] constexpr auto activity(Actions&&... actions) {
  return detail::activity_expr<std::decay_t<Actions>...>{
      detail::make_node_tuple(std::forward<Actions>(actions)...)};
}

template <typename Callable>
[[nodiscard]] constexpr auto guard(Callable&& callable) {
  return detail::guard_expr<std::decay_t<Callable>>{
      std::forward<Callable>(callable)};
}

template <typename Name>
[[nodiscard]] constexpr auto on(Name name) {
  using event_type = std::decay_t<Name>;
  return detail::on_expr<event_type>{event_type{name}};
}

template <std::size_t N>
[[nodiscard]] constexpr auto on(const char (&event)[N]) {
  return on(detail::make_fixed_string(event));
}

template <typename Path>
[[nodiscard]] constexpr auto source(Path path) {
  using path_type = std::decay_t<Path>;
  return detail::source_expr<path_type>{path_type{path}};
}

template <std::size_t N>
[[nodiscard]] constexpr auto source(const char (&path)[N]) {
  return source(detail::make_fixed_string(path));
}

template <typename Path>
[[nodiscard]] constexpr auto target(Path path) {
  using path_type = std::decay_t<Path>;
  return detail::target_expr<path_type>{path_type{path}};
}

template <std::size_t N>
[[nodiscard]] constexpr auto target(const char (&path)[N]) {
  return target(detail::make_fixed_string(path));
}

template <typename... Events>
[[nodiscard]] constexpr auto defer(Events... events) {
  return detail::defer_expr<std::decay_t<Events>...>{
      detail::make_structural_tuple(events...)};
}

template <std::size_t... N>
[[nodiscard]] constexpr auto defer(const char (&... events)[N]) {
  return defer(detail::make_fixed_string(events)...);
}

template <typename Callable>
[[nodiscard]] constexpr auto after(Callable&& callable) {
  return detail::after_expr<std::decay_t<Callable>>{
      std::forward<Callable>(callable)};
}

template <typename Callable>
[[nodiscard]] constexpr auto every(Callable&& callable) {
  return detail::every_expr<std::decay_t<Callable>>{
      std::forward<Callable>(callable)};
}

template <typename Name, typename... Partials>
[[nodiscard]] constexpr auto define(Name name,
                                    Partials&&... partials) noexcept {
  using name_type = std::decay_t<Name>;
  using expression_type =
      detail::model_expression<name_type, std::decay_t<Partials>...>;
  return expression_type{
      name_type{name},
      detail::make_structural_tuple(std::forward<Partials>(partials)...)};
}

template <std::size_t N, typename... Partials>
[[nodiscard]] constexpr auto define(const char (&name)[N],
                                    Partials&&... partials) noexcept {
  return define(detail::make_fixed_string(name),
                std::forward<Partials>(partials)...);
}

template <auto Model, typename InstanceType = Instance, typename TaskProvider = SequentialTaskProvider>
struct compile {
  static constexpr auto model_ = Model;
  using instance_type = InstanceType;
  using TaskProviderType = TaskProvider;

  // 1. Model Normalization & Tables
  static constexpr auto normalized_model = detail::normalize<model_>();
  static constexpr auto tables = detail::build_tables(normalized_model);

  // 2. Behavior Extraction
  static constexpr auto entry_tuple = detail::extract_entries(model_);
  static constexpr auto exit_tuple = detail::extract_exits(model_);
  static constexpr auto activity_tuple = detail::extract_activities(model_);
  static constexpr auto guard_tuple = detail::extract_guards(model_);
  static constexpr auto effect_tuple = detail::extract_effects(model_);
  static constexpr auto timer_tuple = detail::extract_timers(model_);

  // 3. Activity Tracking Definitions
  static constexpr std::size_t total_activity_count = std::tuple_size_v<decltype(activity_tuple)>;
  
  struct ActiveTask {
    typename TaskProvider::TaskHandle task;
    Context* ctx;
  };

  // 4. Thunk Types & Functions
  using behavior_fn = void (*)(Context&, instance_type&, const Event&);
  using guard_fn = bool (*)(Context&, instance_type&, const Event&);
  using timer_fn = void (*)(Context&, instance_type&, const Event&, std::size_t); 

  template <typename F>
  static constexpr auto invoke(F&& f, Context& c, instance_type& i, const Event& e) -> decltype(auto) {
      if constexpr (std::is_invocable_v<F, Context&, instance_type&, const Event&>) {
          return f(c, i, e);
      } else if constexpr (std::is_invocable_v<F, instance_type&, const Event&>) {
          return f(i, e);
      } else if constexpr (std::is_invocable_v<F, instance_type&>) {
          return f(i);
      } else if constexpr (std::is_invocable_v<F>) {
          return f();
      } else {
          return f; 
      }
  }

  template <std::size_t I> static void entry_thunk(Context& c, instance_type& i, const Event& e) { invoke(std::get<I>(entry_tuple), c, i, e); }
  template <std::size_t I> static void exit_thunk(Context& c, instance_type& i, const Event& e) { invoke(std::get<I>(exit_tuple), c, i, e); }
  template <std::size_t I> static void activity_thunk(Context& c, instance_type& i, const Event& e) { invoke(std::get<I>(activity_tuple), c, i, e); }
  template <std::size_t I> static void effect_thunk(Context& c, instance_type& i, const Event& e) { invoke(std::get<I>(effect_tuple), c, i, e); }
  template <std::size_t I> static bool guard_thunk(Context& c, instance_type& i, const Event& e) { return invoke(std::get<I>(guard_tuple), c, i, e); }
  template <std::size_t I> static void timer_thunk(Context& c, instance_type& i, const Event& e, std::size_t id) { 
      auto d = invoke(std::get<I>(timer_tuple), c, i, e);
      if constexpr (requires { i.schedule(id, d); }) i.schedule(id, d);
  }

  template <std::size_t... Is> static constexpr auto make_entry_table(std::index_sequence<Is...>) { return std::array<behavior_fn, sizeof...(Is)>{ &entry_thunk<Is>... }; }
  template <std::size_t... Is> static constexpr auto make_exit_table(std::index_sequence<Is...>) { return std::array<behavior_fn, sizeof...(Is)>{ &exit_thunk<Is>... }; }
  template <std::size_t... Is> static constexpr auto make_activity_table(std::index_sequence<Is...>) { return std::array<behavior_fn, sizeof...(Is)>{ &activity_thunk<Is>... }; }
  template <std::size_t... Is> static constexpr auto make_effect_table(std::index_sequence<Is...>) { return std::array<behavior_fn, sizeof...(Is)>{ &effect_thunk<Is>... }; }
  template <std::size_t... Is> static constexpr auto make_guard_table(std::index_sequence<Is...>) { return std::array<guard_fn, sizeof...(Is)>{ &guard_thunk<Is>... }; }
  template <std::size_t... Is> static constexpr auto make_timer_table(std::index_sequence<Is...>) { return std::array<timer_fn, sizeof...(Is)>{ &timer_thunk<Is>... }; }

  static constexpr auto entry_table = make_entry_table(std::make_index_sequence<std::tuple_size_v<decltype(entry_tuple)>>{});
  static constexpr auto exit_table = make_exit_table(std::make_index_sequence<std::tuple_size_v<decltype(exit_tuple)>>{});
  static constexpr auto activity_table = make_activity_table(std::make_index_sequence<std::tuple_size_v<decltype(activity_tuple)>>{});
  static constexpr auto effect_table = make_effect_table(std::make_index_sequence<std::tuple_size_v<decltype(effect_tuple)>>{});
  static constexpr auto guard_table = make_guard_table(std::make_index_sequence<std::tuple_size_v<decltype(guard_tuple)>>{});
  static constexpr auto timer_table = make_timer_table(std::make_index_sequence<std::tuple_size_v<decltype(timer_tuple)>>{});

  // 5. Data Members
  TaskProvider task_provider_;
  
  static constexpr std::size_t max_deferred = 16;
  std::array<std::size_t, max_deferred> deferred_queue_;
  std::size_t deferred_count_;
  
  std::array<std::optional<ActiveTask>, total_activity_count> active_tasks_;
  std::array<Context, total_activity_count> activity_contexts_;
  
  std::size_t current_state_id_;

  // 6. Constructor
  constexpr compile(TaskProvider tp = {}) noexcept 
    : task_provider_(std::move(tp)),
      deferred_queue_{},
      deferred_count_{0},
      active_tasks_{},
      activity_contexts_{},
      current_state_id_(detail::invalid_index) {}

  // 7. Accessor
  [[nodiscard]] constexpr std::string_view state() const noexcept {
    if (current_state_id_ == detail::invalid_index) return "";
    return normalized_model.get_state_name(current_state_id_);
  }

  // 8. Public Methods
  constexpr void start(instance_type& instance) {
      // Reset
      deferred_count_ = 0;
      current_state_id_ = 0; // Root
      
      // Enter root
      Context ctx{};
      Event e{"init"};
      enter_state(ctx, instance, e, 0);
      
      resolve_initial(ctx, instance, e, 0);
      resolve_completion(ctx, instance);
  }

  constexpr void dispatch(instance_type& instance, std::string_view event_name) noexcept {
     Context ctx{};
     Event e{event_name};
     
     std::size_t event_id = tables.get_event_id(event_name);
     if (event_id == detail::invalid_index) return;

     if (is_deferred(current_state_id_, event_id)) {
         if (deferred_count_ < max_deferred) {
             deferred_queue_[deferred_count_++] = event_id;
         }
         return;
     }
     
     bool handled = dispatch_event_impl(ctx, instance, e, event_id);
     
     if (handled) {
         process_deferred(instance);
     }
  }

  constexpr void handle_timer(instance_type& instance, std::size_t timer_idx) {
      if (timer_idx < tables.timer_transition_map.size()) {
           std::size_t t_id = tables.timer_transition_map[timer_idx];
           if (t_id != detail::invalid_index) {
               const auto& t = normalized_model.transitions[t_id];
               bool active = false;
               std::size_t curr = current_state_id_;
               while(curr != detail::invalid_index) {
                   if (curr == t.source_id) { active = true; break; }
                   curr = normalized_model.states[curr].parent_id;
               }
               
               if (active) {
                   Context ctx{};
                   Event e{""}; 
                   execute_transition(ctx, instance, e, t);
               }
           }
      }
  }

 private:
  constexpr bool is_deferred(std::size_t state, std::size_t event_id) const {
      std::string_view event_name = normalized_model.get_event_name(event_id);
      std::size_t curr = state;
      while (curr != detail::invalid_index) {
          const auto& st = normalized_model.states[curr];
          for (std::size_t i = 0; i < st.defer_count; ++i) {
               std::size_t def_id = normalized_model.deferred_events[st.defer_start + i];
               if (normalized_model.get_event_name(def_id) == event_name) return true;
          }
          curr = st.parent_id;
      }
      return false;
  }

  constexpr bool dispatch_event_impl(Context& ctx, instance_type& instance, const Event& e, std::size_t event_id) {
      if (current_state_id_ == detail::invalid_index) return false;
      
      std::size_t curr = current_state_id_;
      std::size_t t_id = tables.transition_table[curr][event_id];
      
      while (t_id != detail::invalid_index) {
          const auto& t = normalized_model.transitions[t_id];
          
          bool guard_passed = true;
          if (t.guard_idx != detail::invalid_index) {
               if (t.guard_idx < guard_table.size()) {
                   guard_passed = guard_table[t.guard_idx](ctx, instance, e);
               }
          }
          
          if (guard_passed) {
              execute_transition(ctx, instance, e, t);
              return true;
          }
          
          t_id = tables.next_candidate[t_id];
      }
      return false;
  }

  constexpr void execute_transition(Context& ctx, instance_type& instance, const Event& e, const auto& t) {
       if (t.target_id != detail::invalid_index) {
            std::size_t target = t.target_id;
            std::size_t old_state = current_state_id_;
            
            exit_to_lca(ctx, instance, e, old_state, target);
            
            if (t.effect_start != detail::invalid_index) {
                for (std::size_t i = 0; i < t.effect_count; ++i) {
                    effect_table[t.effect_start + i](ctx, instance, e);
                }
            }
            
            enter_from_lca(ctx, instance, e, old_state, target);
            current_state_id_ = target;
            
            resolve_initial(ctx, instance, e, target);
            resolve_completion(ctx, instance);
       } else {
            if (t.effect_start != detail::invalid_index) {
                for (std::size_t i = 0; i < t.effect_count; ++i) {
                    effect_table[t.effect_start + i](ctx, instance, e);
                }
            }
       }
  }

  constexpr void exit_to_lca(Context& ctx, instance_type& instance, const Event& e, std::size_t source, std::size_t target) {
      std::array<std::size_t, 16> source_path;
      std::size_t source_len = 0;
      for (std::size_t s = source; s != detail::invalid_index; s = normalized_model.states[s].parent_id) {
          source_path[source_len++] = s;
      }
      
      std::array<std::size_t, 16> target_path;
      std::size_t target_len = 0;
      for (std::size_t s = target; s != detail::invalid_index; s = normalized_model.states[s].parent_id) {
          target_path[target_len++] = s;
      }
      
      std::size_t lca = detail::invalid_index;
      int i = static_cast<int>(source_len) - 1;
      int j = static_cast<int>(target_len) - 1;
      while (i >= 0 && j >= 0 && source_path[static_cast<std::size_t>(i)] == target_path[static_cast<std::size_t>(j)]) {
          lca = source_path[static_cast<std::size_t>(i)];
          i--;
          j--;
      }
      
      for (std::size_t s = source; s != lca && s != detail::invalid_index; s = normalized_model.states[s].parent_id) {
          exit_state(ctx, instance, e, s);
      }
  }

  constexpr void enter_from_lca(Context& ctx, instance_type& instance, const Event& e, std::size_t source, std::size_t target) {
      std::array<std::size_t, 16> source_path;
      std::size_t source_len = 0;
      for (std::size_t s = source; s != detail::invalid_index; s = normalized_model.states[s].parent_id) {
          source_path[source_len++] = s;
      }
      
      std::array<std::size_t, 16> target_path;
      std::size_t target_len = 0;
      for (std::size_t s = target; s != detail::invalid_index; s = normalized_model.states[s].parent_id) {
          target_path[target_len++] = s;
      }
      
      int i = static_cast<int>(source_len) - 1;
      int j = static_cast<int>(target_len) - 1;
      while (i >= 0 && j >= 0 && source_path[static_cast<std::size_t>(i)] == target_path[static_cast<std::size_t>(j)]) {
          i--;
          j--;
      }
      
      for (; j >= 0; j--) {
          enter_state(ctx, instance, e, target_path[static_cast<std::size_t>(j)]);
      }
  }

  constexpr void exit_state(Context& ctx, instance_type& instance, const Event& e, std::size_t s_id) {
      const auto& s = normalized_model.states[s_id];
      if (s.exit_start != detail::invalid_index) {
          for (std::size_t i = 0; i < s.exit_count; ++i) {
              exit_table[s.exit_start + i](ctx, instance, e);
          }
      }
      auto range = tables.state_timer_ranges[s_id];
      for(std::size_t i=0; i<range.count; ++i) {
          auto& timer = tables.state_timer_list[range.start + i];
          if constexpr (requires { instance.cancel_timer(timer.timer_idx); }) {
              instance.cancel_timer(timer.timer_idx);
          }
      }
      if (s.activity_start != detail::invalid_index) {
          for (std::size_t i = 0; i < s.activity_count; ++i) {
             std::size_t idx = s.activity_start + i;
             if (idx < active_tasks_.size() && active_tasks_[idx].has_value()) {
                 active_tasks_[idx]->ctx->set();
                 if (active_tasks_[idx]->task.joinable()) {
                     active_tasks_[idx]->task.join();
                 }
                 active_tasks_[idx].reset();
             }
          }
      }
  }

  constexpr void enter_state(Context& ctx, instance_type& instance, const Event& e, std::size_t s_id) {
      const auto& s = normalized_model.states[s_id];
      if (s.entry_start != detail::invalid_index) {
          for (std::size_t i = 0; i < s.entry_count; ++i) {
              entry_table[s.entry_start + i](ctx, instance, e);
          }
      }
      if (s.activity_start != detail::invalid_index) {
          for (std::size_t i = 0; i < s.activity_count; ++i) {
              std::size_t idx = s.activity_start + i;
              if (idx < active_tasks_.size()) {
                  activity_contexts_[idx].reset();
                  Context* activity_ctx = &activity_contexts_[idx];
                  
                  auto task = task_provider_.create_task([idx, &instance, e, activity_ctx]() {
                      activity_table[idx](*activity_ctx, instance, e);
                  }, "activity", 0, 0);
                  
                  active_tasks_[idx] = ActiveTask{std::move(task), activity_ctx};
              }
          }
      }
      auto range = tables.state_timer_ranges[s_id];
      for(std::size_t i=0; i<range.count; ++i) {
          auto& timer = tables.state_timer_list[range.start + i];
          if (timer.timer_idx < timer_table.size()) {
             timer_table[timer.timer_idx](ctx, instance, e, timer.timer_idx);
          }
      }
  }

  constexpr void resolve_initial(Context& ctx, instance_type& instance, const Event& e, std::size_t current) {
      std::size_t init = normalized_model.states[current].initial_transition_id;
      while (init != detail::invalid_index) {
          const auto& t = normalized_model.transitions[init];
          if (t.effect_start != detail::invalid_index) {
              for (std::size_t i = 0; i < t.effect_count; ++i) {
                  effect_table[t.effect_start + i](ctx, instance, e);
              }
          }
          if (t.target_id != detail::invalid_index) {
              enter_from_lca(ctx, instance, e, current, t.target_id);
              current = t.target_id;
              current_state_id_ = current;
              init = normalized_model.states[current].initial_transition_id;
          } else {
              break;
          }
      }
  }
  
  constexpr void resolve_completion(Context& ctx, instance_type& instance) {
      if (current_state_id_ == detail::invalid_index) return;
      
      const auto& range = tables.completion_transitions_ranges[current_state_id_];
      if (range.count == 0) return;
      
      for (std::size_t i = 0; i < range.count; ++i) {
           std::size_t t_id = tables.completion_transitions_list[range.start + i];
           const auto& t = normalized_model.transitions[t_id];
           
           bool guard_passed = true;
           if (t.guard_idx != detail::invalid_index) {
               if (t.guard_idx < guard_table.size()) {
                   Event empty{""};
                   guard_passed = guard_table[t.guard_idx](ctx, instance, empty);
               }
           }
           
           if (guard_passed) {
               Event empty{""};
               execute_transition(ctx, instance, empty, t);
               return; 
           }
      }
  }

  constexpr void process_deferred(instance_type& instance) {
      std::size_t count = deferred_count_;
      if (count == 0) return;
      
      std::array<std::size_t, max_deferred> current_queue = deferred_queue_;
      deferred_count_ = 0;
      
      for (std::size_t i = 0; i < count; ++i) {
          std::size_t evt_id = current_queue[i];
          if (is_deferred(current_state_id_, evt_id)) {
              if (deferred_count_ < max_deferred) {
                  deferred_queue_[deferred_count_++] = evt_id;
              }
          } else {
              std::string_view name = normalized_model.get_event_name(evt_id);
              dispatch(instance, name);
          }
      }
  }
};

}  // namespace cthsm
