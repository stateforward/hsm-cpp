## cthsm – Compile‑Time HSM Design

### High‑level goals

* **Same DSL, different namespace**
  * All user‑facing builders and concepts from `hsm` are mirrored in `cthsm` with the **same call‑site syntax**, only changing the namespace:
    * `hsm::define` → `cthsm::define`
    * `hsm::state`, `hsm::initial`, `hsm::choice`, `hsm::final` → `cthsm::state`, `cthsm::initial`, `cthsm::choice`, `cthsm::final`
    * `hsm::transition`, `hsm::source`, `hsm::target`, `hsm::on` → `cthsm::transition`, `cthsm::source`, `cthsm::target`, `cthsm::on`
    * `hsm::entry`, `hsm::exit`, `hsm::activity`, `hsm::effect`, `hsm::guard`, `hsm::defer` → `cthsm::entry`, `cthsm::exit`, `cthsm::activity`, `cthsm::effect`, `cthsm::guard`, `cthsm::defer`
    * time‑based API: `hsm::after`, `hsm::every` → `cthsm::after`, `cthsm::every`.
  * User test code should be able to switch from `hsm` to `cthsm` by:
    * changing `namespace` references from `hsm` → `cthsm`, and
    * swapping the runtime `start/stop` API for the compile‑time `compile`/`dispatch` API where needed.

* **Lift as much as possible to compile time**
  * The **graph structure** (states, transitions, initial/choice/final pseudo‑states, deferral sets, transition kinds, enter/exit paths) is computed at compile time.
  * **Lookup structures** for dispatch (transition selection and deferral checking) are generated as `constexpr` tables, not built with `std::unordered_map` at runtime.
  * **Behavior wiring** (which actions/guards/effects belong to which transitions and states) is decided at compile time; actual callable invocations remain runtime.

* **Preserve semantics where practical**
  * State naming and qualified **absolute** paths and the semantics tested in the current `tests/*` should remain valid under `cthsm` (when rewritten to use absolute paths).
  * Event deferral, internal vs external vs self transitions, choice states, and time events (`after`, `every`) should behave equivalently, modulo clearly documented compile‑time restrictions.

* **Real‑time / embedded friendly**
  * `cthsm` targets resource‑constrained, real‑time embedded systems: the engine avoids dynamic allocation and `std::function`/`std::any`/`std::unordered_map` in the hot path, relying instead on `constexpr` tables and fixed‑size storage.
  * Dispatch and deferral checks are designed to be O(1) or tightly bounded and predictable, making it suitable for hard/soft real‑time tasks while preserving the same high‑level DSL syntax as `hsm`.

* **RTOS compatibility (including FreeRTOS)**
  * Concurrency, timers, and sleep/yield behavior are exposed through a small, pluggable abstraction (similar in spirit to `TaskProvider`) so that `cthsm` can run both on bare‑metal systems and under RTOSes such as FreeRTOS.
  * The core library does not depend on `std::thread` or OS‑specific primitives; instead, users can provide an adapter that maps `cthsm`’s scheduling hooks onto FreeRTOS APIs (`xTaskCreate`, `vTaskDelay`, timers, queues, etc.) when needed.

* **No change to user action/guard syntax**
  * Callables passed to `entry/exit/effect/activity/guard/after/every` keep the same shape:\
    `[](hsm::Context&, MyInstance&, hsm::Event&) { ... }` → `[](cthsm::Context&, MyInstance&, cthsm::Event&) { ... }`.
  * Both function pointers and lambdas (including capturing lambdas) must be supported.

### Non‑goals / acceptable differences

* **Not required to expose a mutable runtime `Model`**
  * `hsm` builds a heap‑allocated `Model` with `std::string` keys and `std::unordered_map`‑based lookup; `cthsm` is free to use purely type‑based and `constexpr` data instead.
  * Deep reflection and mutation of the model at runtime is out of scope for `cthsm`.

* **Some dynamic flexibility may be reduced**
  * `hsm` can in principle accept arbitrary runtime event names (`hsm::Event{"foo"}`); `cthsm` focuses on **statically known event names** used in `on("EVENT")` and timers.
  * Non‑literal event names can be supported via a slower fallback path or left unsupported, as long as this is documented.

* **No requirement to mirror all threading policies**
  * `hsm` has `TaskProvider` and threaded activities; `cthsm` may initially support a subset or provide a simpler policy-based extension point for concurrency.

## Benchmark Results

### Performance Comparison

The following benchmarks compare `cthsm` (compile-time) against `hsm` (runtime) on a MacBook Pro (Apple Silicon).

| Scenario | `hsm` (trans/sec) | `cthsm` (trans/sec) | Speedup |
|----------|------------------:|--------------------:|--------:|
| **Baseline** (Nested states, no behaviors) | 328,299 | 4,733,728 | **14.4x** |
| **1.a** (With entry behaviors) | 304,136 | 4,299,226 | **14.1x** |
| **1.b** (Entry + Activity*) | 35,535 | 2,221,975 | **62.5x** |
| **1.c** (Entry + Exit + Activity*) | 32,815 | 1,226,091 | **37.4x** |
| **1.d** (Full: Entry/Exit/Activity/Effect) | 35,826 | 2,095,557 | **58.5x** |

*\*Note on Activities: `hsm` spawns a `std::thread` for each activity, incurring significant overhead. `cthsm` invokes the activity callable synchronously/inline. Users requiring concurrency in `cthsm` must offload work to a scheduler manually or via a custom adapter.*

### Binary Size Comparison

Comparison of benchmark executables compiled with `-O3` (or equivalent default release settings):

| Artifact | Size (Bytes) | Size (MB) | Reduction |
|----------|-------------:|----------:|----------:|
| `hsm` benchmark | 1,822,120 | 1.74 MB | - |
| `cthsm` benchmark | 1,599,728 | 1.53 MB | **~12%** |

`cthsm` achieves a modest reduction in binary size despite heavy template usage, likely due to the removal of `std::function`, `std::any`, `std::string`, and `std::unordered_map` overheads from the hot path.

### Test Parity Status

All core features have been ported and verified with equivalent behavior (modulo intended API differences):

* [x] **Transitions**: Internal, External, Self, Local, Initial.
* [x] **Guards & Effects**: Full support.
* [x] **Behaviors**: Entry, Exit, Activities (synchronous).
* [x] **Pseudostates**: Initial, Choice, Final.
* [x] **Path Resolution**: Absolute, Relative (limited), Ancestor lookups.
* [x] **Deferral**: Event deferral and re-dispatch.
* [x] **Timers**: `after` and `every` (simulated/synchronous).

***

## Public API design

### Namespaces and core types

* **Namespace**: everything lives under `cthsm`.
* **Core concepts** (mirroring `hsm`):
  * `cthsm::Instance` – base class for user instances (equivalent role to `hsm::Instance`).
  * `cthsm::Context` – synchronization / cancellation context for behaviors and timers.
  * `cthsm::Event` – lightweight runtime event wrapper; holds a name and optional payload (same conceptual API as `hsm::Event`, but backed by compile‑time metadata where possible).
  * `cthsm::Action<T>`, `cthsm::Condition<T>`, `cthsm::TimeExpression<D, T>` – same function signatures as in `hsm`, but not wrapped in `std::function` inside the core meta‑model.

### DSL builders

All builders preserve argument order and meaning; only the internal representation changes.

* **Structure builders**
  * `cthsm::state(std::string name, Partials&&...)`
  * `cthsm::initial(Partials&&...)`
  * `cthsm::choice(std::string name, Partials&&...)`
  * `cthsm::final(std::string name)`

* **Transition builders**
  * `cthsm::transition(Partials&&...)`
  * `cthsm::source(std::string source_name)`
  * `cthsm::target(std::string target_name)`
  * `cthsm::on(std::string event_name)`

* **Behavior / decoration builders**
  * `cthsm::entry(...)`, `cthsm::exit(...)`, `cthsm::activity(...)`
  * `cthsm::effect(...)`, `cthsm::guard(...)`
  * `cthsm::defer(...)`
  * `cthsm::after<D, T>(TimeExpression<D, T>)`, `cthsm::every<D, T>(TimeExpression<D, T>)`

* **Top‑level model builder**
  * `auto model = cthsm::define("Name", Partials&&...);`
    * Same parameter list as `hsm::define`, but returns a **compile‑time model descriptor type**, not a `std::unique_ptr<Model>`.

### Compile step and runtime machine

* **Compile‑time compilation entry point**

```cpp
auto model = cthsm::define(
    "light",
    cthsm::initial(cthsm::target("off")),
    cthsm::state("off",
                 cthsm::transition(cthsm::on("turn_on"), cthsm::target("on"))),
    cthsm::state("on",
                 cthsm::transition(cthsm::on("turn_off"), cthsm::target("off"))));

// Default Instance base
cthsm::compile<decltype(model)> sm;
```

* **`cthsm::compile` primary template**
  * `template <typename ModelExpr, typename Instance = cthsm::Instance> struct compile;`
  * `ModelExpr` is the (deduced) type of the object returned by `cthsm::define`.
  * `compile` owns the runtime state for a single SM instance (current active state, any timers/activities, and any captured callables).

* **Runtime interface of `compile<ModelExpr, Instance>`**
  * `std::string_view state() const;` – returns the current qualified state name (same format as `hsm::Instance::state()`).
  * `void dispatch(std::string_view event_name);` – minimal, string‑based event dispatch.
  * Optionally a richer overload taking a `cthsm::Event` if data payloads are needed.
  * Optional `stop()` / `reset()` semantics if needed to mirror `hsm::start`/`stop`.

***

## Internal representation

### Type‑level model (meta‑graph)

* **Model expression types**
  * Each DSL builder in `cthsm` returns a lightweight, trivially copyable **expression node** type rather than a `std::unique_ptr<Partial>`.
  * Example shapes (names are illustrative only):
    * `state_expr<NameString, Partials...>`
    * `transition_expr<Partials...>`
    * `initial_expr<Partials...>`
    * `choice_expr<NameString, Partials...>`
    * `entry_expr<Callable...>`, `exit_expr<Callable...>`, `effect_expr<Callable...>`, `guard_expr<Callable>`
    * `target_expr<NameString>`, `on_expr<EventNameString>`, `defer_expr<EventNameStrings...>`.

* **Compile‑time strings**
  * String arguments (`"state"`, `"event"`, absolute paths) become non‑type template parameters using a `fixed_string` helper (C++20 NTTP for `char` arrays).
  * Basic path helpers for absolute paths (e.g., `normalize`, `is_absolute`) are re‑implemented in a `constexpr` manner for `fixed_string`; relative path resolution (`.`, `..`, `../sibling`, etc.) is intentionally not supported in `cthsm` to keep the compile‑time model simpler.

* **Normalized graph**
  * `cthsm::define` takes the raw expression tree and produces a canonical **meta‑model**:
    * A typelist of `state_desc` types, each with:
      * `id` (compile‑time integer), `qualified_name` (`fixed_string`), parent `id`, flags (initial/final/choice), and sets of attached behaviors and deferred events.
    * A typelist of `transition_desc` types, each with:
      * source state id, target state id (or sentinel for internal), kind (external/self/internal/local), event id, guard id/none, effect ids, and precomputed entry/exit paths (lists of state ids).
    * A typelist of `event_desc` types, each mapping a unique event name string to an integer id.

### IDs and lookup tables

* **State / event / transition IDs**
  * States, events, and transitions are assigned consecutive integer IDs at compile time (via index‑in‑typelist).
  * These IDs are used to index into `constexpr std::array` tables embedded in `compile<ModelExpr>`.

* **Dispatch tables**
  * For each state id and event id, we precompute the ordered list of enabled transitions (hierarchy‑aware, nearest ancestor first), mirroring the semantics of `buildTransitionTable` in `hsm.hpp`.
  * This is represented as `constexpr` arrays, e.g.:
    * `constexpr std::array<transition_index, NumStateEventPairs>` plus metadata to locate the range for `(state_id, event_id)`.
  * Deferred events per state are similarly represented as a bitset or `constexpr std::array<bool, NumEvents>`.

### Behaviors (entry/exit/effects/guards/activities)

* **Compile‑time association, runtime storage**
  * Each behavior expression stores the **callable type** as a template parameter (`F`) rather than erasing it into `std::function`.
  * The `compile<ModelExpr, Instance>` object holds instances of these callables as data members (possibly grouped into tuples per state/transition).
  * The meta‑model links state/transition IDs to indices in those tuples, so dispatch can call them by direct, non‑virtual, non‑`std::function` invocation.

* **Capturing lambdas**
  * Capturing lambdas are supported by value: they become members of `compile<ModelExpr, Instance>`.
  * References captured by lambdas are the user’s responsibility (same as any other object lifetime), but the syntax of guards/actions remains unchanged.

### Time‑based behaviors (`after` / `every`)

* `cthsm::after` and `cthsm::every` behave like their `hsm` counterparts but use the compile‑time model to:
  * attach timer behaviors to specific source states, and
  * generate dedicated timer event ids and transitions.
  * The runtime implementation can still be built on a `TaskProvider`‑like concept, but the mapping from timer to event and state is precomputed.

### Deferred events

* Deferred event sets for each state are computed at compile time from `defer(...)` expressions.
* At runtime, checking whether an event is deferred in a given state reduces to a simple indexed lookup (bitset or `bool` array), with no `std::unordered_map`.

***

## Execution algorithm

### Initialization

* `compile<ModelExpr, Instance>` constructs its behavior objects and initializes runtime state to the model’s root.
* The initial transition is computed entirely at compile time (which state to start in, which entry actions to run); runtime `start` simply executes the precomputed entry path.

### Dispatch

* `dispatch(event_name)` performs:
  * Map `event_name` → `event_id` via a small `constexpr` lookup (e.g., linear search over a short array or a perfect hash/table if needed).
  * Use `(current_state_id, event_id)` to locate candidate transitions via the precomputed table.
  * For each candidate transition in priority order:
    * Evaluate its guard callable (if any); on success:
      * Execute precomputed exit path (calling exit behaviors and stopping activities).
      * Execute effects.
      * Execute precomputed enter path (entry behaviors, starting activities, following nested initial transitions when appropriate).
  * Update `current_state_id` accordingly.

### Deferred events semantics

* Before looking up transitions, check the deferred flag for `(current_state_id, event_id)`:
  * If deferred, push the event into a per‑instance deferred queue (or buffer) without doing any transition lookup.
  * When a state change occurs, immediately re‑enqueue deferred events for re‑processing, using the same transition tables, matching `hsm`’s semantics.

***

## Compatibility and differences vs `hsm`

### Intended equivalence

* **Path semantics**
  * Absolute and relative paths, `.` / `..`, and hierarchical LCA logic follow `hsm::path` precisely, just in `constexpr` form.
  * Tests like `transition_test.cpp` should be portable to `cthsm` by only changing the namespace and the machine construction/dispatch API.

* **Transition kinds**
  * External, internal, self, and local transitions are encoded as a `kind` enum in the meta‑model, with the same behavior as in `hsm::Transition` and `HSM::transition`.

* **Ordering and priorities**
  * Multiple transitions for the same `(state, event)` follow the same priority rules as `buildTransitionTable` (nearest ancestor first, definition order preserved where relevant).

### Acceptable behavioral differences (documented)

* Events not declared via `on("EVENT")` or timers may be treated as “no transition” without wildcard support in the first version of `cthsm`.
* Extremely dynamic usage patterns (building event names at runtime and expecting wildcard matching) may not be supported in the compile‑time engine; such use cases can remain on `hsm`.

***

## Implementation plan (high level)

### File layout

* **Headers**: all public and internal headers for `cthsm` live under `include/cthsm/` (for example `include/cthsm/cthsm.hpp`, `include/cthsm/detail/...`).
* **Sources**: any non‑template or heavy implementation code that cannot be `constexpr`/header‑only lives under `src/cthsm/` (for example `src/cthsm/cthsm.cpp`), keeping the rest of the engine usable as a header‑only library for embedded targets.

### Phase 1 – Minimal compile‑time core

* Implement expression node types and `cthsm::define` that builds a type‑level meta‑model for:
  * states, transitions, initial/final/choice nodes, and basic entry/exit/effect/guard/defer.
* Implement `compile<ModelExpr, Instance>` with:
  * state/event IDs, precomputed transition tables, and a basic `dispatch(std::string_view)` API.
* Port a small subset of tests (simple transitions, path resolution, basic entry/exit/effect) to validate semantics.

### Phase 2 – Advanced features

* Add support for choice states, internal/self transitions, and hierarchical transitions mirroring all existing `transition_test` cases.
* Implement deferred events and verify against `deferred_map_test` and `transition_map_test`.
* Add `after` and `every` timers using a pluggable policy for sleeping and threading (similar to `TaskProvider`).

### Phase 3 – Performance and ergonomics

* Optimize compile‑time algorithms (typelist processing, ID assignment) to keep compile times reasonable.
* Consider optional debug/introspection APIs that can emit a runtime `hsm::Model` view from the compile‑time meta‑model for tooling and visualization.
* Document limitations and migration guidelines from `hsm` to `cthsm`.
