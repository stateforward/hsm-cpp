#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "hsm.hpp"
#include <iostream>
#include <chrono>

using namespace hsm;

TEST_CASE("TransitionMap - Simple transitions") {
    auto model = define("TestMachine",
        initial(target("idle")),
        state("idle",
            transition(on("START"), target("running"))
        ),
        state("running",
            transition(on("STOP"), target("idle"))
        )
    );
    
    // Build transition map
    
    // Check that transition map has entries for states
    CHECK(model->transition_map.size() > 0);
    
    // Check idle state has transitions
    std::string idle_path = "/TestMachine/idle";
    auto idle_it = model->transition_map.find(idle_path);
    CHECK(idle_it != model->transition_map.end());
    CHECK(idle_it->second.size() == 1); // One event type
    
    // Check running state has transitions
    std::string running_path = "/TestMachine/running";
    auto running_it = model->transition_map.find(running_path);
    CHECK(running_it != model->transition_map.end());
    CHECK(running_it->second.size() == 1); // One event type
}

TEST_CASE("TransitionMap - Hierarchical transitions") {
    auto model = define("TestMachine",
        initial(target("parent")),
        state("parent",
            initial(target("child1")),
            transition(on("PARENT_EVENT"), target("sibling")),
            state("child1",
                transition(on("CHILD_EVENT"), target("child2"))
            ),
            state("child2")
        ),
        state("sibling")
    );
    
    
    // Parent should have its transition
    std::string parent_path = "/TestMachine/parent";
    auto parent_it = model->transition_map.find(parent_path);
    CHECK(parent_it != model->transition_map.end());
    CHECK(parent_it->second.size() == 1);
    
    // Child1 should have its own transition AND inherit parent's transition
    std::string child1_path = "/TestMachine/parent/child1";
    auto child1_it = model->transition_map.find(child1_path);
    CHECK(child1_it != model->transition_map.end());
    CHECK(child1_it->second.size() == 2); // CHILD_EVENT and inherited PARENT_EVENT
    
    // Child2 should inherit parent's transition
    std::string child2_path = "/TestMachine/parent/child2";
    auto child2_it = model->transition_map.find(child2_path);
    CHECK(child2_it != model->transition_map.end());
    CHECK(child2_it->second.size() == 1); // Inherited PARENT_EVENT
}

TEST_CASE("TransitionMap - Multiple transitions per state") {
    auto model = define("TestMachine",
        initial(target("idle")),
        state("idle",
            transition(on("EVENT1"), target("state1")),
            transition(on("EVENT2"), target("state2")),
            transition(on("EVENT3"), effect([](Context&, Instance&, Event&) {}))
        ),
        state("state1"),
        state("state2")
    );
    
    
    std::string idle_path = "/TestMachine/idle";
    auto idle_it = model->transition_map.find(idle_path);
    CHECK(idle_it != model->transition_map.end());
    CHECK(idle_it->second.size() == 3); // Three different events
}

TEST_CASE("TransitionMap - Guarded transitions") {
    auto model = define("TestMachine",
        initial(target("idle")),
        state("idle",
            transition(on("EVENT"), guard([](Context&, Instance&, Event&) { return true; }), target("state1")),
            transition(on("EVENT"), guard([](Context&, Instance&, Event&) { return false; }), target("state2")),
            transition(on("EVENT"), target("state3")) // Fallback
        ),
        state("state1"),
        state("state2"),
        state("state3")
    );
    
    
    std::string idle_path = "/TestMachine/idle";
    auto idle_it = model->transition_map.find(idle_path);
    CHECK(idle_it != model->transition_map.end());
    
    // Should have one event type with multiple transitions
    CHECK(idle_it->second.size() == 1);
    
    // Get transitions for EVENT
    std::string event_name = "EVENT";
    auto event_it = idle_it->second.find(event_name);
    CHECK(event_it != idle_it->second.end());
    CHECK(event_it->second.size() == 3); // Three transitions for same event
}

TEST_CASE("TransitionMap - Choice state transitions") {
    auto model = define("TestMachine",
        initial(target("idle")),
        state("idle",
            transition(on("EVENT"), target("choice"))
        ),
        choice("choice",
            transition(guard([](Context&, Instance&, Event&) { return true; }), target("state1")),
            transition(target("state2")) // Required fallback
        ),
        state("state1"),
        state("state2")
    );
    
    
    // Choice state should have its transitions stored on the vertex
    auto* choice_vertex = model->get_member<Vertex>("/TestMachine/choice");
    CHECK(choice_vertex != nullptr);
    CHECK(choice_vertex->transitions.size() == 2);
}

TEST_CASE("TransitionMap - Final state no transitions") {
    auto model = define("TestMachine",
        initial(target("idle")),
        state("idle",
            transition(on("FINISH"), target("done"))
        ),
        final("done")
    );
    
    
    // Final state should exist in map but have empty transitions
    std::string done_path = "/TestMachine/done";
    auto done_it = model->transition_map.find(done_path);
    CHECK(done_it != model->transition_map.end());
    CHECK(done_it->second.empty());
}

// Timer function needs to be a function pointer for after()
std::chrono::milliseconds timer_1000ms(Context&, Instance&, Event&) {
    return std::chrono::milliseconds(1000);
}

TEST_CASE("TransitionMap - Timer transitions") {
    auto model = define("TestMachine",
        initial(target("idle")),
        state("idle",
            transition(after(timer_1000ms), target("timeout"))
        ),
        state("timeout")
    );
    
    
    // Timer transitions are stored with generated event names
    std::string idle_path = "/TestMachine/idle";
    auto idle_it = model->transition_map.find(idle_path);
    CHECK(idle_it != model->transition_map.end());
    
    // Should have one event type (the generated timer event)
    CHECK(idle_it->second.size() == 1);
    
    // The event name will be generated like "_after_123"
    // Just check that we have a transition
    for (const auto& [event_name, transitions] : idle_it->second) {
        CHECK(transitions.size() == 1);
        // Target might be stored as absolute path
        CHECK((transitions[0]->target == "timeout" || 
               transitions[0]->target == "/TestMachine/idle/timeout" ||
               transitions[0]->target == "/TestMachine/timeout"));
    }
}