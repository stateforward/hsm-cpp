#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "cthsm/cthsm.hpp"

#include <vector>
#include <string>
#include <algorithm>

using namespace cthsm;

struct TestInstance : public Instance {
    std::vector<std::string> log;
    
    void add_log(const std::string& msg) {
        log.push_back(msg);
    }
    
    void clear_log() {
        log.clear();
    }
    
    bool has_log(const std::string& msg) const {
        return std::find(log.begin(), log.end(), msg) != log.end();
    }
};

// Behaviors
void entry_a(Context&, Instance& i, const Event&) {
    static_cast<TestInstance&>(i).add_log("entry_a");
}

void exit_a(Context&, Instance& i, const Event&) {
    static_cast<TestInstance&>(i).add_log("exit_a");
}

void entry_b(Context&, Instance& i, const Event&) {
    static_cast<TestInstance&>(i).add_log("entry_b");
}

void effect_ab(Context&, Instance& i, const Event&) {
    static_cast<TestInstance&>(i).add_log("effect_ab");
}

bool guard_true(Context&, Instance&, const Event&) {
    return true;
}

bool guard_false(Context&, Instance&, const Event&) {
    return false;
}

TEST_CASE("Behaviors - Entry/Exit/Effect") {
    constexpr auto model = define("machine",
        initial(target("state_a")),
        state("state_a",
            entry(entry_a),
            exit(exit_a),
            transition(on("next"), target("state_b"), effect(effect_ab))
        ),
        state("state_b",
            entry(entry_b)
        )
    );
    
    compile<model, TestInstance> sm;
    TestInstance inst;
    sm.start(inst);
    
    // Check initial entry
    CHECK(inst.log.size() == 1);
    CHECK(inst.log[0] == "entry_a");
    CHECK(sm.state() == "/machine/state_a");
    
    inst.clear_log();
    
    // Dispatch
    sm.dispatch(inst, "next");
    
    // Check sequence: exit_a -> effect_ab -> entry_b
    CHECK(inst.log.size() == 3);
    CHECK(inst.log[0] == "exit_a");
    CHECK(inst.log[1] == "effect_ab");
    CHECK(inst.log[2] == "entry_b");
    CHECK(sm.state() == "/machine/state_b");
}

TEST_CASE("Behaviors - Guards") {
    constexpr auto model = define("machine",
        initial(target("start")),
        state("start",
            // High priority transition blocked by guard
            transition(on("go"), guard(guard_false), target("blocked")),
            // Fallback transition allowed
            transition(on("go"), guard(guard_true), target("allowed"))
        ),
        state("blocked"),
        state("allowed")
    );
    
    compile<model, TestInstance> sm;
    TestInstance inst;
    sm.start(inst);
    
    sm.dispatch(inst, "go");
    CHECK(sm.state() == "/machine/allowed");
}

TEST_CASE("Behaviors - Hierarchy Order") {
    constexpr auto model = define("machine",
        initial(target("p/c")),
        state("p",
            entry(entry_a),
            exit(exit_a),
            state("c",
                entry(entry_b),
                transition(on("out"), target("/machine/other"))
            )
        ),
        state("other")
    );
    
    compile<model, TestInstance> sm;
    TestInstance inst;
    sm.start(inst);
    
    // Entry: parent then child
    CHECK(inst.log.size() == 2);
    CHECK(inst.log[0] == "entry_a");
    CHECK(inst.log[1] == "entry_b");
    
    inst.clear_log();
    sm.dispatch(inst, "out");
    
    // Exit: child then parent
    // exit_a is parent, exit_child (none)
    // Wait, exit_a is parent.
    // Order: child exit (implicit) -> parent exit (exit_a) -> other entry
    CHECK(inst.log.size() == 1);
    CHECK(inst.log[0] == "exit_a");
}
