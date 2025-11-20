#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <string>
#include <vector>

#include "cthsm/cthsm.hpp"

using namespace cthsm;

struct FinalStatesInstance : public Instance {
  std::vector<std::string> execution_log;
  bool final_reached{false};

  void log(const std::string& message) { execution_log.push_back(message); }

  void clear() {
    execution_log.clear();
    final_reached = false;
  }
};

// Action functions
void log_entry_start(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_start");
}
void log_entry_active(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_active");
}
void log_entry_container(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_container");
}
void log_entry_working(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_working");
}
void log_entry_reset(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_reset");
}
void log_entry_level1(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_level1");
}
void log_entry_level2(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_level2");
}
void log_entry_step1(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_step1");
}
void log_entry_step2(Context&, Instance& i, const AnyEvent&) {
  static_cast<FinalStatesInstance&>(i).log("entry_step2");
}

TEST_CASE("Final States - Basic Functionality") {
  SUBCASE("Simple Final State") {
    constexpr auto model =
        define("SimpleFinal", initial(target("/SimpleFinal/start")),
               state("start", entry(log_entry_start),
                     transition(on("FINISH"), target("/SimpleFinal/end"))),
               final("end"));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    CHECK(sm.state() == "/SimpleFinal/start");
    CHECK_FALSE(instance.final_reached);

    // Transition to final state
    sm.dispatch(instance, cthsm::AnyEvent{"FINISH"});

    CHECK(sm.state() == "/SimpleFinal/end");

    // Check execution order
    REQUIRE(instance.execution_log.size() >= 1);
    CHECK(instance.execution_log[0] == "entry_start");
  }

  SUBCASE("Final State with Multiple Paths") {
    constexpr auto model = define(
        "MultipleFinal", initial(target("/MultipleFinal/active")),
        state("active", entry(log_entry_active),
              transition(on("SUCCESS"), target("/MultipleFinal/success")),
              transition(on("FAILURE"), target("/MultipleFinal/failure")),
              transition(on("CANCEL"), target("/MultipleFinal/cancelled"))),
        final("success"), final("failure"), final("cancelled"));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    CHECK(sm.state() == "/MultipleFinal/active");

    SUBCASE("Success Path") {
      sm.dispatch(instance, cthsm::AnyEvent{"SUCCESS"});
      CHECK(sm.state() == "/MultipleFinal/success");
    }

    SUBCASE("Failure Path") {
      sm.dispatch(instance, cthsm::AnyEvent{"FAILURE"});
      CHECK(sm.state() == "/MultipleFinal/failure");
    }

    SUBCASE("Cancel Path") {
      sm.dispatch(instance, cthsm::AnyEvent{"CANCEL"});
      CHECK(sm.state() == "/MultipleFinal/cancelled");
    }
  }
}

TEST_CASE("Final States - Semantic Restrictions") {
  SUBCASE("Final State Cannot Have Entry Actions") {
    // In cthsm, final() does not accept entry actions, so we can't even compile
    // if we tried. We just verify basic behavior.
    constexpr auto model = define(
        "FinalWithEntry", initial(target("/FinalWithEntry/start")),
        state("start", transition(on("FINISH"), target("/FinalWithEntry/end"))),
        final("end"));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    sm.dispatch(instance, cthsm::AnyEvent{"FINISH"});
    CHECK(sm.state() == "/FinalWithEntry/end");

    bool found_final_entry = false;
    for (const auto& log : instance.execution_log) {
      if (log == "entry_end") {
        found_final_entry = true;
        break;
      }
    }
    CHECK_FALSE(found_final_entry);
  }

  // Skipping "Cannot Have Exit Actions" and "Cannot Have Outgoing Transitions"
  // as they are enforced by API

  SUBCASE("Final State Cannot Have Outgoing Transitions (Runtime Check)") {
    // Even if we dispatch events, it should stay in final state
    constexpr auto model = define(
        "FinalNoTransitions", initial(target("/FinalNoTransitions/start")),
        state("start",
              transition(on("FINISH"), target("/FinalNoTransitions/end"))),
        final("end"));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    sm.dispatch(instance, cthsm::AnyEvent{"FINISH"});
    CHECK(sm.state() == "/FinalNoTransitions/end");

    // Try various events - all should be ignored
    sm.dispatch(instance, cthsm::AnyEvent{"RESTART"});
    CHECK(sm.state() == "/FinalNoTransitions/end");

    sm.dispatch(instance, cthsm::AnyEvent{"CONTINUE"});
    CHECK(sm.state() == "/FinalNoTransitions/end");

    sm.dispatch(instance, cthsm::AnyEvent{"ANY_EVENT"});
    CHECK(sm.state() == "/FinalNoTransitions/end");
  }
}

TEST_CASE("Final States - Hierarchical Scenarios") {
  SUBCASE("Final State in Nested State") {
    constexpr auto model = define(
        "NestedFinal",
        initial(target(
            "/NestedFinal/container/working")),  // Direct target to working
        state("container", entry(log_entry_container),
              // initial(target("working")), // Not using local initial for
              // simplicity/cthsm preference
              state("working", entry(log_entry_working),
                    transition(on("COMPLETE"),
                               target("/NestedFinal/container/done"))),
              final("done"),
              transition(on("RESET"), target("/NestedFinal/reset"))),
        state("reset", entry(log_entry_reset)));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    CHECK(sm.state() == "/NestedFinal/container/working");

    // Complete the nested work
    sm.dispatch(instance, cthsm::AnyEvent{"COMPLETE"});
    CHECK(sm.state() == "/NestedFinal/container/done");

    // Parent state should still be able to handle events
    sm.dispatch(instance, cthsm::AnyEvent{"RESET"});
    CHECK(sm.state() == "/NestedFinal/reset");

    // Check execution order
    REQUIRE(instance.execution_log.size() >= 3);
    CHECK(instance.execution_log[0] == "entry_container");
    CHECK(instance.execution_log[1] == "entry_working");
    CHECK(instance.execution_log[2] == "entry_reset");
  }

  SUBCASE("Multiple Final States in Hierarchy") {
    constexpr auto model = define(
        "HierarchicalFinals",
        initial(target("/HierarchicalFinals/level1/level2")),
        state(
            "level1", entry(log_entry_level1),
            // initial(target("level2")),
            state("level2", entry(log_entry_level2),
                  transition(on("FINISH_INNER"),
                             target("/HierarchicalFinals/level1/inner_done"))),
            final("inner_done"),
            transition(on("FINISH_OUTER"),
                       target("/HierarchicalFinals/outer_done"))),
        final("outer_done"));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    CHECK(sm.state() == "/HierarchicalFinals/level1/level2");

    SUBCASE("Inner Final First") {
      // Finish inner state first
      sm.dispatch(instance, cthsm::AnyEvent{"FINISH_INNER"});
      CHECK(sm.state() == "/HierarchicalFinals/level1/inner_done");

      // Then finish outer state
      sm.dispatch(instance, cthsm::AnyEvent{"FINISH_OUTER"});
      CHECK(sm.state() == "/HierarchicalFinals/outer_done");
    }

    SUBCASE("Outer Final Direct") {
      // Finish outer state directly (should exit inner state)
      sm.dispatch(instance, cthsm::AnyEvent{"FINISH_OUTER"});
      CHECK(sm.state() == "/HierarchicalFinals/outer_done");
    }
  }
}

TEST_CASE("Final States - Error Conditions and Edge Cases") {
  SUBCASE("Final State as Initial Target") {
    constexpr auto model = define(
        "ImmediateFinal", initial(target("/ImmediateFinal/end")), final("end"));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    // Should go directly to final state
    CHECK(sm.state() == "/ImmediateFinal/end");
  }

  SUBCASE("Final State with Deferred Events") {
    // Final states should not defer events since they don't process them
    // (Actually cthsm supports defer() on final() ?? No, final() takes only
    // Name) So this test is checking behavior if we just send event.
    constexpr auto model = define(
        "FinalWithDeferred", initial(target("/FinalWithDeferred/start")),
        state("start",
              transition(on("FINISH"), target("/FinalWithDeferred/end"))),
        final("end"));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    sm.dispatch(instance, cthsm::AnyEvent{"FINISH"});
    CHECK(sm.state() == "/FinalWithDeferred/end");

    // Events sent to final state should not be deferred or processed
    sm.dispatch(instance, cthsm::AnyEvent{"DEFERRED_EVENT"});
    CHECK(sm.state() == "/FinalWithDeferred/end");
  }

  SUBCASE("Rapid Transitions to Final State") {
    constexpr auto model =
        define("RapidFinal", initial(target("/RapidFinal/start")),
               state("start", entry(log_entry_start),
                     transition(on("STEP1"), target("/RapidFinal/step1"))),
               state("step1", entry(log_entry_step1),
                     transition(on("STEP2"), target("/RapidFinal/step2"))),
               state("step2", entry(log_entry_step2),
                     transition(on("FINAL"), target("/RapidFinal/end"))),
               final("end"));

    compile<model, FinalStatesInstance> sm;
    FinalStatesInstance instance;
    sm.start(instance);

    CHECK(sm.state() == "/RapidFinal/start");

    // Rapid succession of events leading to final state
    sm.dispatch(instance, cthsm::AnyEvent{"STEP1"});
    CHECK(sm.state() == "/RapidFinal/step1");

    sm.dispatch(instance, cthsm::AnyEvent{"STEP2"});
    CHECK(sm.state() == "/RapidFinal/step2");

    sm.dispatch(instance, cthsm::AnyEvent{"FINAL"});
    CHECK(sm.state() == "/RapidFinal/end");

    // Verify all entry actions were called
    REQUIRE(instance.execution_log.size() >= 3);
    CHECK(instance.execution_log[0] == "entry_start");
    CHECK(instance.execution_log[1] == "entry_step1");
    CHECK(instance.execution_log[2] == "entry_step2");
  }
}
