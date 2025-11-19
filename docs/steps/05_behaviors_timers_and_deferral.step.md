## Step 5 – Behaviors, timers, and deferral

*Note: If new `*.step.md` files are needed for this phase, create them immediately after this file and name them using the pattern `{{step number}}.{{sub step}}_{{title}}.step.md` (for this step, `step number` is `05`).*

* [x] **Verify previous steps and establish baseline**
  * Confirm all TODOs in `01_project_scaffolding_and_build_integration.step.md` through `04_compile_class_and_dispatch_core.step.md` are checked off.
  * Run the current C++ test suite to ensure a clean, passing baseline before adding behaviors, timers, and deferral logic.

* [x] **Wire entry/exit/effect/activity behaviors**
  * Extend the meta-model to assign stable indices for entry, exit, effect, and activity behaviors per state/transition.
  * In `cthsm::compile`, store concrete callable instances (function pointers or lambdas) in fixed-size tuples or arrays, indexed by these IDs, avoiding `std::function`.
  * Implement helpers in `compile` to invoke the appropriate behaviors during state entry/exit and transition execution, mirroring `hsm` semantics.

* [x] **Implement guards**
  * Extend the meta-model to record which guard (if any) is attached to each transition.
  * In `compile::dispatch`, evaluate guards via their indices before taking transitions, ensuring their invocation signature matches the `cthsm` context/instance/event types.

* [x] **Implement `after` and `every` timers**
  * Implement the compile-time representation for `after` and `every` (e.g. timer descriptors attached to source states and generated time-event IDs).
  * Provide a pluggable, embedded-friendly timing mechanism (e.g. via a `TaskProvider`-like policy or a user-supplied scheduler) that wakes up and dispatches timer events according to the computed durations.
  * Define at least one adapter interface suitable for RTOS environments (including FreeRTOS), so that timer scheduling and wakeups can be mapped onto RTOS primitives (tasks, timers, delays) without changing the `cthsm` DSL.
  * Ensure timers can be cancelled or stopped when leaving states, following the semantics in `cthsm.design.md`.

* [x] **Implement deferral semantics**
  * Use the compile-time deferred event sets to quickly determine when an event is deferred in `compile::dispatch`.
  * Maintain a per-instance deferred event buffer and ensure events are re-enqueued and re-processed when the active state changes, matching the `hsm` behavior.

* [x] **Add comprehensive behavior/timer/deferral tests**
  * Add tests that exercise:
    * entry/exit/effect ordering for transitions (including internal vs external vs self),
    * basic and composite state activities,
    * guarded transitions and fallbacks,
    * `after` and `every` timers (using a deterministic or fake scheduler where needed),
    * deferral behavior for various states and events.
  * Mirror a representative subset of the existing `hsm` tests (e.g. guard conditions, timer transitions, deferral tests) using `cthsm`, and ensure that all expected behaviors match.
  * Run the full C++ test suite and confirm zero regressions compared to the previous baseline.

* [x] **Finalize this step**
  * Once all tasks above are complete and all tests pass with zero regressions, check off every TODO in this step file.
