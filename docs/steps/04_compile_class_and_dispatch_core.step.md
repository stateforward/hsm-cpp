## Step 4 – `compile` class and dispatch core

*Note: If new `*.step.md` files are needed for this phase, create them immediately after this file and name them using the pattern `{{step number}}.{{sub step}}_{{title}}.step.md` (for this step, `step number` is `04`).*

* [x] **Verify previous steps and establish baseline**
  * Confirm all TODOs in `01_project_scaffolding_and_build_integration.step.md`, `02_meta_model_and_compile_time_strings.step.md`, and `03_define_and_model_normalization.step.md` are checked off.
  * Run the current C++ test suite and ensure all tests pass before modifying the dispatch core.

* [x] **Design the `cthsm::compile<ModelExpr, Instance>` interface**
  * Finalize the public API for `cthsm::compile<ModelExpr, Instance = cthsm::Instance>` (constructors, `state()`, `dispatch(...)`, optional `stop()` / `reset()`) in `cthsm/cthsm.hpp`.
  * Ensure the interface mirrors the spirit of the `hsm::HSM`/`Instance` interaction while remaining simple and predictable for embedded systems.

* [x] **Implement state/event/transition tables**
  * Using the normalized meta-model from `cthsm::define`, generate `constexpr` tables for:
    * mapping event names to event IDs,
    * per-state/per-event transition selection order (nearest ancestor first),
    * per-state deferred event flags.
  * Ensure these tables are stored as `constexpr` `std::array` (or equivalent) and are indexable in O(1) time.

* [x] **Implement the dispatch algorithm**
  * Implement `compile::dispatch(std::string_view event_name)` using:
    * event-name→ID lookup,
    * state/event→transition lookup via precomputed tables,
    * guard evaluation, and
    * execution of entry/exit/effect behaviors via indices that will later be wired to concrete callables.
  * Handle deferred events according to the design (skip transition lookup when deferred, re-enqueue on state change).

* [x] **Implement initial state resolution**
  * Implement logic in the `compile` constructor (or a `start()`-like helper) that uses the meta-model to:
    * determine the initial active state (following initial pseudo-states and transitions), and
    * execute the correct sequence of entry actions.
  * Ensure this logic is purely driven by the precomputed meta-model, with no dynamic graph traversal at runtime.

* [x] **Add tests for dispatch and state resolution**
  * Add focused tests that:
    * define small example state machines via `cthsm::define`,
    * instantiate `cthsm::compile` with a test instance type,
    * verify initial state names and transitions taken on specific events (including basic internal/external/self cases) match the equivalent `hsm` behavior.
  * Run the full C++ test suite and confirm zero regressions against the prior baseline.

* [x] **Finalize this step**
  * Once all tasks above are complete and all tests pass with zero regressions, check off every TODO in this step file.
