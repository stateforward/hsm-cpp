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

  sm.dispatch(inst, cthsm::EventBase{"start"});
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

  sm.dispatch(inst, cthsm::EventBase{"unknown"});
  CHECK(sm.state() == "/machine/idle");  // Should stay

  sm.dispatch(inst, cthsm::EventBase{"start"});
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

  sm.dispatch(inst, cthsm::EventBase{"start"});
  CHECK(sm.state() == "/machine/working/processing");

  sm.dispatch(inst, cthsm::EventBase{"done"});
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

  sm.dispatch(inst, cthsm::EventBase{"next"});
  CHECK(sm.state() == "/machine/s2");

  sm.dispatch(inst, cthsm::EventBase{"next"});
  CHECK(sm.state() == "/machine/s3");

  sm.dispatch(inst, cthsm::EventBase{"reset"});
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

TEST_CASE("Dispatch - History pseudostates") {
  struct HistoryInstance : public Instance {};

  SUBCASE("Deep history returns to exact leaf") {
    constexpr auto model = define(
        "HistoryDeep",
        initial(target("/HistoryDeep/region/A")),
        state("region",
              state("A", transition(on("TO_B"),
                                     target("/HistoryDeep/region/B"))),
              state("B", transition(on("EXIT"),
                                     target("/HistoryDeep/outside")))),
        state("outside", transition(on("BACK_DEEP"),
                                     target(deep_history("/HistoryDeep/region")))));

    compile<model, HistoryInstance> sm;
    HistoryInstance inst;
    sm.start(inst);

    CHECK(sm.state() == "/HistoryDeep/region/A");

    sm.dispatch(inst, cthsm::EventBase{"TO_B"});
    CHECK(sm.state() == "/HistoryDeep/region/B");

    sm.dispatch(inst, cthsm::EventBase{"EXIT"});
    CHECK(sm.state() == "/HistoryDeep/outside");

    sm.dispatch(inst, cthsm::EventBase{"BACK_DEEP"});
    CHECK(sm.state() == "/HistoryDeep/region/B");
  }

  SUBCASE("Shallow history re-enters last active child and follows its initial") {
    constexpr auto model = define(
        "HistoryShallow",
        initial(target("/HistoryShallow/C/X")),
        state("C",
              initial(target("/HistoryShallow/C/X")),
              state("X",
                    initial(target("/HistoryShallow/C/X/X1")),
                    state("X1", transition(on("TO_Y"),
                                           target("/HistoryShallow/C/Y"))),
                    state("X2")),
              state("Y", transition(on("EXIT"),
                                    target("/HistoryShallow/outside")))),
        state("outside", transition(on("BACK_SHALLOW"),
                                     target(shallow_history("/HistoryShallow/C")))));

    compile<model, HistoryInstance> sm;
    HistoryInstance inst;
    sm.start(inst);

    CHECK(sm.state() == "/HistoryShallow/C/X/X1");

    sm.dispatch(inst, cthsm::EventBase{"TO_Y"});
    CHECK(sm.state() == "/HistoryShallow/C/Y");

    sm.dispatch(inst, cthsm::EventBase{"EXIT"});
    CHECK(sm.state() == "/HistoryShallow/outside");

    sm.dispatch(inst, cthsm::EventBase{"BACK_SHALLOW"});
    CHECK(sm.state() == "/HistoryShallow/C/Y");
  }

  SUBCASE("History with no prior leaf falls back to composite initial") {
    constexpr auto model = define(
        "HistoryNoPrior",
        initial(target("/HistoryNoPrior/outside")),
        state("C",
              initial(target("/HistoryNoPrior/C/A")),
              state("A"), state("B")),
        state("outside", transition(on("GO"),
                                     target(shallow_history("/HistoryNoPrior/C")))));

    compile<model, HistoryInstance> sm;
    HistoryInstance inst;
    sm.start(inst);

    CHECK(sm.state() == "/HistoryNoPrior/outside");

    sm.dispatch(inst, cthsm::EventBase{"GO"});
    CHECK(sm.state() == "/HistoryNoPrior/C/A");
  }
}

TEST_CASE("Dispatch - Wildcard transitions") {
  struct WildInstance : public Instance {};

  constexpr auto model = define(
      "WildcardMachine", initial(target("/WildcardMachine/s")),
      state("s", transition(on("foo"),
                             target("/WildcardMachine/foo_state")),
            transition(on<Any>(), target("/WildcardMachine/any_state"))),
      state("foo_state"), state("any_state"));

  SUBCASE("Wildcard handles unknown event in same state") {
    compile<model, WildInstance> sm;
    WildInstance inst;
    sm.start(inst);

    CHECK(sm.state() == "/WildcardMachine/s");

    sm.dispatch(inst, cthsm::EventBase{"bar"});
    CHECK(sm.state() == "/WildcardMachine/any_state");
  }

  SUBCASE("Specific event wins over wildcard") {
    compile<model, WildInstance> sm;
    WildInstance inst;
    sm.start(inst);

    CHECK(sm.state() == "/WildcardMachine/s");

    sm.dispatch(inst, cthsm::EventBase{"foo"});
    CHECK(sm.state() == "/WildcardMachine/foo_state");
  }
}

TEST_CASE("Dispatch - Completion transitions (Composite)") {
  struct CompInstance : public Instance {};

  constexpr auto model = define(
      "CompMachine",
      initial(target("/CompMachine/container/start")),
      state("container",
            // Completion transition on the container itself (hierarchical)
            transition(target("/CompMachine/finished")),
            state("start", transition(on("GO"), target("/CompMachine/container/end"))),
            final("end")
      ),
      state("finished")
  );

  compile<model, CompInstance> sm;
  CompInstance inst;
  sm.start(inst);

  CHECK(sm.state() == "/CompMachine/container/start");

  sm.dispatch(inst, cthsm::EventBase{"GO"});
  // Should go to end, then immediately triggers completion on container -> finished
  CHECK(sm.state() == "/CompMachine/finished");
}

TEST_CASE("Dispatch - History and Completion interaction") {
  struct HistCompInstance : public Instance {};

  constexpr auto model = define(
      "HistComp",
      initial(target("/HistComp/container/step1")),
      state("container",
            transition(target("/HistComp/completed")),
            state("step1", transition(on("NEXT"), target("step2"))),
            state("step2", transition(on("FINISH"), target("done")),
                           transition(on("INTERRUPT"), target("/HistComp/interrupted"))),
            final("done")
      ),
      state("completed",
            transition(on("RESTART"), target("/HistComp/container"))),
      state("interrupted",
            transition(on("RESUME"), target(shallow_history("/HistComp/container"))))
  );

  compile<model, HistCompInstance> sm;
  HistCompInstance inst;
  sm.start(inst);

  CHECK(sm.state() == "/HistComp/container/step1");

  // 1. Interrupt and Resume (History)
  sm.dispatch(inst, cthsm::EventBase{"NEXT"});
  CHECK(sm.state() == "/HistComp/container/step2");

  sm.dispatch(inst, cthsm::EventBase{"INTERRUPT"});
  CHECK(sm.state() == "/HistComp/interrupted");

  sm.dispatch(inst, cthsm::EventBase{"RESUME"});
  // Should resume to step2
  CHECK(sm.state() == "/HistComp/container/step2");

  // 2. Finish and Complete
  sm.dispatch(inst, cthsm::EventBase{"FINISH"});
  // step2 -> done (final) -> container completes -> completed
  CHECK(sm.state() == "/HistComp/completed");
}
