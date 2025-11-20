#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "cthsm/cthsm.hpp"

TEST_CASE("cthsm smoke test compiles and exposes basic APIs") {
  constexpr auto model = cthsm::define("smoke");
  cthsm::compile<model> machine;
  cthsm::Instance inst;
  machine.start(inst);

  CHECK(machine.state() == "/smoke");
  CHECK_NOTHROW(machine.dispatch(inst, cthsm::AnyEvent{"noop"}));
}
