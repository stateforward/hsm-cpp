#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "cthsm/detail/behaviors.hpp"
#include "cthsm/detail/expressions.hpp"
#include "cthsm/detail/normalize.hpp"
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

  void set() { flag_.store(true, std::memory_order_release); }

  [[nodiscard]] bool is_set() const {
    return flag_.load(std::memory_order_acquire);
  }

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
    void join() {}  // Already finished
    bool joinable() const { return false; }
  };

  template <typename F>
  TaskHandle create_task(F&& f, const char* /*name*/ = nullptr,
                         size_t /*stack*/ = 0, int /*prio*/ = 0) {
    // execute immediately (sequential)
    f();
    return TaskHandle{};
  }

  void sleep_for(std::chrono::milliseconds /*duration*/, Context* /*ctx*/ = nullptr) {
    // No-op in sequential default
  }
};

namespace detail {

template <typename T>
struct is_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

template <typename T>
inline constexpr bool is_duration_v = is_duration<T>::value;

template <typename T>
consteval std::string_view type_name() {
#if defined(__clang__)
  std::string_view name = __PRETTY_FUNCTION__;
  auto start = name.find("[T = ");
  if (start == std::string_view::npos) return "UNKNOWN";
  start += 5;
  auto end = name.find_last_of(']');
  return name.substr(start, end - start);
#elif defined(__GNUC__)
  std::string_view name = __PRETTY_FUNCTION__;
  auto start = name.find("[with T = ");
  if (start == std::string_view::npos) {
    start = name.find("[T = ");
    if (start == std::string_view::npos) return "UNKNOWN";
    start += 5;
  } else {
    start += 10;
  }
  auto end = name.find_last_of(']');
  return name.substr(start, end - start);
#else
  return "UNKNOWN";
#endif
}

// Traits for extracting event type from handler
template <typename T>
struct extract_event_type {
  using type = void;
};

template <typename Tuple>
struct get_event_from_args {
  using type = void;
};

template <typename A, typename B, typename C>
struct get_event_from_args<std::tuple<A, B, C>> {
  using type = std::decay_t<C>;
};
template <typename A, typename B>
struct get_event_from_args<std::tuple<A, B>> {
  using type = std::decay_t<B>;
};

template <typename L>
  requires requires { &L::operator(); }
struct extract_event_type<L> : extract_event_type<decltype(&L::operator())> {};

template <typename R, typename C, typename... Args>
struct extract_event_type<R (C::*)(Args...) const> {
  using type = typename get_event_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename C, typename... Args>
struct extract_event_type<R (C::*)(Args...) const noexcept> {
  using type = typename get_event_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename C, typename... Args>
struct extract_event_type<R (C::*)(Args...)> {
  using type = typename get_event_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename C, typename... Args>
struct extract_event_type<R (C::*)(Args...) noexcept> {
  using type = typename get_event_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename... Args>
struct extract_event_type<R (*)(Args...)> {
  using type = typename get_event_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename... Args>
struct extract_event_type<R (*)(Args...) noexcept> {
  using type = typename get_event_from_args<std::tuple<Args...>>::type;
};

// Traits for extracting instance type from handler
template <typename T>
struct extract_instance_type {
  using type = void;
};

template <typename Tuple>
struct get_instance_from_args {
  using type = void;
};

template <typename A, typename B, typename C>
struct get_instance_from_args<std::tuple<A, B, C>> {
  using type = std::decay_t<B>;
};
template <typename A, typename B>
struct get_instance_from_args<std::tuple<A, B>> {
  using type = std::decay_t<A>;
};
template <typename A>
struct get_instance_from_args<std::tuple<A>> {
  using type = std::decay_t<A>;
};

template <typename L>
  requires requires { &L::operator(); }
struct extract_instance_type<L>
    : extract_instance_type<decltype(&L::operator())> {};

template <typename R, typename C, typename... Args>
struct extract_instance_type<R (C::*)(Args...) const> {
  using type = typename get_instance_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename C, typename... Args>
struct extract_instance_type<R (C::*)(Args...) const noexcept> {
  using type = typename get_instance_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename C, typename... Args>
struct extract_instance_type<R (C::*)(Args...)> {
  using type = typename get_instance_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename C, typename... Args>
struct extract_instance_type<R (C::*)(Args...) noexcept> {
  using type = typename get_instance_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename... Args>
struct extract_instance_type<R (*)(Args...)> {
  using type = typename get_instance_from_args<std::tuple<Args...>>::type;
};
template <typename R, typename... Args>
struct extract_instance_type<R (*)(Args...) noexcept> {
  using type = typename get_instance_from_args<std::tuple<Args...>>::type;
};

}  // namespace detail

struct EventBase {
  constexpr EventBase() noexcept = default;
  constexpr explicit EventBase(std::string_view name) noexcept : name_(name) {}

  [[nodiscard]] constexpr std::string_view name() const noexcept {
    return name_;
  }

 private:
  std::string_view name_{};
};

template <typename T = void>
struct Event : EventBase {
    static constexpr std::string_view name_v = detail::type_name<T>();
    constexpr Event() : EventBase(name_v) {}
};

template <>
struct Event<void> : EventBase {
    constexpr Event() noexcept = default;
    constexpr explicit Event(std::string_view name) noexcept : EventBase(name) {}
};

struct Any {};

namespace detail {
template <>
consteval std::string_view type_name<Any>() {
  return "*";
}
}

using Clock = std::chrono::steady_clock;

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

template <typename T>
[[nodiscard]] constexpr auto on() {
  struct TypedName {
    [[nodiscard]] constexpr std::string_view view() const {
      return detail::type_name<T>();
    }
    [[nodiscard]] constexpr std::size_t size() const {
      return detail::type_name<T>().size();
    }
  };
  return detail::on_expr<TypedName>{TypedName{}};
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

template <typename Callable>
[[nodiscard]] constexpr auto when(Callable&& callable) {
  return detail::when_expr<std::decay_t<Callable>>{
      std::forward<Callable>(callable)};
}

template <typename Callable>
[[nodiscard]] constexpr auto at(Callable&& callable) {
  return detail::at_expr<std::decay_t<Callable>>{
      std::forward<Callable>(callable)};
}

template <typename Name, typename... Partials>
[[nodiscard]] constexpr auto define(Name name,
                                    Partials&&... partials) noexcept {
    using namespace detail; // Expose detail to access structural tuples
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

template <auto Model, typename InstanceType = Instance,
          typename TaskProvider = SequentialTaskProvider,
          typename Clock = cthsm::Clock>
struct compile {
  static constexpr auto model_ = Model;
  using instance_type = InstanceType;
  using TaskProviderType = TaskProvider;
  using ClockType = Clock;

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
  static constexpr std::size_t total_activity_count =
      std::tuple_size_v<decltype(activity_tuple)>;
  static constexpr std::size_t total_timer_count =
      std::tuple_size_v<decltype(timer_tuple)>;

  struct ActiveTask {
    typename TaskProvider::TaskHandle task;
    Context* ctx;
  };

  // 4. Thunk Types & Functions
  using behavior_fn = void (*)(Context&, instance_type&, const EventBase&);
  using guard_fn = bool (*)(Context&, instance_type&, const EventBase&);
  using timer_fn = void (*)(Context&, instance_type&, const EventBase&, std::size_t, compile&, detail::timer_kind); 

  template <typename F>
  static constexpr auto invoke(F&& f, Context& c, instance_type& i,
                               const EventBase& e) -> decltype(auto) {
    using TargetInst =
        typename detail::extract_instance_type<std::decay_t<F>>::type;

    using EffInst = std::conditional_t<!std::is_void_v<TargetInst>, TargetInst,
                                       instance_type>;
    auto& eff_i = static_cast<EffInst&>(i);

    if constexpr (std::is_invocable_v<F, Context&, EffInst&,
                                      const EventBase&>) {
      return f(c, eff_i, e);
    } else if constexpr (std::is_invocable_v<F, EffInst&, const EventBase&>) {
      return f(eff_i, e);
    } else if constexpr (std::is_invocable_v<F, EffInst&>) {
      return f(eff_i);
    } else if constexpr (std::is_invocable_v<F>) {
      return f();
    } else {
      // Typed event check
      using ArgType =
          typename detail::extract_event_type<std::decay_t<F>>::type;
      if constexpr (!std::is_void_v<ArgType> &&
                    !std::is_same_v<ArgType, EventBase>) {
        if (e.name() == detail::type_name<ArgType>()) {
          if constexpr (std::is_invocable_v<F, Context&, EffInst&,
                                            const ArgType&>) {
            return f(c, eff_i, static_cast<const ArgType&>(e));
          } else if constexpr (std::is_invocable_v<F, EffInst&,
                                                   const ArgType&>) {
            return f(eff_i, static_cast<const ArgType&>(e));
          }
        }
        
        // Fallback for mismatch
        if constexpr (std::is_invocable_v<F, Context&, EffInst&, const ArgType&>) {
            using R = std::invoke_result_t<F, Context&, EffInst&, const ArgType&>;
            if constexpr (std::is_same_v<R, bool>) return false;
            else return;
        } else if constexpr (std::is_invocable_v<F, EffInst&, const ArgType&>) {
            using R = std::invoke_result_t<F, EffInst&, const ArgType&>;
            if constexpr (std::is_same_v<R, bool>) return false;
            else return;
        } else {
            return;
        }
      } else {
          return;
      }
    }
  }

  template <std::size_t I> static void entry_thunk(Context& c, instance_type& i, const EventBase& e) { invoke(std::get<I>(entry_tuple), c, i, e); }
  template <std::size_t I> static void exit_thunk(Context& c, instance_type& i, const EventBase& e) { invoke(std::get<I>(exit_tuple), c, i, e); }
  template <std::size_t I> static void activity_thunk(Context& c, instance_type& i, const EventBase& e) { invoke(std::get<I>(activity_tuple), c, i, e); }
  template <std::size_t I> static void effect_thunk(Context& c, instance_type& i, const EventBase& e) { invoke(std::get<I>(effect_tuple), c, i, e); }
  template <std::size_t I> static bool guard_thunk(Context& c, instance_type& i, const EventBase& e) { return invoke(std::get<I>(guard_tuple), c, i, e); }
  template <std::size_t I> static void timer_thunk(Context& c, instance_type& i, const EventBase& e, std::size_t /*id*/, compile& self, detail::timer_kind kind) { 
      // Dispatch based logic: 
      // 1. Find transition associated with this timer index
      // 2. Get event name from transition
      // 3. Dispatch that event
      
      if (kind == detail::timer_kind::after) {
          using RetType = decltype(invoke(std::get<I>(timer_tuple), c, i, e));
          if constexpr (detail::is_duration_v<RetType>) {
              auto d = invoke(std::get<I>(timer_tuple), c, i, e);
              self.task_provider_.sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(d), &c);
              if (!c.is_set()) {
                  self.dispatch_timer_event(i, I); // dispatch event for timer I
              }
          }
      } else if (kind == detail::timer_kind::every) {
          using RetType = decltype(invoke(std::get<I>(timer_tuple), c, i, e));
          if constexpr (detail::is_duration_v<RetType>) {
              auto d = invoke(std::get<I>(timer_tuple), c, i, e);
              while (!c.is_set()) {
                  self.task_provider_.sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(d), &c);
                  if (c.is_set()) break;
                  self.dispatch_timer_event(i, I);
              }
          }
      } else if (kind == detail::timer_kind::when) {
          if constexpr (std::is_same_v<decltype(invoke(std::get<I>(timer_tuple), c, i, e)), bool>) {
               bool res = invoke(std::get<I>(timer_tuple), c, i, e);
               while (!res && !c.is_set()) {
                   self.task_provider_.sleep_for(std::chrono::milliseconds(10), &c);
                   if (c.is_set()) break;
                   res = invoke(std::get<I>(timer_tuple), c, i, e);
               }
               if (!c.is_set()) self.dispatch_timer_event(i, I);
          } else {
               invoke(std::get<I>(timer_tuple), c, i, e);
               if (!c.is_set()) self.dispatch_timer_event(i, I);
          }
      } else if (kind == detail::timer_kind::at) {
          // at() support: calculate duration until time point
          // invoke returns time_point
          auto tp = invoke(std::get<I>(timer_tuple), c, i, e);
          
          using TP = decltype(tp);
          if constexpr (detail::is_duration_v<TP> || std::is_same_v<TP, bool> || std::is_void_v<TP>) {
          // Fallback/No-op for mismatched types
        } else {
          // Use the HSM's defined Clock.
          // The time_point returned by at() expression must be compatible with this Clock.
          auto now = Clock::now();
          auto d = tp - now;
              if (d.count() > 0) {
                 self.task_provider_.sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(d), &c);
              }
              if (!c.is_set()) self.dispatch_timer_event(i, I);
          }
      }
  }

  template <std::size_t... Is>
  static constexpr auto make_entry_table(std::index_sequence<Is...>) {
    return std::array<behavior_fn, sizeof...(Is)>{&entry_thunk<Is>...};
  }
  template <std::size_t... Is>
  static constexpr auto make_exit_table(std::index_sequence<Is...>) {
    return std::array<behavior_fn, sizeof...(Is)>{&exit_thunk<Is>...};
  }
  template <std::size_t... Is>
  static constexpr auto make_activity_table(std::index_sequence<Is...>) {
    return std::array<behavior_fn, sizeof...(Is)>{&activity_thunk<Is>...};
  }
  template <std::size_t... Is>
  static constexpr auto make_effect_table(std::index_sequence<Is...>) {
    return std::array<behavior_fn, sizeof...(Is)>{&effect_thunk<Is>...};
  }
  template <std::size_t... Is>
  static constexpr auto make_guard_table(std::index_sequence<Is...>) {
    return std::array<guard_fn, sizeof...(Is)>{&guard_thunk<Is>...};
  }
  template <std::size_t... Is>
  static constexpr auto make_timer_table(std::index_sequence<Is...>) {
    return std::array<timer_fn, sizeof...(Is)>{&timer_thunk<Is>...};
  }

  static constexpr auto entry_table = make_entry_table(
      std::make_index_sequence<std::tuple_size_v<decltype(entry_tuple)>>{});
  static constexpr auto exit_table = make_exit_table(
      std::make_index_sequence<std::tuple_size_v<decltype(exit_tuple)>>{});
  static constexpr auto activity_table = make_activity_table(
      std::make_index_sequence<std::tuple_size_v<decltype(activity_tuple)>>{});
  static constexpr auto effect_table = make_effect_table(
      std::make_index_sequence<std::tuple_size_v<decltype(effect_tuple)>>{});
  static constexpr auto guard_table = make_guard_table(
      std::make_index_sequence<std::tuple_size_v<decltype(guard_tuple)>>{});
  static constexpr auto timer_table = make_timer_table(
      std::make_index_sequence<std::tuple_size_v<decltype(timer_tuple)>>{});

  // 5. Data Members
  TaskProvider task_provider_;

  static constexpr std::size_t max_deferred = 16;
  std::array<std::size_t, max_deferred> deferred_queue_;
  std::size_t deferred_count_;

  std::array<Context, total_activity_count> activity_contexts_;
  std::array<std::optional<ActiveTask>, total_activity_count> active_tasks_;

  std::array<Context, total_timer_count> timer_contexts_;
  std::array<std::optional<ActiveTask>, total_timer_count> active_timer_tasks_;

  std::size_t current_state_id_;

  // 6. Constructor & Destructor
  constexpr compile(TaskProvider tp = {}) noexcept
      : task_provider_(std::move(tp)),
        deferred_queue_{},
        deferred_count_{0},
        activity_contexts_{},
        active_tasks_{},
        timer_contexts_{},
        active_timer_tasks_{},
        current_state_id_(detail::invalid_index) {}

  ~compile() {
    // Cancel all active tasks to unblock threads waiting on contexts
    for (auto& task_opt : active_tasks_) {
      if (task_opt.has_value()) {
        task_opt->ctx->set();
      }
    }
    for (auto& task_opt : active_timer_tasks_) {
      if (task_opt.has_value()) {
        task_opt->ctx->set();
      }
    }
    // Member destructors will join tasks now that they are signalled
  }

  // 7. Accessor
  [[nodiscard]] constexpr std::string_view state() const noexcept {
    if (current_state_id_ == detail::invalid_index) return "";
    return normalized_model.get_state_name(current_state_id_);
  }

  // 8. Public Methods
  constexpr void start(instance_type& instance) {
    // Reset
    deferred_count_ = 0;
    current_state_id_ = 0;  // Root

    // Enter root
    Context ctx{};
    EventBase e{"init"};
    enter_state(ctx, instance, e, 0);

    resolve_initial(ctx, instance, e, 0);
    resolve_completion(ctx, instance);
  }

  constexpr void dispatch(instance_type& instance,
                          std::string_view event_name) noexcept {
    EventBase e{event_name};

    dispatch_internal(instance, e, event_name);
  }

  template <typename T>
  constexpr void dispatch(instance_type& instance) noexcept {
    static_assert(
        std::is_base_of_v<EventBase, T> || std::is_base_of_v<Event<T>, T>,
        "Must be an Event");
    T e{};
    dispatch_internal(instance, e, e.name());
  }

  template <typename T>
  constexpr void dispatch(instance_type& instance, const T& e) noexcept {
    static_assert(std::is_base_of_v<EventBase, T>, "Must be an Event");
    dispatch_internal(instance, e, e.name());
  }

 private:
  constexpr void dispatch_internal(instance_type& instance, const EventBase& e, std::string_view event_name) {
     Context ctx{};
     std::size_t event_id = tables.get_event_id(event_name);
     
     if (event_id != detail::invalid_index && is_deferred(current_state_id_, event_id)) {
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

 public:
  // Removed handle_timer as per instruction.

  constexpr void dispatch_timer_event(instance_type& instance,
                                      std::size_t timer_idx) {
    if (timer_idx < tables.timer_transition_map.size()) {
      std::size_t t_id = tables.timer_transition_map[timer_idx];
      if (t_id != detail::invalid_index) {
        std::size_t event_id = normalized_model.transitions[t_id].event_id;
        if (event_id != detail::invalid_index) {
          std::string_view event_name =
              normalized_model.get_event_name(event_id);
          dispatch(instance, event_name);
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
        std::size_t def_id =
            normalized_model.deferred_events[st.defer_start + i];
        if (normalized_model.get_event_name(def_id) == event_name) return true;
      }
      curr = st.parent_id;
    }
    return false;
  }

  constexpr bool dispatch_event_impl(Context& ctx, instance_type& instance, const EventBase& e, std::size_t event_id) {
      if (current_state_id_ == detail::invalid_index) return false;
      
      std::size_t curr = current_state_id_;
      std::size_t t_id = detail::invalid_index;

      if (event_id != detail::invalid_index) {
        t_id = tables.transition_table[curr][event_id];
        
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
      }
      
      // Fallback to wildcard
      t_id = tables.get_wildcard_transition_id(curr);

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

  constexpr void execute_transition(Context& ctx, instance_type& instance,
                                    const EventBase& e, const auto& t) {
    if (t.target_id != detail::invalid_index) {
      std::size_t target = t.target_id;
      std::size_t old_state = current_state_id_;

      exit_to_lca(ctx, instance, e, old_state, target, t.kind);

      if (t.effect_start != detail::invalid_index) {
        for (std::size_t i = 0; i < t.effect_count; ++i) {
          effect_table[t.effect_start + i](ctx, instance, e);
        }
      }

      enter_from_lca(ctx, instance, e, old_state, target, t.kind);
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

  constexpr void exit_to_lca(
      Context& ctx, instance_type& instance, const EventBase& e,
      std::size_t source, std::size_t target,
      detail::transition_kind kind = detail::transition_kind::external) {
    std::array<std::size_t, 16> source_path;
    std::size_t source_len = 0;
    for (std::size_t s = source; s != detail::invalid_index;
         s = normalized_model.states[s].parent_id) {
      source_path[source_len++] = s;
    }

    std::array<std::size_t, 16> target_path;
    std::size_t target_len = 0;
    for (std::size_t s = target; s != detail::invalid_index;
         s = normalized_model.states[s].parent_id) {
      target_path[target_len++] = s;
    }

    std::size_t lca = detail::invalid_index;
    int i = static_cast<int>(source_len) - 1;
    int j = static_cast<int>(target_len) - 1;
    while (i >= 0 && j >= 0 &&
           source_path[static_cast<std::size_t>(i)] ==
               target_path[static_cast<std::size_t>(j)]) {
      lca = source_path[static_cast<std::size_t>(i)];
      i--;
      j--;
    }

    if (kind == detail::transition_kind::external && source == target) {
      // External self-transition: LCA is parent
      if (normalized_model.states[source].parent_id != detail::invalid_index) {
        lca = normalized_model.states[source].parent_id;
      } else {
        // Root self-transition? LCA is invalid_index effectively (exit all)
        lca = detail::invalid_index;
      }
    }

    for (std::size_t s = source; s != lca && s != detail::invalid_index;
         s = normalized_model.states[s].parent_id) {
      exit_state(ctx, instance, e, s);
    }
  }

  constexpr void enter_from_lca(
      Context& ctx, instance_type& instance, const EventBase& e,
      std::size_t source, std::size_t target,
      detail::transition_kind kind = detail::transition_kind::external) {
    std::array<std::size_t, 16> source_path;
    std::size_t source_len = 0;
    for (std::size_t s = source; s != detail::invalid_index;
         s = normalized_model.states[s].parent_id) {
      source_path[source_len++] = s;
    }

    std::array<std::size_t, 16> target_path;
    std::size_t target_len = 0;
    for (std::size_t s = target; s != detail::invalid_index;
         s = normalized_model.states[s].parent_id) {
      target_path[target_len++] = s;
    }

    int i = static_cast<int>(source_len) - 1;
    int j = static_cast<int>(target_len) - 1;
    while (i >= 0 && j >= 0 &&
           source_path[static_cast<std::size_t>(i)] ==
               target_path[static_cast<std::size_t>(j)]) {
      i--;
      j--;
    }

    if (kind == detail::transition_kind::external && source == target) {
      // Reset i, j to parent level (force entry from parent down)
      // source == target, so paths are same. i and j ended at -1 (if fully
      // matched). We want to enter starting from source (which is target).
      // target_path[0] is target.
      // We want j to start at 0.
      // If we just force logic, we can recalculate j.
      // The loop decremented j until it didn't match or exhausted.
      // If source == target, it exhausted (-1).
      // We want to enter target. So we need loop `for (; j >= 0; j--)` to run
      // for j=0. So set j = 0? Yes, target is at index 0.
      j = 0;
    }

    for (; j >= 0; j--) {
      enter_state(ctx, instance, e, target_path[static_cast<std::size_t>(j)]);
    }
  }

  constexpr void exit_state(Context& ctx, instance_type& instance,
                            const EventBase& e, std::size_t s_id) {
    const auto& s = normalized_model.states[s_id];
    if (s.exit_start != detail::invalid_index) {
      for (std::size_t i = 0; i < s.exit_count; ++i) {
        exit_table[s.exit_start + i](ctx, instance, e);
      }
    }
    auto range = tables.state_timer_ranges[s_id];
    for (std::size_t i = 0; i < range.count; ++i) {
      auto& timer = tables.state_timer_list[range.start + i];
      if constexpr (requires { instance.cancel_timer(timer.timer_idx); }) {
        instance.cancel_timer(timer.timer_idx);
      }
      if (timer.timer_idx < active_timer_tasks_.size() &&
          active_timer_tasks_[timer.timer_idx].has_value()) {
        active_timer_tasks_[timer.timer_idx]->ctx->set();
        if (active_timer_tasks_[timer.timer_idx]->task.joinable()) {
          active_timer_tasks_[timer.timer_idx]->task.join();
        }
        active_timer_tasks_[timer.timer_idx].reset();
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

  constexpr void enter_state(Context& ctx, instance_type& instance,
                             const EventBase& e, std::size_t s_id) {
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

          auto task = task_provider_.create_task(
              [idx, &instance, e, activity_ctx]() {
                activity_table[idx](*activity_ctx, instance, e);
              },
              "activity", 0, 0);

          active_tasks_[idx] = ActiveTask{std::move(task), activity_ctx};
        }
      }
    }
    auto range = tables.state_timer_ranges[s_id];
    for (std::size_t i = 0; i < range.count; ++i) {
      auto& timer = tables.state_timer_list[range.start + i];
      if (timer.timer_idx < timer_table.size()) {
        timer_contexts_[timer.timer_idx].reset();
        Context* timer_ctx = &timer_contexts_[timer.timer_idx];

        auto task = task_provider_.create_task(
            [this, &instance, e, timer, timer_ctx]() {
              timer_table[timer.timer_idx](*timer_ctx, instance, e,
                                           timer.timer_idx, *this, timer.kind);
            },
            "timer", 0, 0);

        active_timer_tasks_[timer.timer_idx] =
            ActiveTask{std::move(task), timer_ctx};
      }
    }
  }

  constexpr void resolve_initial(Context& ctx, instance_type& instance,
                                 const EventBase& e, std::size_t current) {
    std::size_t init = normalized_model.states[current].initial_transition_id;
    while (init != detail::invalid_index) {
      const auto& t = normalized_model.transitions[init];
      if (t.effect_start != detail::invalid_index) {
        for (std::size_t i = 0; i < t.effect_count; ++i) {
          effect_table[t.effect_start + i](ctx, instance, e);
        }
      }
      if (t.target_id != detail::invalid_index) {
        enter_from_lca(ctx, instance, e, current, t.target_id,
                       detail::transition_kind::local);
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
          EventBase empty{""};
          guard_passed = guard_table[t.guard_idx](ctx, instance, empty);
        }
      }

      if (guard_passed) {
        EventBase empty{""};
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
