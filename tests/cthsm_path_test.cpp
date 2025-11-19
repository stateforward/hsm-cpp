#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "cthsm/cthsm.hpp"

#include <vector>
#include <string>

using namespace cthsm;

struct PathTestInstance : public Instance {
    std::vector<std::string> execution_log;
    
    void log(const std::string& message) {
        execution_log.push_back(message);
    }
    
    void clear() {
        execution_log.clear();
    }
};

void entry_parent(Context&, Instance& i, const Event&) { static_cast<PathTestInstance&>(i).log("entry_parent"); }
void exit_parent(Context&, Instance& i, const Event&) { static_cast<PathTestInstance&>(i).log("exit_parent"); }
void entry_child(Context&, Instance& i, const Event&) { static_cast<PathTestInstance&>(i).log("entry_child"); }
void entry_state1(Context&, Instance& i, const Event&) { static_cast<PathTestInstance&>(i).log("entry_state1"); }
void entry_state2(Context&, Instance& i, const Event&) { static_cast<PathTestInstance&>(i).log("entry_state2"); }

TEST_CASE("Path Resolution") {
    SUBCASE("Relative Path to Direct Child") {
        constexpr auto model = define(
            "TestMachine",
            initial(target("/TestMachine/parent")),
            state("parent",
                entry(entry_parent),
                exit(exit_parent),
                // Relative path "child" should resolve to /TestMachine/parent/child
                transition(on("TO_CHILD"), target("child")),
                state("child",
                    entry(entry_child)
                )
            )
        );
        
        compile<model, PathTestInstance> sm;
        PathTestInstance instance;
        sm.start(instance);
        
        CHECK(sm.state() == "/TestMachine/parent");
        instance.clear();
        
        sm.dispatch(instance, "TO_CHILD");
        
        // cthsm supports child resolution
        CHECK(sm.state() == "/TestMachine/parent/child");
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "entry_child");
    }
    
    SUBCASE("Model-Level Transitions (Sibling Resolution)") {
        constexpr auto model = define(
            "ModelLevel",
            initial(target("/ModelLevel/state1")),
            // Transition defined at model level
            // Target "state2" should resolve to /ModelLevel/state2 (child of model/root)
            transition(on("TO_STATE2"), target("state2")),
            state("state1",
                entry(entry_state1)
            ),
            state("state2",
                entry(entry_state2)
            )
        );
        
        compile<model, PathTestInstance> sm;
        PathTestInstance instance;
        sm.start(instance);
        
        CHECK(sm.state() == "/ModelLevel/state1");
        instance.clear();
        
        sm.dispatch(instance, "TO_STATE2");
        
        CHECK(sm.state() == "/ModelLevel/state2");
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "entry_state2");
    }
    
    SUBCASE("Absolute Path Not Under Model (Not Supported by cthsm logic yet?)") {
        // cthsm normalize.hpp find_state_id iterates all states.
        // If we define a state with name not under model... wait.
        // All states in cthsm are collected recursively under the model.
        // So all states start with /ModelName/...
        // So "Absolute Path Not Under Model" is impossible to define in cthsm DSL 
        // because define("Name", ...) automatically prefixes all children.
        // Unless we have a way to escape the prefix? No.
        // So skipping this test case as irrelevant for cthsm.
    }
    
    SUBCASE("Sibling Resolution from State") {
        constexpr auto model = define(
            "SiblingRes",
            initial(target("/SiblingRes/s1")),
            state("s1",
                // "s2" is a sibling of s1.
                // resolve_target checks child, then sibling.
                // Sibling check: parent(s1) is root. root/s2 exists.
                transition(on("GO"), target("s2"))
            ),
            state("s2")
        );
        
        compile<model, PathTestInstance> sm;
        PathTestInstance instance;
        sm.start(instance);
        
        CHECK(sm.state() == "/SiblingRes/s1");
        sm.dispatch(instance, "GO");
        CHECK(sm.state() == "/SiblingRes/s2");
    }
}
