#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "hsm.hpp"
#include <iostream>

using namespace hsm;

TEST_CASE("DeferredMap - Simple deferred events") {
    auto model = define("TestMachine",
        initial(target("idle")),
        state("idle",
            defer("EVENT1", "EVENT2"),
            transition(on("START"), target("processing"))
        ),
        state("processing",
            transition(on("EVENT1"), target("handled1")),
            transition(on("EVENT2"), target("handled2"))
        ),
        state("handled1"),
        state("handled2")
    );
    
    buildDeferredTable(*model);
    
    // Check deferred map for idle state
    std::string idle_path = "/TestMachine/idle";
    auto idle_it = model->deferred_map.find(idle_path);
    CHECK(idle_it != model->deferred_map.end());
    CHECK(idle_it->second.size() == 2);
    
    // Processing state should not have deferred events
    std::string processing_path = "/TestMachine/processing";
    auto processing_it = model->deferred_map.find(processing_path);
    bool processing_empty = (processing_it == model->deferred_map.end()) || processing_it->second.empty();
    CHECK(processing_empty);
}

TEST_CASE("DeferredMap - Hierarchical deferred events") {
    auto model = define("TestMachine",
        initial(target("parent")),
        state("parent",
            initial(target("child1")),
            defer("PARENT_DEFERRED"),
            state("child1",
                defer("CHILD_DEFERRED"),
                transition(on("NEXT"), target("child2"))
            ),
            state("child2")
        ),
        state("sibling",
            transition(on("PARENT_DEFERRED"), target("handled")),
            transition(on("CHILD_DEFERRED"), target("handled"))
        ),
        state("handled")
    );
    
    buildDeferredTable(*model);
    
    // Parent should have its deferred event
    std::string parent_path = "/TestMachine/parent";
    auto parent_it = model->deferred_map.find(parent_path);
    CHECK(parent_it != model->deferred_map.end());
    CHECK(parent_it->second.size() == 1);
    
    // Child1 should have its deferred event AND inherit parent's deferred event
    std::string child1_path = "/TestMachine/parent/child1";
    auto child1_it = model->deferred_map.find(child1_path);
    CHECK(child1_it != model->deferred_map.end());
    CHECK(child1_it->second.size() == 2); // CHILD_DEFERRED and inherited PARENT_DEFERRED
    
    // Child2 should inherit parent's deferred event
    std::string child2_path = "/TestMachine/parent/child2";
    auto child2_it = model->deferred_map.find(child2_path);
    CHECK(child2_it != model->deferred_map.end());
    CHECK(child2_it->second.size() == 1); // Inherited PARENT_DEFERRED
}

TEST_CASE("DeferredMap - No deferred events in final state") {
    auto model = define("TestMachine",
        initial(target("idle")),
        state("idle",
            defer("EVENT"),
            transition(on("FINISH"), target("done"))
        ),
        final("done")
    );
    
    buildDeferredTable(*model);
    
    // Idle should have deferred event
    std::string idle_path = "/TestMachine/idle";
    auto idle_it = model->deferred_map.find(idle_path);
    CHECK(idle_it != model->deferred_map.end());
    CHECK(idle_it->second.size() == 1);
    
    // Final state should not have deferred events
    std::string done_path = "/TestMachine/done";
    auto done_it = model->deferred_map.find(done_path);
    bool done_empty = (done_it == model->deferred_map.end()) || done_it->second.empty();
    CHECK(done_empty);
}

TEST_CASE("DeferredMap - Multiple deferred same event") {
    auto model = define("TestMachine",
        initial(target("state1")),
        state("state1",
            defer("EVENT", "EVENT", "OTHER"), // Duplicate EVENT should only appear once in map
            transition(on("NEXT"), target("state2"))
        ),
        state("state2",
            transition(on("EVENT"), target("handled")),
            transition(on("OTHER"), target("handled"))
        ),
        state("handled")
    );
    
    buildDeferredTable(*model);
    
    std::string state1_path = "/TestMachine/state1";
    auto state1_it = model->deferred_map.find(state1_path);
    CHECK(state1_it != model->deferred_map.end());
    // Map should deduplicate
    CHECK(state1_it->second.size() == 2);
}

TEST_CASE("DeferredMap - Deferred events with transitions") {
    auto model = define("TestMachine",
        initial(target("busy")),
        state("busy",
            defer("REQUEST1", "REQUEST2"),
            transition(on("COMPLETE"), target("ready")),
            transition(on("REQUEST1"), effect([](Context&, Instance&, Event&) {
                // This transition exists but REQUEST1 is deferred
            }))
        ),
        state("ready",
            transition(on("REQUEST1"), target("processing1")),
            transition(on("REQUEST2"), target("processing2"))
        ),
        state("processing1"),
        state("processing2")
    );
    
    buildDeferredTable(*model);
    
    // Even though busy has a transition for REQUEST1, it should still be deferred
    std::string busy_path = "/TestMachine/busy";
    auto busy_it = model->deferred_map.find(busy_path);
    CHECK(busy_it != model->deferred_map.end());
    CHECK(busy_it->second.size() == 2);
}

TEST_CASE("DeferredMap - Multiple events in single defer") {
    auto model = define("TestMachine",
        initial(target("busy")),
        state("busy",
            defer("EVENT1", "EVENT2", "EVENT3", "EVENT4"),
            transition(on("COMPLETE"), target("ready"))
        ),
        state("ready",
            transition(on("EVENT1"), target("handled")),
            transition(on("EVENT2"), target("handled")),
            transition(on("EVENT3"), target("handled")),
            transition(on("EVENT4"), target("handled"))
        ),
        state("handled")
    );
    
    buildDeferredTable(*model);
    
    // busy state should have all 4 deferred events
    std::string busy_path = "/TestMachine/busy";
    auto busy_it = model->deferred_map.find(busy_path);
    CHECK(busy_it != model->deferred_map.end());
    CHECK(busy_it->second.size() == 4);
    
    // Verify each event is deferred
    std::string event1 = "EVENT1";
    std::string event2 = "EVENT2";
    std::string event3 = "EVENT3";
    std::string event4 = "EVENT4";
    CHECK(busy_it->second.find(event1) != busy_it->second.end());
    CHECK(busy_it->second.find(event2) != busy_it->second.end());
    CHECK(busy_it->second.find(event3) != busy_it->second.end());
    CHECK(busy_it->second.find(event4) != busy_it->second.end());
}

TEST_CASE("DeferredMap - Empty deferred maps") {
    auto model = define("TestMachine",
        initial(target("state1")),
        state("state1",
            transition(on("EVENT"), target("state2"))
        ),
        state("state2",
            transition(on("EVENT"), target("state1"))
        )
    );
    
    buildDeferredTable(*model);
    
    // States without defer() should not have entries or have empty maps
    std::string state1_path = "/TestMachine/state1";
    auto state1_it = model->deferred_map.find(state1_path);
    bool state1_empty = (state1_it == model->deferred_map.end()) || state1_it->second.empty();
    CHECK(state1_empty);
    
    std::string state2_path = "/TestMachine/state2";
    auto state2_it = model->deferred_map.find(state2_path);
    bool state2_empty = (state2_it == model->deferred_map.end()) || state2_it->second.empty();
    CHECK(state2_empty);
}