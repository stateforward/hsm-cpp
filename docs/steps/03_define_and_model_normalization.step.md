## Step 3 – `define` and model normalization

*Note: This step has been updated to pivot from a purely type-based normalization to a value-based (NTTP) normalization approach to preserve the `state("name")` DSL syntax in C++20.*

* [x] **Verify previous steps and establish baseline**
  * Confirm all TODOs in `01_project_scaffolding_and_build_integration.step.md` and `02_meta_model_and_compile_time_strings.step.md` are checked off.
  * Run the current C++ test suite and ensure all tests are passing before proceeding.

* [x] **Upgrade compile-time literal support**
  * Use a simplified `cthsm::detail::fixed_string<N>` for NTTPs.
  * Update DSL builders to store `fixed_string` values in expression nodes (e.g., `state_expr`, `choice_expr`).
  * **Decision**: Abandoned complex "literal" type hacks in favor of standard `structural_tuple` and passing the model as an NTTP.

* [x] **Design the normalized meta-model**
  * Define runtime-friendly descriptor types (`state_desc`, `transition_desc`, `event_desc`) in `cthsm/detail/meta_model.hpp`.
  * Define `normalized_model_data` struct to hold `std::array`s of these descriptors and any necessary string storage.

* [x] **Implement `cthsm::define` and structural tuples**
  * Implement `cthsm::detail::structural_tuple` to allow expression trees to be used as NTTPs (requires public members and/or default comparison).
  * Update `cthsm::define` to return the expression tree value directly (removing the `normalized_type` alias).
  * Update `cthsm::compile<Model>` to accept `auto Model` (value) instead of a type.

* [x] **Implement `consteval` normalization logic**
  * **Counting**: Implement recursive functions in `normalize.hpp` to count the total number of states, transitions, and unique events in the expression tree to size the output arrays.
  * **String Arena**: Implement a mechanism (e.g., a large `fixed_string` or `std::array` buffer within `normalized_model_data`) to store concatenated absolute path strings (`"/root/child"`) generated during normalization, as `state_desc` cannot own dynamic strings.
  * **Population**: Implement recursive `collect_states`, `collect_transitions`, etc., that walk the tree, compute absolute paths, fill the string arena, and populate the `state_desc` / `transition_desc` arrays with correct IDs and indices.
  * **ID Resolution**: Ensure transition targets (which are just path strings in the input) are resolved to state IDs by looking up the path in the fully populated state list.

* [x] **Support absolute-only source/target semantics**
  * In the population logic, enforce that `cthsm::source` and `cthsm::target` paths are resolved against the normalized absolute paths.
  * Compute transition kinds (external/internal/self/local) by comparing source and target paths/IDs.

* [x] **Update and expand tests**
  * **Rewrite `cthsm_meta_model_test.cpp`**: Update checks to verify the *value* of `compile<model>::normalized_model` (e.g., `static_assert(machine::normalized_model.states[0].name == "/root")`) rather than inspecting types.
  * Add coverage for:
    * State hierarchy and parent/child IDs.
    * Transition resolution and kinds.
    * Event collection and deferral sets.
  * Ensure `cthsm_smoke_test` compiles and runs with the new `compile<model>` syntax.

* [x] **Finalize this step**
  * Once all tasks above are complete and all tests pass with zero regressions, check off every TODO in this step file.
