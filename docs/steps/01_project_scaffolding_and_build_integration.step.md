## Step 1 – Project scaffolding and build integration

*Note: If new `*.step.md` files are needed for this phase, create them immediately after this file and name them using the pattern `{{step number}}.{{sub step}}_{{title}}.step.md` (for this step, `step number` is `01`).*

* [x] **Verify prerequisites and baseline**
  * Confirm all TODOs in any earlier step files (if they exist) are checked off.
  * Run the current C++ test suite (e.g. `ctest` or the existing test runner) and record this as the baseline result for regression checking.

* [x] **Create `cthsm` header structure**
  * Create the `include/cthsm/` directory structure, including `include/cthsm/cthsm.hpp` as the primary public header and `include/cthsm/detail/` for internal implementation headers.
  * Ensure `cthsm.hpp` includes only minimal content for now (namespace `cthsm` declaration and forward declarations), so it compiles cleanly on all supported compilers.

* [x] **Integrate `cthsm` into the build system**
  * Update the top-level `CMakeLists.txt` (and any relevant subdirectory CMake files) to install and export the new `include/cthsm/` headers.
  * If a compiled library is needed, add a `src/cthsm/` target (e.g. `cthsm` static library) but keep the implementation lightweight and suitable for embedded builds.
  * Verify that the existing `hsm` targets still build successfully with no changes in behavior.

* [x] **Wire up the example to the new header**
  * Update `examples/cthsm_example.cpp` to include the new public header (`#include "cthsm/cthsm.hpp"` or equivalent include path) instead of any legacy placeholder.
  * Adjust the CMake configuration for examples so that the `cthsm_example` target builds and links against the new `cthsm` library or headers-only target.

* [x] **Add and run basic compilation tests**
  * Add a simple doctest or unit test that includes `cthsm/cthsm.hpp` and verifies that a trivial `cthsm::define` and `cthsm::compile` declaration can be compiled (even if still unimplemented).
  * Run the full C++ test suite and the `cthsm_example` binary (if built) to ensure zero regressions relative to the previously recorded baseline.

* [x] **Finalize this step**
  * Once all tasks above are complete and all tests pass with zero regressions, check off every TODO in this step file.
