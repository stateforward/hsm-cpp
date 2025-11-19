## Step 6 – Test parity and size/speed comparison

*Note: If new `*.step.md` files are needed for this phase, create them immediately after this file and name them using the pattern `{{step number}}.{{sub step}}_{{title}}.step.md` (for this step, `step number` is `06`).*

* [ ] **Verify previous steps and establish baseline**
  * Confirm all TODOs in `01_project_scaffolding_and_build_integration.step.md` through `05_behaviors_timers_and_deferral.step.md` are checked off.
  * Run the current C++ test suite and ensure all tests pass before starting final test parity and benchmarking work.

* [ ] **Port remaining `hsm` tests to `cthsm`**
  * Review the existing `tests/*.cpp` for `hsm` and identify which tests should have direct `cthsm` equivalents (transition tests, guard conditions, timers, deferral, path resolution using absolute paths, etc.).
  * For each selected test, create a `cthsm` variant that:
    * uses the same model structure and event names,
    * uses **absolute paths** for `source`/`target`,
    * replaces `hsm` namespace references with `cthsm` and uses `cthsm::compile` instead of the runtime `HSM`/`start`.
  * Ensure each `cthsm` test compiles and passes on all targeted compilers/toolchains.

* [ ] **Ensure zero-regression combined test suite**
  * Run the full test suite including both `hsm` and `cthsm` tests.
  * Fix any regressions in either engine so that adding `cthsm` introduces no behavioral regressions for `hsm` and vice versa.
  * Record a final “all green” baseline that includes both runtime and compile-time engines.

* [ ] **Design benchmarks for size and speed comparison**
  * Select or create representative benchmarks (e.g. based on `examples/benchmark.cpp`, `performance_test.cpp`, or new microbenchmarks) that can be compiled and run against both `hsm` and `cthsm` with equivalent models and workloads.
  * Ensure benchmarks are configured to be meaningful for real-time embedded targets (e.g. fixed event traces, bounded queues, no dynamic allocation in the hot path).

* [ ] **Measure and compare code size**
  * Build both `hsm` and `cthsm` configurations with the same compiler, optimization level, and target (ideally one or more embedded-style configurations such as `-Os`).
  * Measure:
    * object/library sizes for `hsm` vs `cthsm`,
    * binary sizes for benchmark executables built with each engine.
  * Document the size differences and ensure they align with expectations from the `cthsm` design (ideally equal or smaller for the cthsm core, acknowledging any template-induced growth).

* [ ] **Measure and compare runtime performance**
  * Run the selected benchmarks for both `hsm` and `cthsm`, measuring:
    * steady-state event dispatch throughput,
    * latency distribution (min/avg/max) for dispatch on representative event mixes,
    * any overhead of timers and deferral handling.
  * Ensure measurements are reproducible and, where possible, taken on representative hardware for the target embedded environments.
  * Document the performance comparison, highlighting where `cthsm` meets or exceeds `hsm` and any trade-offs discovered.

* [ ] **Summarize results and update documentation**
  * Update `cthsm.design.md` (or a new summary doc) with:
    * a short summary of test parity status (which `hsm` tests have 1:1 `cthsm` equivalents),
    * a table or bullet summary of size and speed comparisons between `hsm` and `cthsm`,
    * any important caveats or limitations discovered during testing.

* [ ] **Finalize this step**
  * Once all tasks above are complete and all tests and benchmarks run with zero regressions, check off every TODO in this step file.
