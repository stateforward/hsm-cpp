#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "cthsm/cthsm.hpp"

using namespace cthsm;

TEST_CASE("Dispatch - Initial State") {
  // Simple machine without initial -> stays at root
  constexpr auto simple = define("machine");
  compile<simple> sm;
  Instance inst;
  sm.start(inst);

  CHECK(sm.state() == "/machine");
}

TEST_CASE("Dispatch - Initial With Target") {
  constexpr auto model = define("machine", initial(target("idle")),
                                state("idle"), state("active"));

  compile<model> sm;
  Instance inst;
  sm.start(inst);
  CHECK(sm.state() == "/machine/idle");
}

TEST_CASE("Dispatch - Simple Transition") {
  constexpr auto model =
      define("machine", initial(target("idle")),
             state("idle", transition(on("start"), target("active"))),
             state("active"));

  compile<model> sm;
  Instance inst;
  sm.start(inst);
  CHECK(sm.state() == "/machine/idle");

  sm.dispatch(inst, cthsm::AnyEvent{"start"});
  CHECK(sm.state() == "/machine/active");
}

TEST_CASE("Dispatch - Unknown Event Ignored") {
  constexpr auto model =
      define("machine", initial(target("idle")),
             state("idle", transition(on("start"), target("active"))),
             state("active"));

  compile<model> sm;
  Instance inst;
  sm.start(inst);
  CHECK(sm.state() == "/machine/idle");

  sm.dispatch(inst, cthsm::AnyEvent{"unknown"});
  CHECK(sm.state() == "/machine/idle");  // Should stay

  sm.dispatch(inst, cthsm::AnyEvent{"start"});
  CHECK(sm.state() == "/machine/active");
}

TEST_CASE("Dispatch - Hierarchical Transition") {
  constexpr auto model = define(
      "machine", initial(target("idle")),
      state("idle", transition(on("start"), target("working"))),
      state(
          "working", initial(target("processing")),
          state("processing", transition(on("done"), target("/machine/idle"))),
          state("waiting")));

  compile<model> sm;
  Instance inst;
  sm.start(inst);
  CHECK(sm.state() == "/machine/idle");

  sm.dispatch(inst, cthsm::AnyEvent{"start"});
  CHECK(sm.state() == "/machine/working/processing");

  sm.dispatch(inst, cthsm::AnyEvent{"done"});
  CHECK(sm.state() == "/machine/idle");
}

TEST_CASE("Dispatch - Sibling Transitions") {
  constexpr auto model =
      define("machine", initial(target("s1")),
             state("s1", transition(on("next"), target("s2"))),
             state("s2", transition(on("next"), target("s3"))),
             state("s3", transition(on("reset"), target("s1"))));

  compile<model> sm;
  Instance inst;
  sm.start(inst);
  CHECK(sm.state() == "/machine/s1");

  sm.dispatch(inst, cthsm::AnyEvent{"next"});
  CHECK(sm.state() == "/machine/s2");

  sm.dispatch(inst, cthsm::AnyEvent{"next"});
  CHECK(sm.state() == "/machine/s3");

  sm.dispatch(inst, cthsm::AnyEvent{"reset"});
  CHECK(sm.state() == "/machine/s1");
}

TEST_CASE("Dispatch - Nested Initial Transitions") {
  constexpr auto model =
      define("machine", initial(target("outer")),
             state("outer", initial(target("inner")),
                   state("inner", initial(target("leaf")), state("leaf"))));

  compile<model> sm;
  Instance inst;
  sm.start(inst);
  CHECK(sm.state() == "/machine/outer/inner/leaf");
}
