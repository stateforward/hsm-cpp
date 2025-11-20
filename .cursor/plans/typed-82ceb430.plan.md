<!-- 82ceb430-a83a-4f9b-8697-3df248e89e29 b1192685-a603-4611-815f-e383d54b6877 -->
# Add typed event support with CRTP Event

### Goals

- Add a strongly-typed event mechanism so that:
- Users can define events as `struct FooEvent : cthsm::Event<FooEvent> {};`.
- `on<FooEvent>()` and `dispatch<FooEvent>(inst)` work and are type-checked at compile time.
- Behavior functions (entry/exit/effect/activity/guards/timers) can take either `cthsm::Event` or a specific `FooEvent` as their event parameter.
- Only events present in the model are allowed (unknown event types are a compile-time error).
- The existing string-based API (`on("start")`, `dispatch(inst, "start")`) continues to work.

### Design Overview

- Introduce a CRTP base `template<class Derived> struct Event` that:
- Provides a `static constexpr auto name` member used by the existing `HasName`-based APIs.
- Derives `name` from the *type name* of `Derived` via a `consteval` helper that extracts a string from `__PRETTY_FUNCTION__` / compiler equivalents.
- Reuse the existing `HasName` concept so that any type with a suitable `name` (including but not limited to `Event<Derived>`) can be used with `on<T>()` and typed `dispatch<T>()`.
- Add a compile-time mapping from event names in the normalized model to event IDs to enable `dispatch<FooEvent>` to fail at compile time if `FooEvent` is not part of the model.
- Extend the behavior invocation pipeline so that, when a typed event is dispatched, user-provided behaviors can choose to accept either `cthsm::Event` or the concrete `FooEvent` type and the library will route the call accordingly.

### Concrete Steps

- **Step 1: Implement `Event<Derived>` CRTP base**
- Add `template<class Derived> struct Event` in `cthsm.hpp` (or a small header) that:
- Uses a `consteval` helper `detail::type_name_fixed<Derived>()` to produce a `detail::fixed_string` representing the fully qualified type name.
- Exposes `static constexpr auto name = detail::type_name_fixed<Derived>();` so it satisfies `HasName`.
- Implement `detail::type_name_fixed<T>()` using compiler-specific `__PRETTY_FUNCTION__` / `__FUNCSIG__` parsing to extract the type name into a `std::array<char, N>` and then convert that to `fixed_string` via `make_fixed_string_from_array`.
- Document that this relies on well-known compiler extensions but is fully `consteval` and stable for typical compilers (Clang/GCC/MSVC).

- **Step 2: Adjust `on<HasName T>()` to support CRTP events**
- Update the `HasName`-based `on()` overload in `cthsm.hpp` to:
- Detect when `T::name` is already a `fixed_string` (e.g. from `Event<Derived>`) and call `detail::on_expr` directly with that value.
- Fall back to the existing `make_fixed_string(T::name)` path when `T::name` is a string literal (char array).
- This makes `on<FooEvent>()` work seamlessly when `FooEvent` derives from `Event<FooEvent>` and still supports existing event-tag structs with `static constexpr char name[] = "...";`.

- **Step 3: Build a consteval mapping from event names to IDs**
- Inside `compile<Model,...>`, after `normalized_model` and `tables` are computed, add:
- A `constexpr` array of event names (`std::string_view`) sourced from `normalized_model.get_event_name(i)`.
- A `template<class E> consteval std::size_t event_id_for()` that:
- Produces a `std::string_view` from `E::name` (works for `Event<E>` and legacy tags).
- Linearly scans the compile-time event name array; if a match is found, returns that ID.
- If no match is found, triggers a `static_assert` with a clear message like "Event type E is not present in the model".

- **Step 4: Add typed `dispatch<E>` API**
- Add `template<HasName E> constexpr void dispatch(instance_type& instance) noexcept` to `compile`:
- `constexpr auto id = event_id_for<E>();` (consteval mapping from Step 3).
- Construct `Context ctx{}; Event base{normalized_model.get_event_name(id)};` and, for typed behavior support, also default-construct a `E ev{}`.
- Call into an overloaded `dispatch_event_impl` that knows both the generic `Event` and the optional typed event `E`, and reuses the existing transition/defer/completion logic.
- Keep the existing overloads `dispatch(instance, std::string_view)` and `dispatch<HasName E>(instance, const E&)` for dynamic/legacy usage.

- **Step 5: Integrate typed events into behavior invocation (non-optional)**
- Extend the internal `invoke` helper and behavior thunks so that they consider both `cthsm::Event` and (when available) the concrete event type `E`:
- For typed dispatch, pass both a `const Event&` and a `const E&` through the pipeline.
- In `invoke`, prefer calling signatures like `F(Context&, instance_type&, const E&)` or `F(instance_type&, const E&)` when they are well-formed.
- If those are not available, fall back to the existing variants taking `const Event&`, `instance_type&`, or no arguments.
- Ensure this applies consistently to all behavior categories: entry, exit, activity, guard, effect, and timer callbacks.
- For eventless paths (e.g. initial transitions, completion, timers that don’t correspond to a user event), continue to pass a generic `cthsm::Event` with an empty name; behaviors can still opt-in to typed event parameters but will only see concrete types when a real event is dispatched.

- **Step 6: Tests and documentation**
- Add new tests (e.g. `cthsm_typed_event_test.cpp`) that:
- Define `struct Start : cthsm::Event<Start> {};` and build a machine using `on<Start>()` transitions.
- Use behaviors that take `const cthsm::Event&` and others that take `const Start&`, verifying that both are invoked correctly via `sm.dispatch<Start>(inst);`.
- (Optionally, under a disabled block) show a test using a non-model event type that fails to compile, documenting the expected static-assert message.
- Ensure all existing tests (string-based events and `dispatch(inst, "...")`) still compile and pass.
- Add a short section to the docs or README showing how to define CRTP events and use `on<FooEvent>()`, `dispatch<FooEvent>()`, and event-typed behaviors.

### Todos

- `event-crtp-base`: Implement `Event<Derived>` and `detail::type_name_fixed<T>()` so events can derive names from their type names.
- `on-hasname-update`: Update `on<HasName T>()` to support `Event<Derived>`-style fixed_string names and legacy char-array names.
- `event-id-mapping`: Add consteval mapping from event names in `normalized_model` to event IDs with `static_assert` on unknown event types.
- `typed-dispatch-api`: Add typed `dispatch<E>(instance)` overload that uses event-id mapping and reuses existing dispatch logic.
- `typed-behavior-invoke`: Extend the behavior invocation pipeline so callbacks can accept either `cthsm::Event` or a concrete event type `E`, preferring the most specific match.
- `tests-and-docs`: Add tests for CRTP events, typed dispatch, and typed behaviors, and update docs while keeping existing behavior intact.

### To-dos

- [ ] Introduce an EventTag concept/trait and helper to obtain an event name from a type.
- [ ] Add typed on<E>() overload using the new event trait, keeping existing string-based on() working.
- [ ] Build compile-time mapping from event types to event IDs inside compile<>, with consteval lookup that static_asserts on unknown events.
- [ ] Add typed dispatch<E>(instance) overload that uses the new mapping and reuses existing dispatch implementation.
- [ ] Add tests for typed events and briefly document new API while ensuring existing tests still pass.