#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include "kind.hpp"
#include "path.hpp"

namespace hsm {

// Custom hash for heterogeneous lookup with std::string_view
struct StringViewHash {
  using is_transparent = void;

  std::size_t operator()(std::string_view sv) const {
    return std::hash<std::string_view>{}(sv);
  }
  std::size_t operator()(const std::string& s) const {
    return (*this)(std::string_view(s));
  }
  std::size_t operator()(const char* s) const {
    return (*this)(std::string_view(s));
  }
};

// Custom equality for heterogeneous lookup with std::string_view
struct StringViewEqual {
  using is_transparent = void;

  bool operator()(std::string_view lhs, std::string_view rhs) const {
    return lhs == rhs;
  }
  bool operator()(const std::string& lhs, std::string_view rhs) const {
    return std::string_view(lhs) == rhs;
  }
  bool operator()(std::string_view lhs, const std::string& rhs) const {
    return lhs == std::string_view(rhs);
  }
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return lhs == rhs;
  }
  bool operator()(const char* lhs, std::string_view rhs) const {
    return std::string_view(lhs) == rhs;
  }
  bool operator()(std::string_view lhs, const char* rhs) const {
    return lhs == std::string_view(rhs);
  }
  bool operator()(const std::string& lhs, const char* rhs) const {
    return std::string_view(lhs) == std::string_view(rhs);
  }
  bool operator()(const char* lhs, const std::string& rhs) const {
    return std::string_view(lhs) == std::string_view(rhs);
  }
  bool operator()(const char* lhs, const char* rhs) const {
    return std::string_view(lhs) == std::string_view(rhs);
  }
};

// Forward declarations
struct HSM;
struct Event;
struct Context;
struct Partial;
struct Instance;

// Action template for user-defined behaviors
template <typename T = Instance>
using Action = void (*)(Context&, T&, Event&);

template <typename T = Instance>
using Condition = bool (*)(Context&, T&, Event&);

template <typename D>
constexpr bool is_duration_v = std::is_base_of_v<
    std::chrono::duration<typename D::rep, typename D::period>, D>;

template <typename D, typename T = Instance>
using TimeExpression = D (*)(Context&, T&, Event&);

// Task abstraction for embedded systems
struct TaskHandle {
  virtual ~TaskHandle() = default;
  virtual void join() = 0;
  virtual bool joinable() const = 0;
};

// TaskProvider interface for creating tasks/threads
struct TaskProvider {
  virtual ~TaskProvider() = default;
  virtual std::unique_ptr<TaskHandle> create_task(
      std::function<void()> task_function, const std::string& task_name = "",
      size_t stack_size = 0, int priority = 0) = 0;

  virtual void sleep_for(std::chrono::milliseconds duration) = 0;
};

// Default implementation using std::thread
class StdThreadProvider : public TaskProvider {
 public:
  class StdTaskHandle : public TaskHandle {
   public:
    explicit StdTaskHandle(std::thread&& t) : thread_(std::move(t)) {}

    void join() override {
      if (thread_.joinable()) {
        if (std::this_thread::get_id() == thread_.get_id()) {
          thread_.detach();
          return;
        }
        thread_.join();
      }
    }

    bool joinable() const override { return thread_.joinable(); }

   private:
    std::thread thread_;
  };

  std::unique_ptr<TaskHandle> create_task(std::function<void()> task_function,
                                          const std::string& /*task_name*/,
                                          size_t /*stack_size*/,
                                          int /*priority*/) override {
    return std::make_unique<StdTaskHandle>(
        std::thread(std::move(task_function)));
  }

  void sleep_for(std::chrono::milliseconds duration) override {
    std::this_thread::sleep_for(duration);
  }
};

// Global default task provider
inline std::shared_ptr<TaskProvider> default_task_provider() {
  static auto provider = std::make_shared<StdThreadProvider>();
  return provider;
}

enum class Kind : kind_t {
  Null = 0,
  Element = make_kind(1),
  NamedElement = make_kind(2, Element),
  Vertex = make_kind(3, NamedElement),
  State = make_kind(4, Vertex),
  Namespace = make_kind(5, State),
  FinalState = make_kind(6, State),
  Transition = make_kind(7, NamedElement),
  Pseudostate = make_kind(8, Vertex),
  Initial = make_kind(9, Pseudostate),
  Choice = make_kind(10, Pseudostate),
  Behavior = make_kind(11, NamedElement),
  Sequential = make_kind(12, Behavior),
  Concurrent = make_kind(13, Behavior),
  StateMachine = make_kind(14, State, Concurrent),
  External = make_kind(15, Transition),
  Self = make_kind(16, Transition),
  Internal = make_kind(17, Transition),
  Local = make_kind(18, Transition),
  Event = make_kind(19, Element),
  CompletionEvent = make_kind(20, Event),
  TimeEvent = make_kind(21, Event),
  Constraint = make_kind(22, NamedElement),
};

// Base Element interface
struct ElementInterface {
  virtual ~ElementInterface() = default;
  virtual Kind kind() const = 0;
  virtual std::string_view qualified_name() const = 0;
  virtual std::string_view owner() const = 0;
  virtual std::string_view name() const = 0;
};

// Base Element implementation
struct Element : ElementInterface {
  Kind kind_;
  std::string qualified_name_;

  explicit Element(Kind k, std::string qn = "")
      : kind_(k), qualified_name_(std::move(qn)) {}

  Kind kind() const override { return kind_; }
  std::string_view qualified_name() const override { return qualified_name_; }

  std::string_view owner() const override {
    if (qualified_name_ == "/" || qualified_name_.empty()) return "";
    auto pos = qualified_name_.find_last_of('/');
    if (pos == std::string::npos) return "";
    if (pos == 0) return "/";
    return std::string_view(qualified_name_).substr(0, pos);
  }

  std::string_view name() const override {
    auto pos = qualified_name_.find_last_of('/');
    if (pos == std::string::npos) return qualified_name_;
    return std::string_view(qualified_name_).substr(pos + 1);
  }
};

// Vertex - has transitions
struct Vertex : Element {
  std::vector<std::string> transitions;

  explicit Vertex(Kind k, std::string qn) : Element(k, std::move(qn)) {}
};

// Initial pseudostate
struct Initial : Vertex {
  explicit Initial(std::string qn) : Vertex(Kind::Initial, std::move(qn)) {}
};

// State - can have entry/exit/activities and nested states
struct State : Vertex {
  std::string initial;
  std::vector<std::string> entry;
  std::vector<std::string> exit;
  std::vector<std::string> activities;
  std::vector<std::string> deferred;

  explicit State(std::string qn) : Vertex(Kind::State, std::move(qn)) {}
};

// Event class
struct Event : Element {
  std::string name;
  std::any data;

  explicit Event(std::string n = "", Kind k = Kind::Event)
      : Element(k), name(std::move(n)) {
    if (qualified_name_.empty()) qualified_name_ = name;
  }
};

// Static initial event
static Event initial_event("hsm_initial", Kind::CompletionEvent);

// Transition paths for state entry/exit
struct TransitionPath {
  std::vector<std::string> enter;
  std::vector<std::string> exit;
};

// Transition
struct Transition : Element {
  std::string source;
  std::string target;
  std::string guard;
  std::vector<std::string> effect;
  std::vector<std::string> events;
  std::unordered_map<std::string, TransitionPath, StringViewHash,
                     StringViewEqual>
      paths;

  explicit Transition(std::string qn)
      : Element(Kind::Transition, std::move(qn)) {}
};

// Behavior
struct Behavior : Element {
  std::function<void(Context&, Instance&, Event&)> method = nullptr;

  // Add non-templated constructor for direct Instance usage
  explicit Behavior(std::string qn,
                    std::function<void(Context&, Instance&, Event&)> m,
                    Kind k = Kind::Behavior)
      : Element(k, std::move(qn)), method(m) {}

  template <typename T>
  explicit Behavior(std::string qn, std::function<void(Context&, T&, Event&)> m,
                    Kind k = Kind::Behavior)
      : Element(k, std::move(qn)),
        method([m](Context& ctx, Instance& instance, Event& event) {
          m(ctx, static_cast<T&>(instance), event);
        }) {}

  // Additional constructor for function pointers
  template <typename T>
  explicit Behavior(std::string qn, void (*m)(Context&, T&, Event&),
                    Kind k = Kind::Behavior)
      : Element(k, std::move(qn)),
        method([m](Context& ctx, Instance& instance, Event& event) {
          m(ctx, static_cast<T&>(instance), event);
        }) {}
};

// Constraint (guard)
struct Constraint : Element {
  std::function<bool(Context&, Instance&, Event&)> condition = nullptr;

  template <typename T>
  explicit Constraint(std::string qn,
                      std::function<bool(Context&, T&, Event&)> c)
      : Element(Kind::Constraint, std::move(qn)),
        condition([c](Context& ctx, Instance& instance, Event& event) {
          return c(ctx, static_cast<T&>(instance), event);
        }) {}

  explicit Constraint(std::string qn,
                      std::function<bool(Context&, Instance&, Event&)> c)
      : Element(Kind::Constraint, std::move(qn)), condition(c) {}

  template <typename T>
  explicit Constraint(std::string qn, bool (*c)(Context&, T&, Event&))
      : Element(Kind::Constraint, std::move(qn)),
        condition([c](Context& ctx, Instance& instance, Event& event) {
          return c(ctx, static_cast<T&>(instance), event);
        }) {}
};

// Element variant for storage
using ElementVariant =
    std::variant<std::unique_ptr<State>, std::unique_ptr<Vertex>,
                 std::unique_ptr<Transition>, std::unique_ptr<Behavior>,
                 std::unique_ptr<Constraint>, std::unique_ptr<Event>>;

// Helper to get element from variant
template <typename T>
T* get_element(const ElementVariant& variant) {
  if (auto ptr = std::get_if<std::unique_ptr<T>>(&variant)) {
    return ptr->get();
  }
  return nullptr;
}

// Helper to get any element interface from variant
static ElementInterface* get_element_interface(const ElementVariant& variant) {
  return std::visit(
      [](const auto& ptr) -> ElementInterface* { return ptr.get(); }, variant);
}

struct Model;

// Partial elements for building the model
struct Partial {
  virtual ~Partial() = default;
  virtual void apply(Model& model, std::vector<ElementInterface*>& stack) = 0;
};

// Helper to find element in stack
template <typename T>
T* find_in_stack(std::vector<ElementInterface*>& stack, Kind kind) {
  for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
    if (is_kind((*it)->kind(), kind)) {
      return static_cast<T*>(*it);
    }
  }
  return nullptr;
}

// Model - the main container
struct Model : State {
  std::unordered_map<std::string, ElementVariant, StringViewHash,
                     StringViewEqual>
      members;
  std::vector<std::unique_ptr<Partial>> owned_elements;

  // O(1) transition lookup: state_name -> event_name -> transitions
  std::unordered_map<std::string,
                     std::unordered_map<std::string, std::vector<Transition*>,
                                        StringViewHash, StringViewEqual>,
                     StringViewHash, StringViewEqual>
      transition_map;

  // O(1) deferred lookup: state_name -> event_name -> is_deferred
  std::unordered_map<
      std::string,
      std::unordered_map<std::string, bool, StringViewHash, StringViewEqual>,
      StringViewHash, StringViewEqual>
      deferred_map;

  explicit Model(std::string qn) : State(std::move(qn)) {
    kind_ = Kind::StateMachine;
  }

  template <typename T>
  T* get_member(std::string_view qualified_name) {
    auto it = members.find(qualified_name);
    if (it == members.end()) return nullptr;
    return get_element<T>(it->second);
  }

  ElementInterface* get_any_member(std::string_view qualified_name) {
    auto it = members.find(qualified_name);
    if (it == members.end()) return nullptr;
    return get_element_interface(it->second);
  }

  void set_member(const std::string& qualified_name, ElementVariant element) {
    members[qualified_name] = std::move(element);
  }

  void add(std::unique_ptr<Partial> partial) {
    owned_elements.push_back(std::move(partial));
  }
};

// Signal for synchronization
struct Context {
  Context() = default;
  ~Context() = default;

  // Explicitly delete move operations (mutex is not movable)
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;

  // Delete copy operations as well
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  void set() {
    std::lock_guard<std::mutex> lock(mutex_);
    flag_.store(true, std::memory_order_release);
    cv_.notify_all();
  }

  bool is_set() const { return flag_.load(std::memory_order_acquire); }

  void wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return flag_.load(std::memory_order_acquire); });
  }

  void reset() { flag_.store(false, std::memory_order_release); }

 private:
  std::atomic_bool flag_{false};
  std::condition_variable cv_;
  std::mutex mutex_;
};

struct Mutex {
  void lock() {
    internal_.lock();
    signal_.reset();
  }

  void unlock() {
    internal_.unlock();
    signal_.set();
  }

  bool try_lock() {
    if (internal_.try_lock()) {
      signal_.reset();
      return true;
    }
    return false;
  }

  Context& wait() { return signal_; }

 private:
  std::mutex internal_;
  Context signal_;
};

// Fixed-size queue for events
template <size_t MaxSize>
struct FixedQueue {
  bool push(Event&& event) {
    std::unique_lock lock(mutex_);
    if (count_ == MaxSize) return false;

    size_t insert_pos;
    if (is_kind(event.kind(), Kind::CompletionEvent)) {
      head_ = (head_ == 0) ? MaxSize - 1 : head_ - 1;
      insert_pos = head_;
    } else {
      insert_pos = tail_;
      tail_ = (tail_ + 1) % MaxSize;
    }

    events_[insert_pos] = std::move(event);
    count_++;
    return true;
  }

  Event pop() {
    std::unique_lock lock(mutex_);
    if (count_ == 0) return Event{};

    auto result = std::move(events_[head_]);
    head_ = (head_ + 1) % MaxSize;
    count_--;
    return result;
  }

  bool empty() const {
    std::shared_lock lock(mutex_);
    return count_ == 0;
  }

  size_t size() const {
    std::shared_lock lock(mutex_);
    return count_;
  }

 private:
  std::array<Event, MaxSize> events_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
  mutable std::shared_mutex mutex_;
};

// Instance base class
struct Instance {
  friend struct HSM;

  HSM* __hsm = nullptr;
  Instance() = default;
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;
  Instance(Instance&&) = delete;
  Instance& operator=(Instance&&) = delete;
  virtual ~Instance() = default;

  Context& dispatch(Event event);
  std::string_view state() const;
  TaskProvider& task_provider();

 private:
  // Static signal used when no HSM is attached
  static Context no_context_;
  static std::shared_ptr<TaskProvider> null_task_provider_;
};

inline std::shared_ptr<TaskProvider> Instance::null_task_provider_ =
    default_task_provider();

inline std::vector<std::string_view> event_name_variants(
    std::string_view event_name) {
  std::vector<std::string_view> variants;
  variants.reserve(4);
  variants.push_back(event_name);

  std::string_view current = event_name;
  while (!current.empty()) {
    auto pos = current.find_last_of("_/");
    if (pos == std::string_view::npos) break;
    current = current.substr(0, pos);
    if (current.empty()) break;
    variants.push_back(current);
  }
  return variants;
}

struct Active {
  std::unique_ptr<TaskHandle> task;
  std::shared_ptr<Context> signal;

  Active(std::unique_ptr<TaskHandle>&& t, std::shared_ptr<Context>&& s)
      : task(std::move(t)), signal(std::move(s)) {}

  // Make Active movable but not copyable
  Active(Active&&) = default;
  Active& operator=(Active&&) = default;
  Active(const Active&) = delete;
  Active& operator=(const Active&) = delete;
};

// Build transition lookup table for O(1) event dispatch
inline void buildTransitionTable(Model& model) {
  // For each state in the model
  for (const auto& [state_name, state_variant] : model.members) {
    auto* state_element = get_element_interface(state_variant);
    if (!state_element || !is_kind(state_element->kind(), Kind::State))
      continue;

    // Initialize tables for this state
    auto& state_transitions = model.transition_map[state_name];

    // Collect all transitions accessible from this state by walking up
    // hierarchy
    std::unordered_map<std::string, std::vector<std::pair<Transition*, int>>,
                       StringViewHash, StringViewEqual>
        transitions_by_event;
    std::string current_path = state_name;
    int depth = 0;

    while (!current_path.empty() && current_path != ".") {
      // Try to get the element at current path
      auto* current_element = model.get_any_member(current_path);
      if (!current_element) {
        // Special case: if current_path is the model name but we can't find it
        // in members, use the model itself
        if (current_path == model.qualified_name()) {
          current_element = &model;
        } else {
          break;
        }
      }

      // Check if it's a vertex (state or pseudostate)
      if (is_kind(current_element->kind(), Kind::Vertex)) {
        auto* current_vertex = static_cast<Vertex*>(current_element);

        // Process transitions at this level
        for (const auto& trans_name : current_vertex->transitions) {
          auto* transition = model.get_member<Transition>(trans_name);
          if (transition && !transition->events.empty()) {
            // Process each event this transition handles
            for (const auto& event_name : transition->events) {
              // Skip wildcard events for now
              if (event_name.find('*') != std::string::npos) {
                continue;
              }

              // Add to transitions by event with priority
              transitions_by_event[event_name].push_back({transition, depth});
            }
          }
        }
      }

      // If we just processed the model, break
      if (current_path == model.qualified_name()) break;

      // Move up to parent
      if (current_path == "/") break;

      auto pos = current_path.find_last_of('/');
      if (pos == std::string::npos || pos == 0) {
        break;
      }
      current_path = current_path.substr(0, pos);
      if (current_path.empty()) current_path = "/";
      depth++;
    }

    // Sort transitions by priority (lower depth = higher priority) and add to
    // map
    for (auto& [event_name, transitions] : transitions_by_event) {
      std::sort(
          transitions.begin(), transitions.end(),
          [](const auto& a, const auto& b) { return a.second < b.second; });

      // Extract just the transition pointers
      std::vector<Transition*> sorted_transitions;
      sorted_transitions.reserve(transitions.size());
      for (const auto& [trans, _] : transitions) {
        sorted_transitions.push_back(trans);
      }

      state_transitions[event_name] = std::move(sorted_transitions);
    }
  }
}

// Build deferred event lookup table for O(1) deferred event checking
inline void buildDeferredTable(Model& model) {
  // For each state in the model
  for (const auto& [state_name, state_variant] : model.members) {
    auto* state_element = get_element_interface(state_variant);
    if (!state_element || !is_kind(state_element->kind(), Kind::State))
      continue;

    auto& state_deferred = model.deferred_map[state_name];

    std::string current_path = state_name;

    while (!current_path.empty() && current_path != ".") {
      auto* current_state = model.get_member<State>(current_path);
      if (current_state && is_kind(current_state->kind(), Kind::State)) {
        // Process deferred events at this level
        for (const auto& deferred_event : current_state->deferred) {
          // Only support exact event names for O(1) lookup
          if (deferred_event.find('*') == std::string::npos) {
            state_deferred[deferred_event] = true;
          }
        }
      }

      // Move up to parent
      if (current_path == "/" || current_path.empty()) break;
      auto pos = current_path.find_last_of('/');
      if (pos == std::string::npos || pos == 0) {
        current_path = "/";
      } else {
        current_path = current_path.substr(0, pos);
      }
    }
  }
}

// Main HSM class
struct HSM : public Instance {
  friend struct Instance;
  friend Context& stop(Instance& instance);
  friend void start(Instance& instance, std::unique_ptr<Model>& model);

  static constexpr size_t MAX_QUEUE_SIZE = 32;

 private:
  explicit HSM(Model& model_ref,
               std::unique_ptr<TaskProvider> task_provider = nullptr)
      : HSM(*this, model_ref, std::move(task_provider)) {}

  explicit HSM(std::unique_ptr<Model>& model_ptr,
               std::unique_ptr<TaskProvider> task_provider = nullptr)
      : HSM(*model_ptr, std::move(task_provider)) {}

  explicit HSM(Instance& instance, Model& model_ref,
               std::unique_ptr<TaskProvider> task_provider = nullptr)
      : model_(model_ref),
        instance_(instance),
        task_provider_(task_provider ? std::move(task_provider)
                                     : default_task_provider()),
        initialized_(false) {
    instance.__hsm = this;
    // Don't process anything yet - wait for start()
    current_state_.store(nullptr);
  }

  explicit HSM(Instance& instance, std::unique_ptr<Model>& model_ptr,
               std::unique_ptr<TaskProvider> task_provider = nullptr)
      : HSM(instance, *model_ptr, std::move(task_provider)) {
    (void)instance_;
  }

  ~HSM() {
    // Ensure proper cleanup when HSM is destroyed
    if (current_state_.load() != nullptr) {
      stop().wait();
    }
  }

  // Start the state machine - must be called before dispatch
  Context& start() {
    if (initialized_) {
      return processing_mutex_.wait();
    }

    std::lock_guard lock(processing_mutex_);

    // Build optimization tables

    // Process initial transitions if any
    if (!model_.initial.empty()) {
      auto* initial_pseudo = model_.get_member<Vertex>(model_.initial);
      if (initial_pseudo && !initial_pseudo->transitions.empty()) {
        auto* initial_trans =
            model_.get_member<Transition>(initial_pseudo->transitions[0]);
        if (initial_trans) {
          // For initial transitions, pass the parent state (owner) as the
          // current state
          std::string_view owner_path = initial_pseudo->owner();
          ElementInterface* parent_state = nullptr;
          if (!owner_path.empty() && owner_path != ".") {
            parent_state = model_.get_any_member(owner_path);
          }
          if (!parent_state) {
            // If no parent, use the model itself
            parent_state = &model_;
          }

          auto* next_state =
              transition(parent_state, initial_trans, initial_event);
          if (next_state) {
            current_state_.store(next_state);
          }
        }
      } else if (initial_pseudo) {
        // Initial pseudostate exists but has no transitions - no active state
        current_state_.store(nullptr);
      }
    } else {
      // No initial transition, no active state
      current_state_.store(nullptr);
    }

    initialized_ = true;
    return processing_mutex_.wait();
  }

  Context& dispatch(Event event) {
    if (!initialized_) {
      fprintf(stderr, "HSM not started, call start() first\n");
      return processing_mutex_.wait();
    }

    auto* state = current_state_.load();
    if (!state) {
      return processing_mutex_.wait();
    }

    if (!queue_.push(std::move(event))) {
      fprintf(stderr, "HSM queue full, event dropped\n");
      return processing_mutex_.wait();
    }

    if (processing_mutex_.try_lock()) {
      process_queue();
    }
    return processing_mutex_.wait();
  }

  std::string_view state() const {
    auto* state_ptr = current_state_.load();
    return state_ptr ? state_ptr->qualified_name() : "";
  }

  Context& stop() {
    // Lock processing to prevent new events
    std::lock_guard lock(processing_mutex_);

    // Create a final event for exit actions
    Event final_event("hsm_final", Kind::CompletionEvent);

    // Exit all states from current to root
    auto* current = current_state_.load();
    while (current && current->qualified_name() != "/") {
      if (is_kind(current->kind(), Kind::State)) {
        auto* state = static_cast<State*>(current);
        exit(*state, final_event);
      }

      // Move to parent state
      std::string_view owner_name = current->owner();
      if (owner_name.empty() || owner_name == ".") {
        break;
      }
      current = model_.get_any_member(owner_name);
    }

    // Terminate all remaining activities
    {
      std::lock_guard active_lock(active_mutex_);
      for (auto& [name, active] : active_) {
        active.signal->set();
        if (active.task->joinable()) {
          active.task->join();
        }
      }
      active_.clear();
    }

    // Set current state to null to indicate stopped
    current_state_.store(nullptr);

    return processing_mutex_.wait();
  }

  // Access to task provider for behaviors
  TaskProvider& task_provider() { return *task_provider_; }
  const TaskProvider& task_provider() const { return *task_provider_; }

 private:
  Model& model_;
  Instance& instance_;
  Mutex processing_mutex_, active_mutex_;
  std::atomic<ElementInterface*> current_state_{nullptr};
  FixedQueue<MAX_QUEUE_SIZE> queue_;
  std::unordered_map<std::string, Active, StringViewHash, StringViewEqual>
      active_;
  std::shared_ptr<TaskProvider> task_provider_;
  bool initialized_;

  ElementInterface* find_initial_vertex(State* root_state) {
    if (!root_state || root_state->initial.empty()) {
      return root_state;
    }

    auto* initial_pseudo = model_.get_member<Vertex>(root_state->initial);
    if (!initial_pseudo || initial_pseudo->transitions.empty()) {
      return root_state;
    }

    auto* initial_trans =
        model_.get_member<Transition>(initial_pseudo->transitions[0]);
    if (!initial_trans || initial_trans->target.empty()) {
      return root_state;
    }

    auto* target_vertex = model_.get_any_member(initial_trans->target);
    if (!target_vertex) {
      return root_state;
    }

    return target_vertex;
  }

  // Find enabled transition using O(1) lookup
  Transition* findEnabledTransition(
      const std::string& state_name, Event& event,
      const std::vector<std::string_view>& event_names) {
    // Look up transitions for this state (includes ancestor transitions)
    auto state_it = model_.transition_map.find(state_name);
    if (state_it != model_.transition_map.end()) {
      for (auto key : event_names) {
        auto event_it = state_it->second.find(key);
        if (event_it == state_it->second.end()) continue;
        // Check guards and return first enabled transition
        for (auto* transition : event_it->second) {
          if (!transition->guard.empty()) {
            auto* guard = model_.get_member<Constraint>(transition->guard);
            if (guard && guard->condition) {
              Context ctx;
              if (!guard->condition(ctx, instance_, event)) {
                continue;
              }
            }
          }
          return transition;
        }
      }
    }

    return nullptr;
  }

  void process_queue() {
    std::vector<Event> deferred;

    while (!queue_.empty()) {
      auto event = queue_.pop();

      auto event_names = event_name_variants(event.name);
      if (event_names.empty()) {
        event_names.emplace_back(event.name);
      }

      auto* state = current_state_.load();
      if (!state) {
        continue;
      }

      // O(1) deferred event check
      bool is_deferred = false;
      std::string state_name(state->qualified_name());
      auto deferred_it = model_.deferred_map.find(state_name);
      if (deferred_it != model_.deferred_map.end()) {
        for (auto key : event_names) {
          auto event_it = deferred_it->second.find(key);
          if (event_it != deferred_it->second.end() && event_it->second) {
            is_deferred = true;
            break;
          }
        }
      }

      // If deferred, skip transition lookup
      if (is_deferred) {
        deferred.emplace_back(std::move(event));
        continue;
      }

      // O(1) transition lookup
      auto* triggered_transition =
          findEnabledTransition(state_name, event, event_names);

      if (triggered_transition) {
        auto* old_state = state;
        auto* next_state = transition(state, triggered_transition, event);
        if (!next_state) {
          std::cerr << "ERROR: transition() returned null" << std::endl;
        }
        current_state_.store(next_state);

        // If state changed, re-queue deferred events immediately
        if (next_state &&
            next_state->qualified_name() != old_state->qualified_name()) {
          for (auto& deferred_event : deferred) {
            queue_.push(std::move(deferred_event));
          }
          deferred.clear();
        }
      }
      // If no transition found, event is discarded (not deferred)
    }

    // Re-queue any remaining deferred events
    for (auto& event : deferred) {
      queue_.push(std::move(event));
    }
    processing_mutex_.unlock();
  }

  ElementInterface* transition(ElementInterface* current, Transition* trans,
                               Event& event) {
    if (!current || !trans) return current;

    auto it = trans->paths.find(current->qualified_name());
    if (it == trans->paths.end()) {
      // If no exact match, check if this transition is defined on an ancestor
      // state and we can compute the path dynamically
      if (path::is_ancestor(trans->source, current->qualified_name())) {
        // The transition is defined on an ancestor of the current state
        // Compute the path dynamically
        TransitionPath computed_path;

        if (!trans->target.empty() && !is_kind(trans->kind(), Kind::Internal)) {
          // Use LCA to compute the exit and enter paths
          std::string lca = path::lca(current->qualified_name(), trans->target);
          // Debug
          // std::cerr << "Dynamic path: current=" << current->qualified_name()
          //           << ", target=" << trans->target << ", lca=" << lca <<
          //           std::endl;

          // Exit from current state up to (but not including) LCA
          std::string current_path = std::string(current->qualified_name());
          while (!current_path.empty() && current_path != "/" &&
                 current_path != lca) {
            computed_path.exit.push_back(current_path);
            auto pos = current_path.find_last_of('/');
            if (pos == std::string::npos || pos == 0) {
              break;
            }
            current_path = current_path.substr(0, pos);
          }

          // Enter from LCA down to target
          std::vector<std::string> enter_states;
          current_path = trans->target;
          while (!current_path.empty() && current_path != "/" &&
                 current_path != lca) {
            enter_states.push_back(current_path);
            auto pos = current_path.find_last_of('/');
            if (pos == std::string::npos || pos == 0) {
              break;
            }
            current_path = current_path.substr(0, pos);
          }
          std::reverse(enter_states.begin(), enter_states.end());
          for (const auto& state : enter_states) {
            computed_path.enter.push_back(state);
          }
        }

        // Cache the computed path for future use
        it = trans->paths.emplace(current->qualified_name(), computed_path)
                 .first;
      } else if (trans->target.empty()) {
        // For internal transitions, we still need to execute effects
        // Create an empty path so effects get executed
        TransitionPath empty_path;
        it = trans->paths.emplace(current->qualified_name(), empty_path).first;
      } else {
        // Debug output
        std::cerr << "ERROR: No path found for state '"
                  << current->qualified_name() << "' in transition from '"
                  << trans->source << "' to '" << trans->target << "'"
                  << std::endl;
        std::cerr << "Available paths:" << std::endl;
        for (const auto& [state, path] : trans->paths) {
          std::cerr << "  " << state << std::endl;
        }
        return nullptr;
      }
    }

    const auto& path = it->second;

    // Exit states
    for (const auto& exiting : path.exit) {
      auto* state = model_.get_member<State>(exiting);
      if (state) exit(*state, event);
    }

    // Execute effects
    for (const auto& effect_name : trans->effect) {
      auto* effect = model_.get_member<Behavior>(effect_name);
      if (effect) {
        execute_behavior(effect, event);
      }
    }

    if (is_kind(trans->kind(), Kind::Internal)) {
      return current;
    }

    // Enter states
    ElementInterface* result = current;
    for (const auto& entering : path.enter) {
      auto* next = model_.get_any_member(entering);
      if (!next) continue;

      bool default_entry = entering == trans->target;
      result = enter(next, event, default_entry);
      if (default_entry) return result;
    }

    if (trans->target.empty()) {
      return current;
    }

    // Debug output
    auto* target_state = model_.get_any_member(trans->target);
    if (!target_state) {
      std::cerr << "ERROR: Target state not found: " << trans->target
                << std::endl;
      std::cerr << "Available members:" << std::endl;
      for (const auto& [name, _] : model_.members) {
        std::cerr << "  " << name << std::endl;
      }
    }

    return target_state;
  }

  ElementInterface* enter(ElementInterface* vertex, Event& event,
                          bool default_entry) {
    if (!vertex) return nullptr;

    if (is_kind(vertex->kind(), Kind::State)) {
      auto* state = static_cast<State*>(vertex);
      if (!state || !is_kind(state->kind(), Kind::State)) return vertex;

      // Execute entry actions
      for (const auto& entry_name : state->entry) {
        auto* entry = model_.get_member<Behavior>(entry_name);
        if (entry) execute_behavior(entry, event);
      }

      // Start activities
      for (const auto& activity_name : state->activities) {
        auto* activity = model_.get_member<Behavior>(activity_name);
        if (activity) execute_behavior(activity, event);
      }

      if (!default_entry || state->initial.empty()) {
        return state;
      }

      auto* initial = model_.get_member<Vertex>(state->initial);
      if (!initial || initial->transitions.empty()) {
        return state;
      }

      auto* initial_trans =
          model_.get_member<Transition>(initial->transitions[0]);
      if (!initial_trans) {
        return state;
      }

      auto* result = transition(state, initial_trans, event);
      // If transition fails or returns to initial (transient), stay in
      // containing state
      if (!result || result == initial) {
        return state;
      }
      return result;
    }

    if (is_kind(vertex->kind(), Kind::Choice)) {
      auto* choice = static_cast<Vertex*>(vertex);
      if (!choice || !is_kind(choice->kind(), Kind::Choice)) return vertex;

      for (const auto& trans_name : choice->transitions) {
        auto* choice_trans = model_.get_member<Transition>(trans_name);
        if (!choice_trans) continue;

        bool guard_ok = true;
        if (!choice_trans->guard.empty()) {
          auto* guard = model_.get_member<Constraint>(choice_trans->guard);
          if (guard && guard->condition) {
            Context ctx;
            guard_ok = guard->condition(ctx, instance_, event);
          } else {
            guard_ok = false;
          }
        }

        if (guard_ok) {
          return transition(choice, choice_trans, event);
        }
      }
    }

    return vertex;
  }

  void exit(State& state, Event& event) {
    // Terminate activities
    for (const auto& activity_name : state.activities) {
      terminate_activity(activity_name);
    }

    // Execute exit actions
    for (const auto& exit_name : state.exit) {
      auto* exit_behavior = model_.get_member<Behavior>(exit_name);
      if (exit_behavior) execute_behavior(exit_behavior, event);
    }
  }

  void execute_behavior(Behavior* behavior, Event& event) {
    if (!behavior || !behavior->method) return;

    // Fast path for non-concurrent behaviors (most common case)
    if (!is_kind(behavior->kind(), Kind::Concurrent)) {
      Context ctx;
      behavior->method(ctx, instance_, event);
      return;
    }

    // Slow path for concurrent behaviors
    std::lock_guard lock(active_mutex_);
    std::string_view behavior_name_view = behavior->qualified_name();
    // Only create string if we need to insert
    auto it = active_.find(behavior_name_view);
    if (it == active_.end()) {
      // Now we need the string for storage
      std::string behavior_name(behavior_name_view);

      // Create a shared_ptr for the Signal
      auto ctx = std::make_shared<Context>();

      // Capture shared_ptr to keep Context alive
      auto task = task_provider_->create_task(
          [this, behavior, event, ctx]() mutable {
            behavior->method(*ctx, instance_, event);
          },
          behavior_name, 0, 0);

      active_.emplace(std::move(behavior_name),
                      Active(std::move(task), std::move(ctx)));
    }
  }

  void terminate_activity(std::string_view activity_name) {
    std::lock_guard lock(active_mutex_);
    auto it = active_.find(activity_name);
    if (it != active_.end()) {
      it->second.signal->set();
      if (it->second.task->joinable()) {
        it->second.task->join();
      }
      active_.erase(it);
    }
  }
};

// Static signal initialization
inline Context Instance::no_context_;

inline Context& Instance::dispatch(Event event) {
  if (!__hsm) {
    // Ensure the signal is set for immediate return
    no_context_.set();
    return no_context_;
  }
  return __hsm->dispatch(std::move(event));
}

inline std::string_view Instance::state() const {
  if (!__hsm) {
    return "";
  }
  return __hsm->state();
}

inline TaskProvider& Instance::task_provider() {
  if (!__hsm) {
    return *null_task_provider_;
  }
  return __hsm->task_provider();
}

// Partial state
struct PartialState : Partial {
  std::string name;
  std::vector<std::unique_ptr<Partial>> elements;

  explicit PartialState(std::string n) : name(std::move(n)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* owner = find_in_stack<State>(stack, Kind::State);
    if (!owner) {
      return;
    }

    std::string full_name = path::join(owner->qualified_name(), name);
    auto state = std::make_unique<State>(full_name);
    auto* state_ptr = state.get();

    model.set_member(full_name, std::move(state));
    stack.push_back(state_ptr);

    for (auto& element : elements) {
      element->apply(model, stack);
    }

    stack.pop_back();
  }
};

// Partial transition
struct PartialTransition : Partial {
  std::string name;
  std::vector<std::unique_ptr<Partial>> elements;

  explicit PartialTransition(std::string n) : name(std::move(n)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* owner = find_in_stack<Vertex>(stack, Kind::Vertex);
    if (!owner) {
      return;
    }

    std::string full_name = path::join(
        owner->qualified_name(),
        name.empty() ? "transition_" + std::to_string(model.members.size())
                     : name);
    auto transition = std::make_unique<Transition>(full_name);
    auto* trans_ptr = transition.get();

    model.set_member(full_name, std::move(transition));
    stack.push_back(trans_ptr);

    for (auto& element : elements) {
      element->apply(model, stack);
    }

    stack.pop_back();

    // Set default source if not set
    if (trans_ptr->source.empty()) {
      trans_ptr->source = std::string(owner->qualified_name());
    }

    // Note: path::join() in the target resolution already handles ".", "..",
    // etc. No special handling needed here

    // Add transition to the actual source state, not the owner
    Vertex* source_vertex = model.get_member<State>(trans_ptr->source);
    if (!source_vertex) {
      source_vertex = model.get_member<Vertex>(trans_ptr->source);
    }

    // Special case: if source is the model itself, add transition to the model
    if (!source_vertex && trans_ptr->source == model.qualified_name()) {
      source_vertex = &model;
    }

    if (source_vertex && is_kind(source_vertex->kind(), Kind::Vertex)) {
      source_vertex->transitions.push_back(full_name);
    }

    // Determine transition kind
    if (trans_ptr->target == trans_ptr->source) {
      trans_ptr->kind_ = Kind::Self;
    } else if (trans_ptr->target.empty()) {
      trans_ptr->kind_ = Kind::Internal;
    } else {
      trans_ptr->kind_ = Kind::External;  // Simplified for now
    }

    // Compute paths using LCA algorithm
    TransitionPath path;

    // For initial pseudo-states, we need special handling
    bool is_initial_transition = false;
    auto* source_element = model.get_any_member(trans_ptr->source);
    if (source_element && is_kind(source_element->kind(), Kind::Initial)) {
      is_initial_transition = true;
    }

    // Internal transitions still need a path entry (even if empty)
    if (!is_kind(trans_ptr->kind(), Kind::Internal)) {
      if (is_kind(trans_ptr->kind(), Kind::Self)) {
        // Self transitions: exit and re-enter the same state
        path.exit.push_back(trans_ptr->target);
        path.enter.push_back(trans_ptr->target);
      } else if (!is_initial_transition && !trans_ptr->target.empty()) {
        // Use LCA algorithm to compute correct exit and enter paths
        std::string lca = path::lca(trans_ptr->source, trans_ptr->target);

        // Debug
        // std::cerr << "Computing path from " << trans_ptr->source << " to " <<
        // trans_ptr->target << std::endl; std::cerr << "LCA: " << lca <<
        // std::endl;

        // Helper to get ancestors of a state up to (but not including) a limit
        auto get_ancestors_up_to =
            [](const std::string& state_path,
               const std::string& limit) -> std::vector<std::string> {
          std::vector<std::string> ancestors;
          std::string current = state_path;
          while (!current.empty() && current != "/" && current != limit) {
            ancestors.push_back(current);
            auto pos = current.find_last_of('/');
            if (pos == std::string::npos || pos == 0) {
              break;
            }
            current = current.substr(0, pos);
          }
          return ancestors;
        };

        // Build exit path: from source up to (but not including) LCA
        auto exit_states = get_ancestors_up_to(trans_ptr->source, lca);
        for (const auto& state : exit_states) {
          path.exit.push_back(state);
        }

        // Build enter path: from LCA down to target
        auto target_ancestors = get_ancestors_up_to(trans_ptr->target, lca);
        // Reverse to get top-down order for entering
        std::reverse(target_ancestors.begin(), target_ancestors.end());
        for (const auto& state : target_ancestors) {
          path.enter.push_back(state);
        }
      } else if (!is_initial_transition) {
        // Simple case: just exit source
        path.exit.push_back(trans_ptr->source);
      }
    }

    if (!trans_ptr->target.empty() && is_initial_transition) {
      // For initial transitions, we need to enter all states on the path to
      // target Get the owner of the initial pseudo-state
      std::string_view owner_path = source_element->owner();

      // Split the target path and build enter list
      std::vector<std::string> enter_states;
      std::string current = std::string(trans_ptr->target);

      // Walk up from target to the owner, collecting states
      while (!current.empty() && current != owner_path) {
        enter_states.push_back(current);
        auto pos = current.find_last_of('/');
        if (pos == std::string::npos || pos == 0) {
          break;
        }
        current = current.substr(0, pos);
      }

      // Reverse to get correct entry order (parent first, then children)
      std::reverse(enter_states.begin(), enter_states.end());
      for (const auto& state : enter_states) {
        path.enter.push_back(state);
      }
    }

    // Store the path - for initial transitions, use the parent of the initial
    // pseudostate as key
    if (is_initial_transition) {
      std::string_view owner_path = source_element->owner();
      trans_ptr->paths[std::string(owner_path)] = path;
    } else {
      trans_ptr->paths[trans_ptr->source] = path;
    }

    // For hierarchical transitions, we need to create paths for all possible
    // source states If the source is a composite state, create paths for all
    // its child states
    auto* source_state = model.get_member<State>(trans_ptr->source);
    if (source_state && !trans_ptr->target.empty()) {
      // Find all states that are children of the source state
      for (const auto& [member_name, member_variant] : model.members) {
        auto* member_element = get_element_interface(member_variant);
        if (member_element && is_kind(member_element->kind(), Kind::State)) {
          // Check if this state is a child of the source state
          std::string_view member_qn = member_element->qualified_name();
          if (path::is_ancestor_or_equal(trans_ptr->source, member_qn) &&
              member_qn != trans_ptr->source) {
            // Create a path for this child state using LCA
            TransitionPath child_path;
            if (!is_kind(trans_ptr->kind(), Kind::Internal)) {
              if (is_kind(trans_ptr->kind(), Kind::Self)) {
                // For self transitions, child states should exit if they're
                // active
                child_path.exit.push_back(std::string(member_qn));
                // No enter path for child states in self transitions
              } else {
                // Use LCA to compute correct paths
                std::string lca =
                    path::lca(std::string(member_qn), trans_ptr->target);

                // Exit from child up to (but not including) LCA
                std::string current = std::string(member_qn);
                while (!current.empty() && current != "/" && current != lca) {
                  child_path.exit.push_back(current);
                  auto pos = current.find_last_of('/');
                  if (pos == std::string::npos || pos == 0) {
                    break;
                  }
                  current = current.substr(0, pos);
                }

                // Enter from LCA down to target
                std::vector<std::string> enter_states;
                current = trans_ptr->target;
                while (!current.empty() && current != "/" && current != lca) {
                  enter_states.push_back(current);
                  auto pos = current.find_last_of('/');
                  if (pos == std::string::npos || pos == 0) {
                    break;
                  }
                  current = current.substr(0, pos);
                }
                std::reverse(enter_states.begin(), enter_states.end());
                for (const auto& state : enter_states) {
                  child_path.enter.push_back(state);
                }
              }
            }
            trans_ptr->paths[std::string(member_qn)] = child_path;
          }
        }
      }
    }
  }
};

// Partial activity (concurrent behavior)
template <typename T>
struct PartialActivity : Partial {
  Action<T> action;
  std::function<void(Context&, T&, Event&)> func;

  explicit PartialActivity(Action<T> act) : action(act), func(nullptr) {}

  // Additional constructor for std::function (lambdas)
  explicit PartialActivity(std::function<void(Context&, T&, Event&)> f)
      : action(nullptr), func(std::move(f)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* state = find_in_stack<State>(stack, Kind::State);
    if (!state || (!action && !func)) return;

    std::string activity_name =
        path::join(state->qualified_name(),
                   "activity_" + std::to_string(model.members.size()));

    // Activities are inherently concurrent
    std::unique_ptr<Behavior> behavior;
    if (action) {
      behavior =
          std::make_unique<Behavior>(activity_name, action, Kind::Concurrent);
    } else {
      behavior =
          std::make_unique<Behavior>(activity_name, func, Kind::Concurrent);
    }

    model.set_member(activity_name, std::move(behavior));
    state->activities.push_back(activity_name);
  }
};

// Partial source
struct PartialSource : Partial {
  std::string source_name;
  explicit PartialSource(std::string name) : source_name(std::move(name)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* transition = find_in_stack<Transition>(stack, Kind::Transition);
    if (transition) {
      std::string resolved_name = source_name;

      if (!path::is_absolute(source_name)) {
        auto* base_vertex = find_in_stack<Vertex>(stack, Kind::Vertex);
        auto resolve_base = [&](bool self_ref) -> std::string {
          if (!base_vertex) return std::string(model.qualified_name());
          if (!self_ref && is_kind(base_vertex->kind(), Kind::Initial)) {
            auto owner_path = base_vertex->owner();
            if (!owner_path.empty() && owner_path != ".") {
              return std::string(owner_path);
            }
          }
          return std::string(base_vertex->qualified_name());
        };

        resolved_name =
            path::join(resolve_base(false), std::string_view(source_name));
      } else if (!path::is_ancestor(model.qualified_name(), source_name)) {
        // For absolute paths not under the model, prepend model name
        // Remove leading '/' from source_name before joining
        if (!source_name.empty() && source_name[0] == '/') {
          resolved_name =
              path::join(model.qualified_name(), source_name.substr(1));
        } else {
          resolved_name = path::join(model.qualified_name(), source_name);
        }
      }

      transition->source = resolved_name;
    }
  }
};

// Partial target
struct PartialTarget : Partial {
  std::string target_name;
  explicit PartialTarget(std::string name) : target_name(std::move(name)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* transition = find_in_stack<Transition>(stack, Kind::Transition);
    if (transition) {
      std::string resolved_name = target_name;

      if (!path::is_absolute(target_name)) {
        auto resolve_base = [&]() -> std::string {
          bool uses_explicit_relative =
              !target_name.empty() && target_name.front() == '.';
          auto* vertex = find_in_stack<Vertex>(stack, Kind::Vertex);
          if (vertex) {
            const bool is_initial = is_kind(vertex->kind(), Kind::Initial);
            if ((uses_explicit_relative && !is_initial) ||
                is_kind(vertex->kind(), Kind::State)) {
              return std::string(vertex->qualified_name());
            }
            auto owner_path = vertex->owner();
            if (!owner_path.empty() && owner_path != ".") {
              return std::string(owner_path);
            }
          }
          auto* state_owner = find_in_stack<State>(stack, Kind::State);
          if (state_owner) {
            return std::string(state_owner->qualified_name());
          }
          return std::string(model.qualified_name());
        };

        auto base_path = resolve_base();
        if (target_name == ".") {
          resolved_name = base_path;
        } else {
          resolved_name = path::join(base_path, std::string_view(target_name));
        }
      } else if (!path::is_ancestor(model.qualified_name(), target_name)) {
        // For absolute paths not under the model, prepend model name
        // Remove leading '/' from target_name before joining
        if (!target_name.empty() && target_name[0] == '/') {
          resolved_name =
              path::join(model.qualified_name(), target_name.substr(1));
        }
      } else {
        // Absolute path under model - use as is
        resolved_name = target_name;
      }

      transition->target = resolved_name;
    }
  }
};

// Partial trigger
struct PartialTrigger : Partial {
  std::vector<std::string> events;
  explicit PartialTrigger(std::vector<std::string> evts)
      : events(std::move(evts)) {}

  void apply(Model& /*model*/, std::vector<ElementInterface*>& stack) override {
    auto* transition = find_in_stack<Transition>(stack, Kind::Transition);
    if (transition) {
      transition->events = events;
    }
  }
};

// Partial guard
template <typename T>
struct PartialGuard : Partial {
  std::function<bool(Context&, T&, Event&)> func;

  // Constructor for function pointers
  explicit PartialGuard(bool (*cond)(Context&, T&, Event&))
      : func([cond](Context& ctx, T& instance, Event& event) {
          return cond(ctx, instance, event);
        }) {}

  // Constructor for std::function (lambdas)
  explicit PartialGuard(std::function<bool(Context&, T&, Event&)> f)
      : func(std::move(f)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* transition = find_in_stack<Transition>(stack, Kind::Transition);
    if (!transition || !func) return;

    std::string guard_name = path::join(transition->qualified_name(), "guard");

    auto constraint = std::make_unique<Constraint>(guard_name, func);

    model.set_member(guard_name, std::move(constraint));
    transition->guard = guard_name;
  }
};

// Partial effect
template <typename T>
struct PartialEffect : Partial {
  Action<T> action;
  std::function<void(Context&, T&, Event&)> func;

  explicit PartialEffect(Action<T> act) : action(act), func(nullptr) {}

  // Additional constructor for std::function (lambdas)
  explicit PartialEffect(std::function<void(Context&, T&, Event&)> f)
      : action(nullptr), func(std::move(f)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* transition = find_in_stack<Transition>(stack, Kind::Transition);
    if (!transition || (!action && !func)) return;

    std::string effect_name =
        path::join(transition->qualified_name(),
                   "effect_" + std::to_string(transition->effect.size()));

    std::unique_ptr<Behavior> behavior;
    if (action) {
      behavior = std::make_unique<Behavior>(effect_name, action);
    } else {
      behavior = std::make_unique<Behavior>(effect_name, func);
    }

    model.set_member(effect_name, std::move(behavior));
    transition->effect.push_back(effect_name);
  }
};

// Partial entry
template <typename T>
struct PartialEntry : Partial {
  static_assert(std::is_base_of<Instance, T>::value,
                "T must derive from Instance");
  std::vector<std::function<void(Context&, T&, Event&)>> actions;

  template <typename... Actions>
  explicit PartialEntry(Actions&&... acts) {
    (add_action(std::forward<Actions>(acts)), ...);
  }

 private:
  template <typename F>
  void add_action(F&& func) {
    if constexpr (std::is_same_v<std::decay_t<F>, Action<T>>) {
      actions.emplace_back(func);
    } else {
      actions.emplace_back(
          std::function<void(Context&, T&, Event&)>(std::forward<F>(func)));
    }
  }

 public:
  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* state = find_in_stack<State>(stack, Kind::State);
    if (!state || actions.empty()) return;

    for (size_t i = 0; i < actions.size(); ++i) {
      std::string entry_name = path::join(
          state->qualified_name(), "entry_" + std::to_string(i) + "_" +
                                       std::to_string(model.members.size()));

      auto behavior = std::make_unique<Behavior>(entry_name, actions[i]);
      model.set_member(entry_name, std::move(behavior));
      state->entry.push_back(entry_name);
    }
  }
};

// Partial exit
template <typename T>
struct PartialExit : Partial {
  static_assert(std::is_base_of<Instance, T>::value,
                "T must derive from Instance");
  std::vector<std::function<void(Context&, T&, Event&)>> actions;

  template <typename... Actions>
  explicit PartialExit(Actions&&... acts) {
    (add_action(std::forward<Actions>(acts)), ...);
  }

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* state = find_in_stack<State>(stack, Kind::State);
    if (!state || actions.empty()) return;

    for (const auto& action : actions) {
      std::string exit_name =
          path::join(state->qualified_name(),
                     "exit_" + std::to_string(model.members.size()));

      auto behavior = std::make_unique<Behavior>(exit_name, action);
      model.set_member(exit_name, std::move(behavior));
      state->exit.push_back(exit_name);
    }
  }

 private:
  template <typename Func>
  void add_action(Func&& func) {
    if constexpr (std::is_convertible_v<Func, Action<T>>) {
      actions.emplace_back(std::forward<Func>(func));
    } else if constexpr (std::is_invocable_v<Func, Context&, T&, Event&>) {
      actions.emplace_back(std::forward<Func>(func));
    } else {
      static_assert(std::is_invocable_v<Func, Context&, T&, Event&>,
                    "Function must be invocable with (Context&, T&, Event&)");
    }
  }
};

// Partial initial
struct PartialInitial : Partial {
  std::string name;
  std::vector<std::unique_ptr<Partial>> elements;

  explicit PartialInitial(std::string n) : name(std::move(n)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* owner = find_in_stack<State>(stack, Kind::State);
    if (!owner) return;

    std::string full_name = path::join(owner->qualified_name(), name);
    auto initial = std::make_unique<Vertex>(Kind::Initial, full_name);
    auto* initial_ptr = initial.get();

    model.set_member(full_name, std::move(initial));
    owner->initial = full_name;

    stack.push_back(initial_ptr);

    // Create initial transition
    auto transition =
        std::make_unique<PartialTransition>(".initial_transition");
    transition->elements.push_back(std::make_unique<PartialSource>(full_name));
    transition->elements.push_back(std::make_unique<PartialTrigger>(
        std::vector<std::string>{"hsm_initial"}));

    for (auto& element : elements) {
      transition->elements.push_back(std::move(element));
    }

    transition->apply(model, stack);
    stack.pop_back();
  }
};

// Partial choice
struct PartialChoice : Partial {
  std::string name;
  std::vector<std::unique_ptr<Partial>> elements;

  explicit PartialChoice(std::string n) : name(std::move(n)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* owner = find_in_stack<State>(stack, Kind::State);
    if (!owner) {
      return;
    }

    std::string full_name = path::join(owner->qualified_name(), name);
    auto choice = std::make_unique<Vertex>(Kind::Choice, full_name);
    auto* choice_ptr = choice.get();

    model.set_member(full_name, std::move(choice));
    stack.push_back(choice_ptr);

    for (auto& element : elements) {
      element->apply(model, stack);
    }

    stack.pop_back();
  }
};

// Time-based behavior partials
template <typename D, typename T>
class AfterBehavior : public Partial {
  static_assert(is_duration_v<D>, "D must be a duration");
  static_assert(std::is_base_of_v<Instance, T>, "T must be Instance");

 private:
  std::string event_name_;
  std::string transition_source_;
  TimeExpression<D, T> duration_func_;

 public:
  explicit AfterBehavior(std::string event_name, std::string transition_source,
                         TimeExpression<D, T> duration_func)
      : event_name_(std::move(event_name)),
        transition_source_(std::move(transition_source)),
        duration_func_(std::move(duration_func)) {}

  void apply(Model& model, std::vector<ElementInterface*>& /*stack*/) override {
    std::cerr << "DEBUG: AfterBehavior::apply called for transition_source: "
              << transition_source_ << std::endl;
    auto* source_state = model.get_member<State>(transition_source_);
    if (!source_state) {
      std::cerr << "DEBUG: AfterBehavior::apply - source_state not found!"
                << std::endl;
      return;
    }
    std::cerr << "DEBUG: AfterBehavior::apply - source_state found: "
              << source_state->qualified_name() << std::endl;

    // Create activity name
    std::string activity_name = std::string(source_state->qualified_name()) +
                                "/activity_" +
                                std::to_string(model.members.size());

    // Create the timer activity behavior
    auto timer_behavior = std::make_unique<Behavior>(
        activity_name,
        std::function<void(Context&, T&, Event&)>(
            [event_name = event_name_, duration_func = duration_func_](
                Context& signal, T& hsm, Event& event) {
              std::cerr << "DEBUG: Timer behavior started for event: "
                        << event_name << std::endl;

              // Calculate duration using the provided function
              auto duration = duration_func(signal, hsm, event);
              std::cerr << "DEBUG: Timer duration: " << duration.count() << "ms"
                        << std::endl;

              if (duration <= D(0)) {
                std::cerr << "DEBUG: Timer duration <= 0, returning"
                          << std::endl;
                return;
              }

              // Start timer
              std::cerr << "DEBUG: Timer starting sleep..." << std::endl;
              hsm.task_provider().sleep_for(duration);
              std::cerr << "DEBUG: Timer sleep completed" << std::endl;

              if (signal.is_set()) {
                std::cerr << "DEBUG: Signal is set, timer cancelled"
                          << std::endl;
                return;
              }

              // Only dispatch if we are still in the same state
              std::cerr << "DEBUG: Timer dispatching event via instance: "
                        << event_name << std::endl;
              Event time_event(event_name, Kind::TimeEvent);

              // Use Instance dispatch instead of HSM dispatch to avoid deadlock
              auto& ctx = hsm.Instance::dispatch(std::move(time_event));
              ctx.wait();
              std::cerr << "DEBUG: Timer event dispatched via instance"
                        << std::endl;
            }),
        Kind::Concurrent);

    std::cerr << "DEBUG: AfterBehavior::apply - creating activity: "
              << activity_name << std::endl;
    model.set_member(activity_name, std::move(timer_behavior));
    source_state->activities.push_back(activity_name);
    std::cerr << "DEBUG: AfterBehavior::apply - added activity to state, "
                 "activities count: "
              << source_state->activities.size() << std::endl;
  }
};

template <typename D, typename T>
class EveryBehavior : public Partial {
  static_assert(is_duration_v<D>, "D must be a duration");
  static_assert(std::is_base_of_v<Instance, T>, "T must be Instance");

 private:
  std::string event_name_;
  std::string transition_source_;
  TimeExpression<D, T> duration_func_;

 public:
  explicit EveryBehavior(std::string event_name, std::string transition_source,
                         TimeExpression<D, T> duration_func)
      : event_name_(std::move(event_name)),
        transition_source_(std::move(transition_source)),
        duration_func_(std::move(duration_func)) {}

  void apply(Model& model, std::vector<ElementInterface*>& /*stack*/) override {
    auto* source_state = model.get_member<State>(transition_source_);
    if (!source_state) {
      return;
    }

    // Create activity name
    std::string activity_name = std::string(source_state->qualified_name()) +
                                "/activity/" + event_name_;

    // Create the repeating timer activity behavior
    auto timer_behavior = std::make_unique<Behavior>(
        activity_name,
        std::function<void(Context&, T&, Event&)>(
            [event_name = event_name_, duration_func = duration_func_](
                Context& signal, T& hsm, Event& event) {
              // Calculate duration using the provided function
              auto duration = duration_func(signal, hsm, event);
              if (duration <= D(0)) {
                return;
              }

              while (!signal.is_set()) {
                hsm.task_provider().sleep_for(duration);

                if (signal.is_set()) {
                  break;
                }

                // Only dispatch if HSM is still running
                Event time_event(event_name, Kind::TimeEvent);
                hsm.dispatch(std::move(time_event)).wait();
              }
            }),
        Kind::Concurrent);

    model.set_member(activity_name, std::move(timer_behavior));
    source_state->activities.push_back(activity_name);
  }
};

// Time-based partial elements
template <typename D, typename T>
class PartialAfter : public Partial {
 private:
  TimeExpression<D, T> duration_func_;

 public:
  explicit PartialAfter(TimeExpression<D, T> duration_func)
      : duration_func_(std::move(duration_func)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    std::cerr << "DEBUG: PartialAfter::apply called" << std::endl;
    auto* transition = find_in_stack<Transition>(stack, Kind::Transition);
    if (!transition) {
      std::cerr << "DEBUG: No transition found in stack!" << std::endl;
      return;
    }

    std::cerr << "DEBUG: Found transition: " << transition->qualified_name()
              << std::endl;

    // Determine the source from the stack context since transition->source
    // isn't set yet
    std::string source_name;
    auto* owner = find_in_stack<Vertex>(stack, Kind::Vertex);
    if (owner) {
      source_name = std::string(owner->qualified_name());
      std::cerr << "DEBUG: Determined source from stack: " << source_name
                << std::endl;
    } else {
      std::cerr << "DEBUG: No owner found in stack!" << std::endl;
      return;
    }

    // Create unique event name for this timer
    std::string event_name = std::string(transition->qualified_name()) +
                             "_after_" + std::to_string(model.members.size());

    std::cerr << "DEBUG: Generated event name: " << event_name << std::endl;

    // Add the time event to the transition's events
    transition->events.push_back(event_name);

    // Create and add the AfterBehavior partial
    model.add(std::make_unique<AfterBehavior<D, T>>(event_name, source_name,
                                                    duration_func_));

    std::cerr << "DEBUG: Added AfterBehavior to owned_elements" << std::endl;
  }
};

template <typename D, typename T>
class PartialEvery : public Partial {
 private:
  TimeExpression<D, T> duration_func_;

 public:
  explicit PartialEvery(TimeExpression<D, T> duration_func)
      : duration_func_(std::move(duration_func)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* transition = find_in_stack<Transition>(stack, Kind::Transition);
    if (!transition) {
      return;
    }

    // Determine the source from the stack context since transition->source may
    // not be initialized yet
    std::string source_name;
    if (transition->source.empty()) {
      if (auto* owner = find_in_stack<Vertex>(stack, Kind::Vertex)) {
        source_name = std::string(owner->qualified_name());
      } else {
        source_name = std::string(model.qualified_name());
      }
    } else {
      source_name = transition->source;
    }

    // Create unique event name for this timer
    std::string event_name = std::string(transition->qualified_name()) +
                             "_every_" + std::to_string(model.members.size());

    // Add the time event to the transition's events
    transition->events.push_back(event_name);

    // Create and add the EveryBehavior partial
    model.add(std::make_unique<EveryBehavior<D, T>>(event_name, source_name,
                                                    duration_func_));
  }
};

// Time-based transition functions

// after creates a time-based transition that fires after a specified duration
template <typename D, typename T>
std::unique_ptr<Partial> after(TimeExpression<D, T> duration_func) {
  return std::make_unique<PartialAfter<D, T>>(std::move(duration_func));
}

// every creates a time-based transition that fires repeatedly at specified
// intervals
template <typename D, typename T>
std::unique_ptr<Partial> every(TimeExpression<D, T> duration_func) {
  return std::make_unique<PartialEvery<D, T>>(std::move(duration_func));
}

// Partial final state
struct PartialFinal : Partial {
  std::string name;

  explicit PartialFinal(std::string n) : name(std::move(n)) {}

  void apply(Model& model, std::vector<ElementInterface*>& stack) override {
    auto* owner = find_in_stack<State>(stack, Kind::State);
    if (!owner) {
      return;
    }

    std::string full_name = path::join(owner->qualified_name(), name);
    auto state = std::make_unique<State>(full_name);
    state->kind_ = Kind::FinalState;

    model.set_member(full_name, std::move(state));
  }
};

// Builder functions
template <typename... TPartials>
std::unique_ptr<PartialState> state(std::string name, TPartials&&... partials) {
  auto result = std::make_unique<PartialState>(std::move(name));
  (result->elements.push_back(std::forward<TPartials>(partials)), ...);
  return result;
}

inline std::unique_ptr<PartialFinal> final(std::string name) {
  return std::make_unique<PartialFinal>(std::move(name));
}

template <typename... TPartials>
std::unique_ptr<PartialTransition> transition(TPartials&&... partials) {
  auto result = std::make_unique<PartialTransition>("");
  (result->elements.push_back(std::forward<TPartials>(partials)), ...);
  return result;
}

template <typename... TPartials>
std::unique_ptr<PartialInitial> initial(TPartials&&... partials) {
  auto result = std::make_unique<PartialInitial>(".initial");
  (result->elements.push_back(std::forward<TPartials>(partials)), ...);
  return result;
}

template <typename... TPartials>
std::unique_ptr<PartialChoice> choice(std::string name,
                                      TPartials&&... partials) {
  auto result = std::make_unique<PartialChoice>(std::move(name));
  (result->elements.push_back(std::forward<TPartials>(partials)), ...);
  return result;
}

inline std::unique_ptr<PartialSource> source(std::string source_name) {
  return std::make_unique<PartialSource>(std::move(source_name));
}

inline std::unique_ptr<PartialTarget> target(std::string target_name) {
  return std::make_unique<PartialTarget>(std::move(target_name));
}

inline std::unique_ptr<PartialTrigger> on(std::string event_name) {
  return std::make_unique<PartialTrigger>(
      std::vector<std::string>{std::move(event_name)});
}

template <typename T>
inline std::unique_ptr<PartialEffect<T>> effect(Action<T> action) {
  return std::make_unique<PartialEffect<T>>(action);
}

template <typename T, typename... Actions>
inline std::unique_ptr<PartialEntry<T>> entry(Action<T> first,
                                              Actions... rest) {
  return std::make_unique<PartialEntry<T>>(first, rest...);
}

template <typename T, typename... Actions>
inline std::unique_ptr<PartialExit<T>> exit(Action<T> first, Actions... rest) {
  return std::make_unique<PartialExit<T>>(first, rest...);
}

template <typename T>
inline std::unique_ptr<PartialActivity<T>> activity(Action<T> action) {
  return std::make_unique<PartialActivity<T>>(action);
}

template <typename T>
inline std::unique_ptr<PartialGuard<T>> guard(bool (*condition)(Context&, T&,
                                                                Event&)) {
  return std::make_unique<PartialGuard<T>>(condition);
}

// Lambda overloads that automatically deduce template arguments

// Variadic overload for entry that accepts multiple callables
template <typename... Funcs>
inline auto entry(Funcs&&... funcs) -> std::enable_if_t<
    (std::is_invocable_v<Funcs, Context&, Instance&, Event&> && ...),
    std::unique_ptr<PartialEntry<Instance>>> {
  return std::make_unique<PartialEntry<Instance>>(
      std::forward<Funcs>(funcs)...);
}

// Template version for derived instance types
template <typename T, typename Func, typename... Funcs>
inline auto entry(Func&& func, Funcs&&... funcs)
    -> std::enable_if_t<std::is_base_of_v<Instance, T> &&
                            std::is_invocable_v<Func, Context&, T&, Event&>,
                        std::unique_ptr<PartialEntry<T>>> {
  return std::make_unique<PartialEntry<T>>(std::forward<Func>(func),
                                           std::forward<Funcs>(funcs)...);
}

// Variadic overload for exit that accepts multiple callables
template <typename... Funcs>
inline auto exit(Funcs&&... funcs) -> std::enable_if_t<
    (std::is_invocable_v<Funcs, Context&, Instance&, Event&> && ...),
    std::unique_ptr<PartialExit<Instance>>> {
  return std::make_unique<PartialExit<Instance>>(std::forward<Funcs>(funcs)...);
}

// Overload for activity that accepts stateless lambdas (converts to function
// pointer)
inline auto activity(void (*func)(Context&, Instance&, Event&)) {
  return std::make_unique<PartialActivity<Instance>>(func);
}

// Overload for activity that accepts any callable (including stateful lambdas)
template <typename F>
inline auto activity(F&& func) -> std::enable_if_t<
    std::is_invocable_v<F, Context&, Instance&, Event&> &&
        !std::is_convertible_v<F, void (*)(Context&, Instance&, Event&)>,
    std::unique_ptr<PartialActivity<Instance>>> {
  return std::make_unique<PartialActivity<Instance>>(
      std::function<void(Context&, Instance&, Event&)>(std::forward<F>(func)));
}

// Overload for effect that accepts stateless lambdas (converts to function
// pointer)
inline auto effect(void (*func)(Context&, Instance&, Event&)) {
  return std::make_unique<PartialEffect<Instance>>(func);
}

// Overload for effect that accepts any callable (including stateful lambdas)
template <typename F>
inline auto effect(F&& func) -> std::enable_if_t<
    std::is_invocable_v<F, Context&, Instance&, Event&> &&
        !std::is_convertible_v<F, void (*)(Context&, Instance&, Event&)>,
    std::unique_ptr<PartialEffect<Instance>>> {
  return std::make_unique<PartialEffect<Instance>>(
      std::function<void(Context&, Instance&, Event&)>(std::forward<F>(func)));
}

// Overload for guard that accepts stateless lambdas (converts to function
// pointer)
inline auto guard(bool (*func)(Context&, Instance&, Event&)) {
  return std::make_unique<PartialGuard<Instance>>(func);
}

// Overload for guard that accepts any callable (including stateful lambdas)
template <typename F>
inline auto guard(F&& func) -> std::enable_if_t<
    std::is_invocable_v<F, Context&, Instance&, Event&> &&
        !std::is_convertible_v<F, bool (*)(Context&, Instance&, Event&)>,
    std::unique_ptr<PartialGuard<Instance>>> {
  return std::make_unique<PartialGuard<Instance>>(
      std::function<bool(Context&, Instance&, Event&)>(std::forward<F>(func)));
}

// Partial defer
struct PartialDefer : Partial {
  std::vector<std::string> event_names;

  explicit PartialDefer(std::vector<std::string> names)
      : event_names(std::move(names)) {}

  void apply(Model& /*model*/, std::vector<ElementInterface*>& stack) override {
    auto* state = find_in_stack<State>(stack, Kind::State);
    if (!state) return;

    // Add event names to the deferred array
    for (const auto& event_name : event_names) {
      state->deferred.push_back(event_name);
    }
  }
};

inline std::unique_ptr<PartialDefer> defer(std::string event_name) {
  return std::make_unique<PartialDefer>(
      std::vector<std::string>{std::move(event_name)});
}

template <typename... TEventNames>
inline std::unique_ptr<PartialDefer> defer(std::string first_event,
                                           TEventNames&&... event_names) {
  std::vector<std::string> names;
  names.push_back(std::move(first_event));
  (names.push_back(std::forward<TEventNames>(event_names)), ...);
  return std::make_unique<PartialDefer>(std::move(names));
}

template <typename... TPartials>
std::unique_ptr<Model> define(std::string name, TPartials&&... partials) {
  auto model = std::make_unique<Model>(path::join("/", std::move(name)));
  std::vector<ElementInterface*> stack;
  stack.push_back(model.get());

  (partials->apply(*model, stack), ...);

  // Process owned_elements (similar to Python's while loop)
  while (!model->owned_elements.empty()) {
    // Move all elements to a temporary vector to avoid iterator invalidation
    std::vector<std::unique_ptr<Partial>> to_process;
    to_process.swap(model->owned_elements);

    // Apply each partial
    for (auto& partial : to_process) {
      partial->apply(*model, stack);
    }
  }
  buildTransitionTable(*model);
  buildDeferredTable(*model);
  return model;
}

// Global stop function for convenience, similar to the Go version
inline Context& stop(Instance& instance) { return instance.__hsm->stop(); }

inline void start(Instance& instance, std::unique_ptr<Model>& model) {
  auto hsm = new HSM(instance, model);
  hsm->start().wait();
}

}  // namespace hsm
