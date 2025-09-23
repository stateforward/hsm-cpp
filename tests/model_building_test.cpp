#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include "hsm.hpp"

TEST_CASE("Model Building - Basic Structure") {
  SUBCASE("Simple flat state machine") {
    auto model = hsm::define(
      "SimpleMachine",
      hsm::initial(hsm::target("idle")),
      hsm::state("idle"),
      hsm::state("active"),
      hsm::final("done")
    );
    
    CHECK(model != nullptr);
    CHECK(model->qualified_name() == "/SimpleMachine");
    
    // Check states exist
    CHECK(model->get_member<hsm::State>("/SimpleMachine/idle") != nullptr);
    CHECK(model->get_member<hsm::State>("/SimpleMachine/active") != nullptr);
    CHECK(model->get_member<hsm::State>("/SimpleMachine/done") != nullptr);
    
    // Check initial pseudo-state
    CHECK(!model->initial.empty());
    auto* initial = model->get_member<hsm::Vertex>(model->initial);
    CHECK(initial != nullptr);
    CHECK(initial->transitions.size() == 1);
  }
  
  SUBCASE("Nested state machine") {
    auto model = hsm::define(
      "NestedMachine",
      hsm::initial(hsm::target("parent/child1")),
      hsm::state("parent",
        hsm::state("child1"),
        hsm::state("child2")
      ),
      hsm::state("other")
    );
    
    CHECK(model != nullptr);
    
    // Check parent state
    auto* parent = model->get_member<hsm::State>("/NestedMachine/parent");
    CHECK(parent != nullptr);
    
    // Check nested states
    CHECK(model->get_member<hsm::State>("/NestedMachine/parent/child1") != nullptr);
    CHECK(model->get_member<hsm::State>("/NestedMachine/parent/child2") != nullptr);
    CHECK(model->get_member<hsm::State>("/NestedMachine/other") != nullptr);
  }
  
  SUBCASE("State with transitions") {
    auto model = hsm::define(
      "TransitionMachine",
      hsm::initial(hsm::target("state1")),
      hsm::state("state1",
        hsm::transition(hsm::on("GO"), hsm::target("../state2"))
      ),
      hsm::state("state2")
    );
    
    auto* state1 = model->get_member<hsm::State>("/TransitionMachine/state1");
    CHECK(state1 != nullptr);
    CHECK(state1->transitions.size() == 1);
    
    auto* transition = model->get_member<hsm::Transition>(state1->transitions[0]);
    CHECK(transition != nullptr);
    CHECK(transition->source == "/TransitionMachine/state1");
    CHECK(transition->target == "/TransitionMachine/state2");
    CHECK(transition->events.size() == 1);
    CHECK(transition->events[0] == "GO");
  }
  
  SUBCASE("State with behaviors") {
    int entry_called = 0;
    int exit_called = 0;
    
    auto model = hsm::define(
      "BehaviorMachine",
      hsm::initial(hsm::target("active")),
      hsm::state("active",
        hsm::entry([&entry_called](hsm::Context&, hsm::Instance&, hsm::Event&) {
          entry_called++;
        }),
        hsm::exit([&exit_called](hsm::Context&, hsm::Instance&, hsm::Event&) {
          exit_called++;
        })
      )
    );
    
    auto* active = model->get_member<hsm::State>("/BehaviorMachine/active");
    CHECK(active != nullptr);
    CHECK(active->entry.size() == 1);
    CHECK(active->exit.size() == 1);
    
    // Check behaviors were registered
    auto* entry_behavior = model->get_member<hsm::Behavior>(active->entry[0]);
    CHECK(entry_behavior != nullptr);
    CHECK(entry_behavior->method != nullptr);
    
    auto* exit_behavior = model->get_member<hsm::Behavior>(active->exit[0]);
    CHECK(exit_behavior != nullptr);
    CHECK(exit_behavior->method != nullptr);
  }
  
  SUBCASE("Choice state") {
    auto model = hsm::define(
      "ChoiceMachine",
      hsm::initial(hsm::target("start")),
      hsm::state("start",
        hsm::transition(hsm::on("DECIDE"), hsm::target("decider"))
      ),
      hsm::choice("decider",
        hsm::transition(hsm::guard([](hsm::Context&, hsm::Instance&, hsm::Event&) { return true; }),
                       hsm::target("path1")),
        hsm::transition(hsm::target("path2"))
      ),
      hsm::state("path1"),
      hsm::state("path2")
    );
    
    auto* choice = model->get_member<hsm::Vertex>("/ChoiceMachine/decider");
    CHECK(choice != nullptr);
    CHECK(hsm::is_kind(choice->kind(), hsm::Kind::Choice));
    CHECK(choice->transitions.size() == 2);
  }
  
  SUBCASE("Deferred events") {
    auto model = hsm::define(
      "DeferMachine",
      hsm::initial(hsm::target("waiting")),
      hsm::state("waiting",
        hsm::defer("DATA", "INFO")
      )
    );
    
    auto* waiting = model->get_member<hsm::State>("/DeferMachine/waiting");
    CHECK(waiting != nullptr);
    CHECK(waiting->deferred.size() == 2);
    CHECK(waiting->deferred[0] == "DATA");
    CHECK(waiting->deferred[1] == "INFO");
  }
}

TEST_CASE("Model Building - Path Resolution") {
  SUBCASE("Relative paths in transitions") {
    auto model = hsm::define(
      "PathMachine",
      hsm::initial(hsm::target("a")),
      hsm::state("a",
        hsm::transition(hsm::on("TO_B"), hsm::target("../b")),
        hsm::state("a1",
          hsm::transition(hsm::on("TO_A2"), hsm::target("../a2"))
        ),
        hsm::state("a2")
      ),
      hsm::state("b")
    );
    
    // Check transition from a to b
    auto* state_a = model->get_member<hsm::State>("/PathMachine/a");
    auto* trans_a = model->get_member<hsm::Transition>(state_a->transitions[0]);
    CHECK(trans_a->target == "/PathMachine/b");
    
    // Check transition from a1 to a2
    auto* state_a1 = model->get_member<hsm::State>("/PathMachine/a/a1");
    auto* trans_a1 = model->get_member<hsm::Transition>(state_a1->transitions[0]);
    CHECK(trans_a1->target == "/PathMachine/a/a2");
  }
  
  SUBCASE("Self transitions") {
    auto model = hsm::define(
      "SelfMachine",
      hsm::state("active",
        hsm::transition(hsm::on("REFRESH"), hsm::target("."))
      )
    );
    
    auto* active = model->get_member<hsm::State>("/SelfMachine/active");
    auto* trans = model->get_member<hsm::Transition>(active->transitions[0]);
    CHECK(trans->source == trans->target);
    CHECK(hsm::is_kind(trans->kind(), hsm::Kind::Self));
  }
}

TEST_CASE("Model Building - Transition Paths") {
  SUBCASE("Transition paths are computed") {
    auto model = hsm::define(
      "PathComputation",
      hsm::initial(hsm::target("a/a1")),
      hsm::state("a",
        hsm::state("a1",
          hsm::transition(hsm::on("TO_B"), hsm::target("/PathComputation/b/b1"))
        ),
        hsm::state("a2")
      ),
      hsm::state("b",
        hsm::state("b1"),
        hsm::state("b2")
      )
    );
    
    auto* a1 = model->get_member<hsm::State>("/PathComputation/a/a1");
    auto* trans = model->get_member<hsm::Transition>(a1->transitions[0]);
    
    // Check that paths were computed
    CHECK(!trans->paths.empty());
    
    // Should have a path for the source state
    auto it = trans->paths.find(std::string("/PathComputation/a/a1"));
    CHECK(it != trans->paths.end());
    
    // Should exit a1 and a, enter b and b1
    CHECK(it->second.exit.size() == 2);
    CHECK(it->second.enter.size() == 2);
  }
}