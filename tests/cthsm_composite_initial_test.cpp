#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <string>
#include <vector>

#include "cthsm/cthsm.hpp"

using namespace cthsm;

struct CompositeInstance : public Instance {
  std::vector<std::string> execution_log;
  int effect_count{0};
  int entry_count{0};
  int exit_count{0};

  void log(const std::string& message) { execution_log.push_back(message); }

  void clear_log() {
    execution_log.clear();
    effect_count = 0;
    entry_count = 0;
    exit_count = 0;
  }
};

// Behaviors
void entry_composite(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_composite");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void exit_composite(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("exit_composite");
  static_cast<CompositeInstance&>(i).exit_count++;
}

void entry_child1(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_child1");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void exit_child1(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("exit_child1");
  static_cast<CompositeInstance&>(i).exit_count++;
}

void entry_child2(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_child2");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void exit_child2(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("exit_child2");
  static_cast<CompositeInstance&>(i).exit_count++;
}

void entry_other(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_other");
  static_cast<CompositeInstance&>(i).entry_count++;
}

void effect_initial(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("effect_initial");
  static_cast<CompositeInstance&>(i).effect_count++;
}

void entry_outer(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_outer");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void entry_inner(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_inner");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void entry_deepest(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_deepest");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void entry_deepest_alt(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_deepest_alt");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void entry_inner_alt(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_inner_alt");
  static_cast<CompositeInstance&>(i).entry_count++;
}

void entry_start(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_start");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void exit_start(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("exit_start");
  static_cast<CompositeInstance&>(i).exit_count++;
}

void entry_comp1(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_comp1");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void entry_comp1_child1(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_comp1_child1");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void entry_comp1_child2(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_comp1_child2");
  static_cast<CompositeInstance&>(i).entry_count++;
}

void entry_comp2(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_comp2");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void entry_comp2_child1(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_comp2_child1");
  static_cast<CompositeInstance&>(i).entry_count++;
}
void entry_comp2_child2(Context&, Instance& i, const EventBase&) {
  static_cast<CompositeInstance&>(i).log("entry_comp2_child2");
  static_cast<CompositeInstance&>(i).entry_count++;
}

TEST_CASE("Composite States with Initial Pseudostates") {
  SUBCASE("Basic Composite State with Initial") {
    constexpr auto model =
        define("CompositeWithInitial",
               initial(target("/CompositeWithInitial/composite")),
               state("composite", entry(entry_composite), exit(exit_composite),
                     initial(target("/CompositeWithInitial/composite/child1")),
                     state("child1", entry(entry_child1), exit(exit_child1)),
                     state("child2", entry(entry_child2), exit(exit_child2))),
               state("other", entry(entry_other)));

    compile<model, CompositeInstance> sm;
    CompositeInstance instance;
    sm.start(instance);

    // The machine should enter composite state and then automatically enter
    // child1
    CHECK(sm.state() == "/CompositeWithInitial/composite/child1");
    CHECK(instance.execution_log.size() == 2);
    CHECK(instance.execution_log[0] == "entry_composite");
    CHECK(instance.execution_log[1] == "entry_child1");
    CHECK(instance.entry_count == 2);
  }

  SUBCASE("Composite State with Initial and Effect") {
    constexpr auto model = define(
        "CompositeInitialWithEffect",
        initial(target("/CompositeInitialWithEffect/composite")),
        state("composite", entry(entry_composite),
              initial(target("/CompositeInitialWithEffect/composite/child1"),
                      effect(effect_initial)),
              state("child1", entry(entry_child1)),
              state("child2", entry(entry_child2))));

    compile<model, CompositeInstance> sm;
    CompositeInstance instance;
    sm.start(instance);

    // Should execute: entry_composite, effect_initial, entry_child1
    CHECK(sm.state() == "/CompositeInitialWithEffect/composite/child1");
    CHECK(instance.execution_log.size() == 3);
    CHECK(instance.execution_log[0] == "entry_composite");
    CHECK(instance.execution_log[1] == "effect_initial");
    CHECK(instance.execution_log[2] == "entry_child1");
    CHECK(instance.entry_count == 2);
    CHECK(instance.effect_count == 1);
  }

  SUBCASE("Nested Composite States with Initial") {
    constexpr auto model = define(
        "NestedComposites", initial(target("/NestedComposites/outer")),
        state("outer", entry(entry_outer),
              initial(target("/NestedComposites/outer/inner")),
              state("inner", entry(entry_inner),
                    initial(target("/NestedComposites/outer/inner/deepest")),
                    state("deepest", entry(entry_deepest)),
                    state("deepest_alt", entry(entry_deepest_alt))),
              state("inner_alt", entry(entry_inner_alt))));

    compile<model, CompositeInstance> sm;
    CompositeInstance instance;
    sm.start(instance);

    // Should cascade through all initial transitions
    CHECK(sm.state() == "/NestedComposites/outer/inner/deepest");
    CHECK(instance.execution_log.size() == 3);
    CHECK(instance.execution_log[0] == "entry_outer");
    CHECK(instance.execution_log[1] == "entry_inner");
    CHECK(instance.execution_log[2] == "entry_deepest");
    CHECK(instance.entry_count == 3);
  }

  SUBCASE("Transition to Composite State Triggers Initial") {
    constexpr auto model =
        define("TransitionToComposite",
               initial(target("/TransitionToComposite/start")),
               state("start", entry(entry_start), exit(exit_start),
                     transition(on("GO_TO_COMPOSITE"),
                                target("/TransitionToComposite/composite"))),
               state("composite", entry(entry_composite),
                     initial(target("/TransitionToComposite/composite/child1")),
                     state("child1", entry(entry_child1)),
                     state("child2", entry(entry_child2))));

    compile<model, CompositeInstance> sm;
    CompositeInstance instance;
    sm.start(instance);

    CHECK(sm.state() == "/TransitionToComposite/start");
    instance.clear_log();
    sm.dispatch(instance, cthsm::EventBase{"GO_TO_COMPOSITE"});
    // Transition to composite state
    sm.dispatch(instance, cthsm::EventBase{"GO_TO_COMPOSITE"});

    // Should exit start, enter composite, then enter child1 via initial
    CHECK(sm.state() == "/TransitionToComposite/composite/child1");

    // cthsm should correctly call exit_start
    CHECK(instance.execution_log.size() == 3);
    CHECK(instance.execution_log[0] == "exit_start");
    CHECK(instance.execution_log[1] == "entry_composite");
    CHECK(instance.execution_log[2] == "entry_child1");
  }

  SUBCASE("Direct Transition to Nested State Bypasses Initial") {
    constexpr auto model = define(
        "DirectToNested", initial(target("/DirectToNested/start")),
        state("start", entry(entry_start), exit(exit_start),
              transition(on("GO_TO_CHILD2"),
                         target("/DirectToNested/composite/child2"))),
        state(
            "composite", entry(entry_composite),
            initial(target("/DirectToNested/composite/child1")),  // This should
                                                                  // be bypassed
            state("child1", entry(entry_child1)),
            state("child2", entry(entry_child2))));

    compile<model, CompositeInstance> sm;
    CompositeInstance instance;
    sm.start(instance);

    CHECK(sm.state() == "/DirectToNested/start");
    instance.clear_log();

    // Direct transition to child2
    sm.dispatch(instance, cthsm::EventBase{"GO_TO_CHILD2"});

    // Should go directly to child2, bypassing the initial transition
    CHECK(sm.state() == "/DirectToNested/composite/child2");

    // cthsm should call exit_start
    CHECK(instance.execution_log.size() == 3);
    CHECK(instance.execution_log[0] == "exit_start");
    CHECK(instance.execution_log[1] == "entry_composite");
    CHECK(instance.execution_log[2] == "entry_child2");

    // Verify that child1 was NOT entered (initial was bypassed)
    for (const auto& log : instance.execution_log) {
      CHECK(log != "entry_child1");
    }
  }

  SUBCASE("Multiple Composite States with Different Initials") {
    constexpr auto model = define(
        "MultipleComposites", initial(target("/MultipleComposites/comp1")),
        state("comp1", entry(entry_comp1),
              initial(target("/MultipleComposites/comp1/comp1_child1")),
              state("comp1_child1", entry(entry_comp1_child1)),
              state("comp1_child2", entry(entry_comp1_child2)),
              transition(on("TO_COMP2"), target("/MultipleComposites/comp2"))),
        state("comp2", entry(entry_comp2),
              initial(target(
                  "/MultipleComposites/comp2/comp2_child2")),  // Different
                                                               // initial child
              state("comp2_child1", entry(entry_comp2_child1)),
              state("comp2_child2", entry(entry_comp2_child2))));

    compile<model, CompositeInstance> sm;
    CompositeInstance instance;
    sm.start(instance);

    // First composite should enter child1
    CHECK(sm.state() == "/MultipleComposites/comp1/comp1_child1");
    instance.clear_log();

    // Transition to second composite
    sm.dispatch(instance, cthsm::EventBase{"TO_COMP2"});

    // Second composite should enter child2 (its initial)
    CHECK(sm.state() == "/MultipleComposites/comp2/comp2_child2");
    // Last log should be entry of child2
    CHECK(instance.execution_log.back() == "entry_comp2_child2");
  }

  SUBCASE("Composite State Initial with Absolute Path") {
    constexpr auto model = define(
        "CompositeAbsoluteInitial",
        initial(target("/CompositeAbsoluteInitial/composite")),
        state("composite", entry(entry_composite),
              initial(target("/CompositeAbsoluteInitial/composite/child2")),
              state("child1", entry(entry_child1)),
              state("child2", entry(entry_child2))));

    compile<model, CompositeInstance> sm;
    CompositeInstance instance;
    sm.start(instance);

    // Should use absolute path to enter child2
    CHECK(sm.state() == "/CompositeAbsoluteInitial/composite/child2");
    CHECK(instance.execution_log.size() == 2);
    CHECK(instance.execution_log[0] == "entry_composite");
    CHECK(instance.execution_log[1] == "entry_child2");
  }
}
