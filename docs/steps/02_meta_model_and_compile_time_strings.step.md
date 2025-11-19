## Step 2 – Meta-model and compile-time strings

*Note: If new `*.step.md` files are needed for this phase, create them immediately after this file and name them using the pattern `{{step number}}.{{sub step}}_{{title}}.step.md` (for this step, `step number` is `02`).*

* [x] **Verify previous steps and establish baseline**
  * Confirm all TODOs in `01_project_scaffolding_and_build_integration.step.md` are checked off.
  * Run the current C++ test suite to ensure you are starting from a green baseline with no regressions.

* [x] **Introduce `fixed_string` and type-level identifiers**
  * Implement a `cthsm::detail::fixed_string` (or equivalent) to represent compile-time strings as non-type template parameters (NTTPs).
  * Provide helper aliases or concepts for state names, event names, and absolute paths using `fixed_string`.
  * Ensure the implementation is compatible with the target C++ standard and embedded toolchains.

* [x] **Implement basic absolute path helpers**
  * Implement `constexpr` helpers for absolute paths only (e.g. `is_absolute`, `normalize`, possibly a basic parent/child relationship helper) in `cthsm::detail`, mirroring only the subset of `hsm::path` needed for absolute paths.
  * Explicitly **do not** support relative path resolution (`.`, `..`, `../sibling`, etc.) in `cthsm`; add `static_assert` or equivalent checks to reject such inputs at compile time where practical.

* [x] **Define expression node types for the DSL**
  * Design and implement expression node templates that mirror the `hsm` partials (e.g. `state_expr`, `transition_expr`, `initial_expr`, `choice_expr`, `final_expr`, `entry_expr`, `exit_expr`, `effect_expr`, `guard_expr`, `on_expr`, `target_expr`, `defer_expr`).
  * Ensure these node types are trivially copyable, lightweight, and purely compile-time descriptors, with no dynamic memory allocations.

* [x] **Hook up DSL builders to expression nodes**
  * Implement the `cthsm` DSL builder functions (`state`, `transition`, `initial`, `choice`, `final`, `entry`, `exit`, `effect`, `guard`, `on`, `target`, `defer`, `after`, `every`) so that they return the new expression node types instead of runtime `Partial` objects.
  * Maintain identical call-site syntax to `hsm` (including overloads for function pointers and lambdas) while keeping implementation choices real-time friendly (no `std::function` in the meta-model).

* [x] **Add unit tests for compile-time strings and nodes**
  * Add tests that validate `fixed_string` behavior (e.g. equality, construction from string literals, use as template parameters).
  * Add tests that instantiate several simple state machine expressions using the new DSL and expression nodes, checking that types compile and encode the expected names and structure at compile time (via `static_assert`s and type traits).
  * Run the full C++ test suite and ensure zero regressions compared to the previous baseline.

* [x] **Finalize this step**
  * Once all tasks above are complete and all tests pass with zero regressions, check off every TODO in this step file.
