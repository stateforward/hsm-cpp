#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "hsm.hpp"

// Test instance for guard conditions
class GuardConditionsInstance : public hsm::Instance {
 public:
  std::vector<std::string> execution_log;
  std::atomic<int> counter{0};
  std::atomic<bool> flag_a{false};
  std::atomic<bool> flag_b{false};
  std::atomic<bool> can_transition{true};
  std::string last_event_name;

  void log(const std::string& message) { execution_log.push_back(message); }

  void clear() {
    execution_log.clear();
    counter = 0;
    flag_a = false;
    flag_b = false;
    can_transition = true;
    last_event_name.clear();
  }

  void set_last_event(const std::string& event_name) {
    last_event_name = event_name;
  }
};

// Action functions
void log_entry(const std::string& name, hsm::Context& /*ctx*/,
               hsm::Instance& inst, hsm::Event& /*event*/) {
  auto& test_inst = static_cast<GuardConditionsInstance&>(inst);
  test_inst.log("entry_" + name);
}

void log_effect(const std::string& name, hsm::Context& /*ctx*/,
                hsm::Instance& inst, hsm::Event& /*event*/) {
  auto& test_inst = static_cast<GuardConditionsInstance&>(inst);
  test_inst.log("effect_" + name);
}

void increment_counter(hsm::Context& /*ctx*/, hsm::Instance& inst,
                       hsm::Event& /*event*/) {
  auto& test_inst = static_cast<GuardConditionsInstance&>(inst);
  test_inst.counter++;
  test_inst.log("increment_counter");
}

void set_flag_a(hsm::Context& /*ctx*/, hsm::Instance& inst,
                hsm::Event& /*event*/) {
  auto& test_inst = static_cast<GuardConditionsInstance&>(inst);
  test_inst.flag_a = true;
  test_inst.log("set_flag_a");
}

void record_event_name(hsm::Context& /*ctx*/, hsm::Instance& inst,
                       hsm::Event& event) {
  auto& test_inst = static_cast<GuardConditionsInstance&>(inst);
  test_inst.set_last_event(event.name);
  test_inst.log("recorded_event_" + event.name);
}

// Guard functions
bool guard_counter_positive(hsm::Context& /*ctx*/,
                            GuardConditionsInstance& inst,
                            hsm::Event& /*event*/) {
  return inst.counter.load() > 0;
}

bool guard_counter_even(hsm::Context& /*ctx*/, GuardConditionsInstance& inst,
                        hsm::Event& /*event*/) {
  return (inst.counter.load() % 2) == 0;
}

bool guard_counter_greater_than_5(hsm::Context& /*ctx*/,
                                  GuardConditionsInstance& inst,
                                  hsm::Event& /*event*/) {
  return inst.counter.load() > 5;
}

bool guard_flag_a(hsm::Context& /*ctx*/, GuardConditionsInstance& inst,
                  hsm::Event& /*event*/) {
  return inst.flag_a.load();
}

bool guard_flag_b(hsm::Context& /*ctx*/, GuardConditionsInstance& inst,
                  hsm::Event& /*event*/) {
  return inst.flag_b.load();
}

bool guard_can_transition(hsm::Context& /*ctx*/, GuardConditionsInstance& inst,
                          hsm::Event& /*event*/) {
  return inst.can_transition.load();
}

bool guard_always_true(hsm::Context& /*ctx*/, GuardConditionsInstance& /*inst*/,
                       hsm::Event& /*event*/) {
  return true;
}

bool guard_always_false(hsm::Context& /*ctx*/,
                        GuardConditionsInstance& /*inst*/,
                        hsm::Event& /*event*/) {
  return false;
}

bool guard_event_name_contains_x(hsm::Context& /*ctx*/,
                                 GuardConditionsInstance& /*inst*/,
                                 hsm::Event& event) {
  return event.name.find('X') != std::string::npos;
}

bool guard_complex_condition(hsm::Context& /*ctx*/,
                             GuardConditionsInstance& inst,
                             hsm::Event& /*event*/) {
  return inst.counter.load() > 2 && inst.flag_a.load() && !inst.flag_b.load();
}

// Stateful guard that changes behavior on each call
bool guard_stateful_toggle(hsm::Context& /*ctx*/, GuardConditionsInstance& inst,
                           hsm::Event& /*event*/) {
  static bool state = false;
  state = !state;
  inst.log("guard_stateful_toggle_" + std::string(state ? "true" : "false"));
  return state;
}

TEST_CASE("Guard Conditions - Basic Functionality") {
  SUBCASE("Simple Boolean Guard") {
    auto model = hsm::define(
        "SimpleGuard", hsm::initial(hsm::target("start")),
        hsm::state(
            "start",
            hsm::entry(
                [](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                  log_entry("start", ctx, inst, event);
                }),
            hsm::transition(hsm::on("ATTEMPT"), hsm::guard(guard_flag_a),
                            hsm::target("../guarded")),
            hsm::transition(hsm::on("ATTEMPT"), hsm::target("../unguarded"))),
        hsm::state("guarded",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("guarded", ctx, inst, event);
                   })),
        hsm::state("unguarded",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("unguarded", ctx, inst, event);
                   })));

    SUBCASE("Guard Allows Transition") {
      GuardConditionsInstance instance;
      instance.flag_a = true;  // Guard should pass
      hsm::start(instance, model);

      CHECK(instance.state() == "/SimpleGuard/start");

      instance.dispatch(hsm::Event("ATTEMPT")).wait();
      CHECK(instance.state() == "/SimpleGuard/guarded");

      REQUIRE(instance.execution_log.size() >= 2);
      CHECK(instance.execution_log[0] == "entry_start");
      CHECK(instance.execution_log[1] == "entry_guarded");
    }

    SUBCASE("Guard Blocks Transition") {
      GuardConditionsInstance instance;
      instance.flag_a = false;  // Guard should fail
      hsm::start(instance, model);

      CHECK(instance.state() == "/SimpleGuard/start");

      instance.dispatch(hsm::Event("ATTEMPT")).wait();
      CHECK(instance.state() == "/SimpleGuard/unguarded");

      REQUIRE(instance.execution_log.size() >= 2);
      CHECK(instance.execution_log[0] == "entry_start");
      CHECK(instance.execution_log[1] == "entry_unguarded");
    }
  }

  SUBCASE("Multiple Guards on Same Event") {
    auto model = hsm::define(
        "MultipleGuards", hsm::initial(hsm::target("start")),
        hsm::state(
            "start",
            hsm::transition(hsm::on("GO"),
                            hsm::guard(guard_counter_greater_than_5),
                            hsm::target("../high")),
            hsm::transition(hsm::on("GO"), hsm::guard(guard_counter_positive),
                            hsm::target("../low")),
            hsm::transition(hsm::on("GO"), hsm::target("../zero"))),
        hsm::state("high", hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                         hsm::Event& event) {
                     log_entry("high", ctx, inst, event);
                   })),
        hsm::state("low", hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                        hsm::Event& event) {
                     log_entry("low", ctx, inst, event);
                   })),
        hsm::state("zero", hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                         hsm::Event& event) {
                     log_entry("zero", ctx, inst, event);
                   })));

    SUBCASE("First Guard Succeeds") {
      GuardConditionsInstance instance;
      instance.counter = 10;  // > 5, first guard should succeed
      hsm::start(instance, model);

      instance.dispatch(hsm::Event("GO")).wait();
      CHECK(instance.state() == "/MultipleGuards/high");
    }

    SUBCASE("Second Guard Succeeds") {
      GuardConditionsInstance instance;
      instance.counter = 3;  // > 0 but <= 5, second guard should succeed
      hsm::start(instance, model);

      instance.dispatch(hsm::Event("GO")).wait();
      CHECK(instance.state() == "/MultipleGuards/low");
    }

    SUBCASE("No Guards Succeed") {
      GuardConditionsInstance instance;
      instance.counter = 0;  // No guards should succeed
      hsm::start(instance, model);

      instance.dispatch(hsm::Event("GO")).wait();
      CHECK(instance.state() == "/MultipleGuards/zero");
    }
  }
}

TEST_CASE("Guard Conditions - Event-Based Guards") {
  SUBCASE("Guard Based on Event Content") {
    auto model = hsm::define(
        "EventGuard", hsm::initial(hsm::target("start")),
        hsm::state(
            "start",
            hsm::transition(
                hsm::on("PROCESS"), hsm::guard(guard_event_name_contains_x),
                hsm::target("../special"), hsm::effect(record_event_name)),
            hsm::transition(hsm::on("PROCESS"), hsm::target("../normal"))),
        hsm::state("special",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("special", ctx, inst, event);
                   })),
        hsm::state("normal",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("normal", ctx, inst, event);
                   })));

    SUBCASE("Event Name Contains X") {
      GuardConditionsInstance instance;
      hsm::start(instance, model);

      // Event name with X should trigger special handling
      instance.dispatch(hsm::Event("PROCESS_X")).wait();
      CHECK(instance.state() == "/EventGuard/special");

      // Should have recorded the event name
      CHECK(instance.last_event_name == "PROCESS_X");
    }

    SUBCASE("Event Name Without X") {
      GuardConditionsInstance instance;
      hsm::start(instance, model);

      // Event name without X should use normal handling
      instance.dispatch(hsm::Event("PROCESS")).wait();
      CHECK(instance.state() == "/EventGuard/normal");
    }
  }

  SUBCASE("Context-Based Guards") {
    auto model = hsm::define(
        "ContextGuard", hsm::initial(hsm::target("start")),
        hsm::state("start",
                   hsm::transition(
                       hsm::on("TEST"),
                       hsm::guard([](hsm::Context& ctx, hsm::Instance& /*inst*/,
                                     hsm::Event& /*event*/) {
                         // This is a contrived example - in practice, context
                         // would have useful state
                         return !ctx.is_set();  // Guard passes if context is
                                                // not cancelled
                       }),
                       hsm::target("../allowed")),
                   hsm::transition(hsm::on("TEST"), hsm::target("../blocked"))),
        hsm::state("allowed",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("allowed", ctx, inst, event);
                   })),
        hsm::state("blocked",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("blocked", ctx, inst, event);
                   })));

    GuardConditionsInstance instance;
    hsm::start(instance, model);

    // Normal case - context is not set
    instance.dispatch(hsm::Event("TEST")).wait();
    CHECK(instance.state() == "/ContextGuard/allowed");
  }
}

TEST_CASE("Guard Conditions - Complex Scenarios") {
  SUBCASE("Nested Guards with Effects") {
    auto model = hsm::define(
        "NestedGuards", hsm::initial(hsm::target("start")),
        hsm::state("start",
                   hsm::transition(
                       hsm::on("COMPLEX"), hsm::guard(guard_complex_condition),
                       hsm::target("../complex_target"),
                       hsm::effect([](hsm::Context& ctx, hsm::Instance& inst,
                                      hsm::Event& event) {
                         log_effect("complex_effect", ctx, inst, event);
                       })),
                   hsm::transition(
                       hsm::on("COMPLEX"), hsm::guard(guard_counter_positive),
                       hsm::target("../simple_target"),
                       hsm::effect([](hsm::Context& ctx, hsm::Instance& inst,
                                      hsm::Event& event) {
                         log_effect("simple_effect", ctx, inst, event);
                       })),
                   hsm::transition(hsm::on("COMPLEX"),
                                   hsm::target("../default_target"))),
        hsm::state("complex_target",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("complex_target", ctx, inst, event);
                   })),
        hsm::state("simple_target",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("simple_target", ctx, inst, event);
                   })),
        hsm::state("default_target",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("default_target", ctx, inst, event);
                   })));

    SUBCASE("Complex Guard Succeeds") {
      GuardConditionsInstance instance;
      instance.counter = 5;  // > 2
      instance.flag_a = true;
      instance.flag_b = false;  // Complex condition should pass
      hsm::start(instance, model);

      instance.dispatch(hsm::Event("COMPLEX")).wait();
      CHECK(instance.state() == "/NestedGuards/complex_target");

      // Check that complex effect was executed
      bool found_complex_effect = false;
      for (const auto& log : instance.execution_log) {
        if (log == "effect_complex_effect") {
          found_complex_effect = true;
          break;
        }
      }
      CHECK(found_complex_effect);
    }

    SUBCASE("Simple Guard Succeeds") {
      GuardConditionsInstance instance;
      instance.counter = 2;  // > 0 but complex condition fails
      hsm::start(instance, model);

      instance.dispatch(hsm::Event("COMPLEX")).wait();
      CHECK(instance.state() == "/NestedGuards/simple_target");

      // Check that simple effect was executed
      bool found_simple_effect = false;
      for (const auto& log : instance.execution_log) {
        if (log == "effect_simple_effect") {
          found_simple_effect = true;
          break;
        }
      }
      CHECK(found_simple_effect);
    }

    SUBCASE("No Guards Succeed") {
      GuardConditionsInstance instance;
      instance.counter = 0;  // All guards should fail
      hsm::start(instance, model);

      instance.dispatch(hsm::Event("COMPLEX")).wait();
      CHECK(instance.state() == "/NestedGuards/default_target");
    }
  }

  SUBCASE("Stateful Guards") {
    auto model = hsm::define(
        "StatefulGuards", hsm::initial(hsm::target("start")),
        hsm::state(
            "start",
            hsm::transition(hsm::on("TOGGLE"),
                            hsm::guard(guard_stateful_toggle),
                            hsm::target("../toggled")),
            hsm::transition(hsm::on("TOGGLE"), hsm::target("../not_toggled"))),
        hsm::state("toggled",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("toggled", ctx, inst, event);
                   }),
                   hsm::transition(hsm::on("BACK"), hsm::target("../start"))),
        hsm::state("not_toggled",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("not_toggled", ctx, inst, event);
                   }),
                   hsm::transition(hsm::on("BACK"), hsm::target("../start"))));

    GuardConditionsInstance instance;
    hsm::start(instance, model);

    // First toggle - should succeed (guard returns true)
    instance.dispatch(hsm::Event("TOGGLE")).wait();
    CHECK(instance.state() == "/StatefulGuards/toggled");

    // Go back
    instance.dispatch(hsm::Event("BACK")).wait();
    CHECK(instance.state() == "/StatefulGuards/start");

    // Second toggle - should fail (guard returns false)
    instance.dispatch(hsm::Event("TOGGLE")).wait();
    CHECK(instance.state() == "/StatefulGuards/not_toggled");

    // Check that guard was called with correct states
    bool found_true = false, found_false = false;
    for (const auto& log : instance.execution_log) {
      if (log == "guard_stateful_toggle_true") found_true = true;
      if (log == "guard_stateful_toggle_false") found_false = true;
    }
    CHECK(found_true);
    CHECK(found_false);
  }
}

TEST_CASE("Guard Conditions - Hierarchical Scenarios") {
  SUBCASE("Guards on Parent State Transitions") {
    auto model = hsm::define(
        "HierarchicalGuards", hsm::initial(hsm::target("container")),
        hsm::state(
            "container",
            hsm::entry(
                [](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                  log_entry("container", ctx, inst, event);
                }),
            hsm::initial(hsm::target("child1")),
            hsm::state(
                "child1",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                              hsm::Event& event) {
                  log_entry("child1", ctx, inst, event);
                }),
                hsm::transition(hsm::on("SWITCH"), hsm::target("../child2"))),
            hsm::state("child2",
                       hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                     hsm::Event& event) {
                         log_entry("child2", ctx, inst, event);
                       })),
            hsm::transition(hsm::on("EXIT"), hsm::guard(guard_flag_a),
                            hsm::target("../outside"))),
        hsm::state("outside",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("outside", ctx, inst, event);
                   })));

    SUBCASE("Parent Guard Allows Exit") {
      GuardConditionsInstance instance;
      instance.flag_a = true;  // Guard should allow exit
      hsm::start(instance, model);

      CHECK(instance.state() == "/HierarchicalGuards/container/child1");

      // Try to exit container - should succeed
      instance.dispatch(hsm::Event("EXIT")).wait();
      CHECK(instance.state() == "/HierarchicalGuards/outside");
    }

    SUBCASE("Parent Guard Blocks Exit") {
      GuardConditionsInstance instance;
      instance.flag_a = false;  // Guard should block exit
      hsm::start(instance, model);

      CHECK(instance.state() == "/HierarchicalGuards/container/child1");

      // Try to exit container - should be blocked
      instance.dispatch(hsm::Event("EXIT")).wait();
      CHECK(instance.state() == "/HierarchicalGuards/container/child1");

      // But internal transitions should still work
      instance.dispatch(hsm::Event("SWITCH")).wait();
      CHECK(instance.state() == "/HierarchicalGuards/container/child2");
    }
  }

  SUBCASE("Guard Evaluation Order") {
    auto model = hsm::define(
        "GuardOrder", hsm::initial(hsm::target("parent")),
        hsm::state(
            "parent", hsm::initial(hsm::target("child")),
            hsm::state(
                "child",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                              hsm::Event& event) {
                  log_entry("child", ctx, inst, event);
                }),
                hsm::transition(
                    hsm::on("TEST"), hsm::guard(guard_counter_even),
                    hsm::target("../sibling"),
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst,
                                   hsm::Event& event) {
                      log_effect("child_to_sibling", ctx, inst, event);
                    }))),
            hsm::state("sibling",
                       hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                     hsm::Event& event) {
                         log_entry("sibling", ctx, inst, event);
                       })),
            hsm::transition(
                hsm::on("TEST"), hsm::guard(guard_counter_positive),
                hsm::target("../other"),
                hsm::effect([](hsm::Context& ctx, hsm::Instance& inst,
                               hsm::Event& event) {
                  log_effect("parent_to_other", ctx, inst, event);
                }))),
        hsm::state("other",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("other", ctx, inst, event);
                   })));

    SUBCASE("Child Guard Succeeds (Higher Priority)") {
      GuardConditionsInstance instance;
      instance.counter = 4;  // Even and positive
      hsm::start(instance, model);

      CHECK(instance.state() == "/GuardOrder/parent/child");

      instance.dispatch(hsm::Event("TEST")).wait();
      // Child transition should take precedence
      CHECK(instance.state() == "/GuardOrder/parent/sibling");

      // Check that child effect was executed
      bool found_child_effect = false;
      for (const auto& log : instance.execution_log) {
        if (log == "effect_child_to_sibling") {
          found_child_effect = true;
          break;
        }
      }
      CHECK(found_child_effect);
    }

    SUBCASE("Parent Guard Succeeds (Child Guard Fails)") {
      GuardConditionsInstance instance;
      instance.counter = 3;  // Positive but odd
      hsm::start(instance, model);

      CHECK(instance.state() == "/GuardOrder/parent/child");

      instance.dispatch(hsm::Event("TEST")).wait();
      // Parent transition should be taken
      CHECK(instance.state() == "/GuardOrder/other");

      // Check that parent effect was executed
      bool found_parent_effect = false;
      for (const auto& log : instance.execution_log) {
        if (log == "effect_parent_to_other") {
          found_parent_effect = true;
          break;
        }
      }
      CHECK(found_parent_effect);
    }
  }
}

TEST_CASE("Guard Conditions - Edge Cases and Error Conditions") {
  SUBCASE("Guards with Side Effects") {
    auto model = hsm::define(
        "GuardSideEffects", hsm::initial(hsm::target("start")),
        hsm::state("start",
                   hsm::transition(
                       hsm::on("TEST"),
                       hsm::guard([](hsm::Context& /*ctx*/, hsm::Instance& inst,
                                     hsm::Event& /*event*/) {
                         auto& test_inst =
                             static_cast<GuardConditionsInstance&>(inst);
                         test_inst.counter++;  // Side effect in guard
                         test_inst.log("guard_side_effect");
                         return test_inst.counter.load() > 2;
                       }),
                       hsm::target("../guarded")),
                   hsm::transition(hsm::on("TEST"), hsm::target("../start"))),
        hsm::state("guarded",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("guarded", ctx, inst, event);
                   })),
        hsm::state("unguarded",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("unguarded", ctx, inst, event);
                   })));

    GuardConditionsInstance instance;
    hsm::start(instance, model);

    // First attempt - guard fails but has side effect
    instance.dispatch(hsm::Event("TEST")).wait();
    CHECK(instance.state() == "/GuardSideEffects/start");
    CHECK(instance.counter.load() == 1);

    // Second attempt - guard still fails
    instance.dispatch(hsm::Event("TEST")).wait();
    CHECK(instance.state() == "/GuardSideEffects/start");
    CHECK(instance.counter.load() == 2);

    // Third attempt - guard succeeds
    instance.dispatch(hsm::Event("TEST")).wait();
    CHECK(instance.state() == "/GuardSideEffects/guarded");
    CHECK(instance.counter.load() == 3);
  }

  SUBCASE("Guards Always True/False") {
    auto model = hsm::define(
        "ConstantGuards", hsm::initial(hsm::target("start")),
        hsm::state("start",
                   hsm::transition(hsm::on("ALWAYS_TRUE"),
                                   hsm::guard(guard_always_true),
                                   hsm::target("../always_true_target")),
                   hsm::transition(hsm::on("ALWAYS_FALSE"),
                                   hsm::guard(guard_always_false),
                                   hsm::target("../never_reached")),
                   hsm::transition(hsm::on("ALWAYS_FALSE"),
                                   hsm::target("../fallback"))),
        hsm::state("always_true_target",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("always_true_target", ctx, inst, event);
                   })),
        hsm::state("never_reached"),
        hsm::state("fallback",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("fallback", ctx, inst, event);
                   })));

    GuardConditionsInstance instance;
    hsm::start(instance, model);

    // Always true guard should work
    instance.dispatch(hsm::Event("ALWAYS_TRUE")).wait();
    CHECK(instance.state() == "/ConstantGuards/always_true_target");

    // Reset
    hsm::start(instance, model);

    // Always false guard should fall through to next transition
    instance.dispatch(hsm::Event("ALWAYS_FALSE")).wait();
    CHECK(instance.state() == "/ConstantGuards/fallback");
  }

  SUBCASE("Rapid Guard Evaluations") {
    auto model = hsm::define(
        "RapidGuards", hsm::initial(hsm::target("start")),
        hsm::state("start",
                   hsm::transition(
                       hsm::on("RAPID"), hsm::guard(guard_counter_even),
                       hsm::target("../even"), hsm::effect(increment_counter)),
                   hsm::transition(hsm::on("RAPID"), hsm::target("../odd"),
                                   hsm::effect(increment_counter))),
        hsm::state("even",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("even", ctx, inst, event);
                   }),
                   hsm::transition(hsm::on("BACK"), hsm::target("../start"))),
        hsm::state("odd",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("odd", ctx, inst, event);
                   }),
                   hsm::transition(hsm::on("BACK"), hsm::target("../start"))));

    GuardConditionsInstance instance;
    hsm::start(instance, model);

    // Rapid succession of events
    for (int i = 0; i < 10; ++i) {
      instance.dispatch(hsm::Event("RAPID")).wait();
      if (i % 2 == 0) {
        CHECK(instance.state() == "/RapidGuards/even");
      } else {
        CHECK(instance.state() == "/RapidGuards/odd");
      }
      instance.dispatch(hsm::Event("BACK")).wait();
      CHECK(instance.state() == "/RapidGuards/start");
    }

    CHECK(instance.counter.load() == 10);  // 1 increment per round
  }
}