#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#include "hsm.hpp"

// Test instance for final states
class FinalStatesInstance : public hsm::Instance {
public:
    std::vector<std::string> execution_log;
    std::atomic<bool> final_reached{false};
    
    void log(const std::string& message) {
        execution_log.push_back(message);
    }
    
    void clear() {
        execution_log.clear();
        final_reached = false;
    }
};

// Action functions
void log_entry(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<FinalStatesInstance&>(inst);
    test_inst.log("entry_" + name);
}

void log_exit(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<FinalStatesInstance&>(inst);
    test_inst.log("exit_" + name);
}

void mark_final_reached(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<FinalStatesInstance&>(inst);
    test_inst.final_reached = true;
    test_inst.log("final_reached");
}

TEST_CASE("Final States - Basic Functionality") {
    SUBCASE("Simple Final State") {
        auto model = hsm::define(
            "SimpleFinal",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("start", ctx, inst, event);
                }),
                hsm::transition(hsm::on("FINISH"), hsm::target("../end"))
            ),
            hsm::final("end")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/SimpleFinal/start");
        CHECK_FALSE(instance.final_reached.load());
        
        // Transition to final state
        instance.dispatch(hsm::Event("FINISH")).wait();
        
        CHECK(instance.state() == "/SimpleFinal/end");
        
        // Check execution order
        REQUIRE(instance.execution_log.size() >= 1);
        CHECK(instance.execution_log[0] == "entry_start");
    }

    SUBCASE("Final State with Multiple Paths") {
        auto model = hsm::define(
            "MultipleFinal",
            hsm::initial(hsm::target("active")),
            hsm::state("active",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("active", ctx, inst, event);
                }),
                hsm::transition(hsm::on("SUCCESS"), hsm::target("../success")),
                hsm::transition(hsm::on("FAILURE"), hsm::target("../failure")),
                hsm::transition(hsm::on("CANCEL"), hsm::target("../cancelled"))
            ),
            hsm::final("success"),
            hsm::final("failure"),
            hsm::final("cancelled")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/MultipleFinal/active");
        
        SUBCASE("Success Path") {
            instance.dispatch(hsm::Event("SUCCESS")).wait();
            CHECK(instance.state() == "/MultipleFinal/success");
        }
        
        SUBCASE("Failure Path") {
            instance.dispatch(hsm::Event("FAILURE")).wait();
            CHECK(instance.state() == "/MultipleFinal/failure");
        }
        
        SUBCASE("Cancel Path") {
            instance.dispatch(hsm::Event("CANCEL")).wait();
            CHECK(instance.state() == "/MultipleFinal/cancelled");
        }
    }
}

TEST_CASE("Final States - Semantic Restrictions") {
    SUBCASE("Final State Cannot Have Entry Actions") {
        // This tests that final states don't execute entry actions
        // even if they were somehow defined (which shouldn't happen)
        auto model = hsm::define(
            "FinalWithEntry",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("FINISH"), hsm::target("../end"))
            ),
            hsm::final("end")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        instance.dispatch(hsm::Event("FINISH")).wait();
        CHECK(instance.state() == "/FinalWithEntry/end");
        
        // Final state should not have executed any entry actions
        // (our final state doesn't have any defined anyway)
        bool found_final_entry = false;
        for (const auto& log : instance.execution_log) {
            if (log == "entry_end") {
                found_final_entry = true;
                break;
            }
        }
        CHECK_FALSE(found_final_entry);
    }

    SUBCASE("Final State Cannot Have Exit Actions") {
        // Final states should not have exit actions since they are terminal
        auto model = hsm::define(
            "FinalWithExit",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("FINISH"), hsm::target("../end"))
            ),
            hsm::final("end")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        instance.dispatch(hsm::Event("FINISH")).wait();
        CHECK(instance.state() == "/FinalWithExit/end");
        
        // Try to send another event (should be ignored)
        instance.dispatch(hsm::Event("IGNORED")).wait();
        CHECK(instance.state() == "/FinalWithExit/end");
    }

    SUBCASE("Final State Cannot Have Outgoing Transitions") {
        // Final states should not respond to events once reached
        auto model = hsm::define(
            "FinalNoTransitions",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("FINISH"), hsm::target("../end"))
            ),
            hsm::final("end")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        instance.dispatch(hsm::Event("FINISH")).wait();
        CHECK(instance.state() == "/FinalNoTransitions/end");
        
        // Try various events - all should be ignored
        instance.dispatch(hsm::Event("RESTART")).wait();
        CHECK(instance.state() == "/FinalNoTransitions/end");
        
        instance.dispatch(hsm::Event("CONTINUE")).wait();
        CHECK(instance.state() == "/FinalNoTransitions/end");
        
        instance.dispatch(hsm::Event("ANY_EVENT")).wait();
        CHECK(instance.state() == "/FinalNoTransitions/end");
    }
}

TEST_CASE("Final States - Hierarchical Scenarios") {
    SUBCASE("Final State in Nested State") {
        auto model = hsm::define(
            "NestedFinal",
            hsm::initial(hsm::target("container")),
            hsm::state("container",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("container", ctx, inst, event);
                }),
                hsm::initial(hsm::target("working")),
                hsm::state("working",
                    hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_entry("working", ctx, inst, event);
                    }),
                    hsm::transition(hsm::on("COMPLETE"), hsm::target("../done"))
                ),
                hsm::final("done"),
                hsm::transition(hsm::on("RESET"), hsm::target("../reset"))
            ),
            hsm::state("reset",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("reset", ctx, inst, event);
                })
            )
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/NestedFinal/container/working");
        
        // Complete the nested work
        instance.dispatch(hsm::Event("COMPLETE")).wait();
        CHECK(instance.state() == "/NestedFinal/container/done");
        
        // Parent state should still be able to handle events
        instance.dispatch(hsm::Event("RESET")).wait();
        CHECK(instance.state() == "/NestedFinal/reset");
        
        // Check execution order
        REQUIRE(instance.execution_log.size() >= 3);
        CHECK(instance.execution_log[0] == "entry_container");
        CHECK(instance.execution_log[1] == "entry_working");
        CHECK(instance.execution_log[2] == "entry_reset");
    }

    SUBCASE("Multiple Final States in Hierarchy") {
        auto model = hsm::define(
            "HierarchicalFinals",
            hsm::initial(hsm::target("level1")),
            hsm::state("level1",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("level1", ctx, inst, event);
                }),
                hsm::initial(hsm::target("level2")),
                hsm::state("level2",
                    hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_entry("level2", ctx, inst, event);
                    }),
                    hsm::transition(hsm::on("FINISH_INNER"), hsm::target("../inner_done"))
                ),
                hsm::final("inner_done"),
                hsm::transition(hsm::on("FINISH_OUTER"), hsm::target("../outer_done"))
            ),
            hsm::final("outer_done")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/HierarchicalFinals/level1/level2");
        
        SUBCASE("Inner Final First") {
            // Finish inner state first
            instance.dispatch(hsm::Event("FINISH_INNER")).wait();
            CHECK(instance.state() == "/HierarchicalFinals/level1/inner_done");
            
            // Then finish outer state
            instance.dispatch(hsm::Event("FINISH_OUTER")).wait();
            CHECK(instance.state() == "/HierarchicalFinals/outer_done");
        }
        
        SUBCASE("Outer Final Direct") {
            // Finish outer state directly (should exit inner state)
            instance.dispatch(hsm::Event("FINISH_OUTER")).wait();
            CHECK(instance.state() == "/HierarchicalFinals/outer_done");
        }
    }
}

TEST_CASE("Final States - Error Conditions and Edge Cases") {
    SUBCASE("Final State as Initial Target") {
        // This tests the edge case where initial transition targets a final state
        auto model = hsm::define(
            "ImmediateFinal",
            hsm::initial(hsm::target("end")),
            hsm::final("end")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        // Should go directly to final state
        CHECK(instance.state() == "/ImmediateFinal/end");
    }

    SUBCASE("Final State with Deferred Events") {
        // Final states should not defer events since they don't process them
        auto model = hsm::define(
            "FinalWithDeferred",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("FINISH"), hsm::target("../end"))
            ),
            hsm::final("end")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        instance.dispatch(hsm::Event("FINISH")).wait();
        CHECK(instance.state() == "/FinalWithDeferred/end");
        
        // Events sent to final state should not be deferred or processed
        instance.dispatch(hsm::Event("DEFERRED_EVENT")).wait();
        CHECK(instance.state() == "/FinalWithDeferred/end");
    }

    SUBCASE("Rapid Transitions to Final State") {
        auto model = hsm::define(
            "RapidFinal",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("start", ctx, inst, event);
                }),
                hsm::transition(hsm::on("STEP1"), hsm::target("../step1"))
            ),
            hsm::state("step1",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("step1", ctx, inst, event);
                }),
                hsm::transition(hsm::on("STEP2"), hsm::target("../step2"))
            ),
            hsm::state("step2",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("step2", ctx, inst, event);
                }),
                hsm::transition(hsm::on("FINAL"), hsm::target("../end"))
            ),
            hsm::final("end")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/RapidFinal/start");
        
        // Rapid succession of events leading to final state
        instance.dispatch(hsm::Event("STEP1")).wait();
        CHECK(instance.state() == "/RapidFinal/step1");
        
        instance.dispatch(hsm::Event("STEP2")).wait();
        CHECK(instance.state() == "/RapidFinal/step2");
        
        instance.dispatch(hsm::Event("FINAL")).wait();
        CHECK(instance.state() == "/RapidFinal/end");
        
        // Verify all entry actions were called
        REQUIRE(instance.execution_log.size() >= 3);
        CHECK(instance.execution_log[0] == "entry_start");
        CHECK(instance.execution_log[1] == "entry_step1");
        CHECK(instance.execution_log[2] == "entry_step2");
    }

    SUBCASE("Final State Behavior Under Load") {
        auto model = hsm::define(
            "LoadTestFinal",
            hsm::initial(hsm::target("active")),
            hsm::state("active",
                hsm::transition(hsm::on("FINISH"), hsm::target("../done"))
            ),
            hsm::final("done")
        );

        FinalStatesInstance instance;
        hsm::start(instance, model);
        
        // Transition to final state
        instance.dispatch(hsm::Event("FINISH")).wait();
        CHECK(instance.state() == "/LoadTestFinal/done");
        
        // Send many events to final state - all should be ignored
        for (int i = 0; i < 100; ++i) {
            instance.dispatch(hsm::Event("EVENT_" + std::to_string(i))).wait();
            CHECK(instance.state() == "/LoadTestFinal/done");
        }
    }
}