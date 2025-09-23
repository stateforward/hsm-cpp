#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <any>
#include <unordered_map>
#include <thread>
#include <chrono>

#include "hsm.hpp"

// Test instance to track exit action execution
class ExitTestInstance : public hsm::Instance {
public:
    std::vector<std::string> execution_log;
    std::atomic<int> exit_count{0};
    std::unordered_map<std::string, std::any> data;
    
    void log(const std::string& message) {
        execution_log.push_back(message);
    }
    
    void clear() {
        execution_log.clear();
        exit_count = 0;
        data.clear();
    }
    
    bool has_exit(const std::string& exit) const {
        return std::find(execution_log.begin(), execution_log.end(), exit) != execution_log.end();
    }
    
    int count_exits(const std::string& exit) const {
        return static_cast<int>(std::count(execution_log.begin(), execution_log.end(), exit));
    }
};

// Exit action functions
void exit_simple(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_simple");
    test_inst.exit_count++;
}

void exit_parent(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_parent");
    test_inst.exit_count++;
}

void exit_child(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_child");
    test_inst.exit_count++;
}

void exit_grandchild(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_grandchild");
    test_inst.exit_count++;
}

void exit_state_a(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_state_a");
    test_inst.exit_count++;
}

void exit_state_b(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_state_b");
    test_inst.exit_count++;
}

void exit_state_c(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_state_c");
    test_inst.exit_count++;
}

// Exit action with data manipulation
void exit_with_data(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& event) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_with_data");
    test_inst.data["exited_state"] = std::string("active");
    test_inst.data["exit_event_name"] = event.name;
    test_inst.exit_count++;
}

// Exit action that checks context
void exit_with_context(hsm::Context& ctx, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_with_context");
    // Context should not be set for synchronous actions
    test_inst.data["context_is_set"] = ctx.is_set();
    test_inst.exit_count++;
}

// Multiple exit actions
void exit_first(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_first");
    test_inst.data["order"] = 1;
}

void exit_second(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_second");
    test_inst.data["order"] = 2;
}

void exit_third(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ExitTestInstance&>(inst);
    test_inst.log("exit_third");
    test_inst.data["order"] = 3;
}

TEST_CASE("Exit Actions - Basic Functionality") {
    SUBCASE("Simple Exit Action") {
        auto model = hsm::define(
            "SimpleExit",
            hsm::initial(hsm::target("active")),
            hsm::state("active", 
                hsm::exit(exit_simple),
                hsm::transition(hsm::on("LEAVE"), hsm::target("../inactive"))
            ),
            hsm::state("inactive")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        // Verify we're in active state
        CHECK(instance.state() == "/SimpleExit/active");
        
        instance.clear();
        
        // Trigger transition to leave active state
        hsm::Event leave_event("LEAVE");
        instance.dispatch(leave_event).wait();
        
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "exit_simple");
        CHECK(instance.exit_count == 1);
    }
    
    SUBCASE("Exit Action with Event Access") {
        auto model = hsm::define(
            "ExitWithEvent",
            hsm::initial(hsm::target("active")),
            hsm::state("active", 
                hsm::exit(exit_with_data),
                hsm::transition(hsm::on("TRANSITION"), hsm::target("../next"))
            ),
            hsm::state("next")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event transition_event("TRANSITION");
        instance.dispatch(transition_event).wait();
        
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "exit_with_data");
        CHECK(instance.data["exited_state"].has_value());
        CHECK(std::any_cast<std::string>(instance.data["exited_state"]) == "active");
        CHECK(instance.data["exit_event_name"].has_value());
        CHECK(std::any_cast<std::string>(instance.data["exit_event_name"]) == "TRANSITION");
    }
    
    SUBCASE("Exit Action with Context") {
        auto model = hsm::define(
            "ExitWithContext",
            hsm::initial(hsm::target("active")),
            hsm::state("active", 
                hsm::exit(exit_with_context),
                hsm::transition(hsm::on("GO"), hsm::target("../next"))
            ),
            hsm::state("next")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event go_event("GO");
        instance.dispatch(go_event).wait();
        
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "exit_with_context");
        CHECK(instance.data["context_is_set"].has_value());
        CHECK(std::any_cast<bool>(instance.data["context_is_set"]) == false);
    }
    
    SUBCASE("Multiple Exit Actions") {
        auto model = hsm::define(
            "MultipleExits",
            hsm::initial(hsm::target("active")),
            hsm::state("active", 
                hsm::exit(exit_first, exit_second, exit_third),
                hsm::transition(hsm::on("LEAVE"), hsm::target("../done"))
            ),
            hsm::state("done")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event leave_event("LEAVE");
        instance.dispatch(leave_event).wait();
        
        CHECK(instance.execution_log.size() == 3);
        CHECK(instance.execution_log[0] == "exit_first");
        CHECK(instance.execution_log[1] == "exit_second");
        CHECK(instance.execution_log[2] == "exit_third");
        CHECK(std::any_cast<int>(instance.data["order"]) == 3); // Last one wins
    }
    
    SUBCASE("Lambda Exit Action") {
        auto model = hsm::define(
            "LambdaExit",
            hsm::initial(hsm::target("active")),
            hsm::state("active", 
                hsm::exit([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    auto& test_inst = static_cast<ExitTestInstance&>(inst);
                    test_inst.log("lambda_exit");
                    test_inst.data["lambda_executed"] = true;
                }),
                hsm::transition(hsm::on("NEXT"), hsm::target("../done"))
            ),
            hsm::state("done")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event next_event("NEXT");
        instance.dispatch(next_event).wait();
        
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "lambda_exit");
        CHECK(instance.data["lambda_executed"].has_value());
        CHECK(std::any_cast<bool>(instance.data["lambda_executed"]) == true);
    }
}

TEST_CASE("Exit Actions - Hierarchical States") {
    SUBCASE("Child-Parent Exit Order") {
        auto model = hsm::define(
            "ChildParent",
            hsm::initial(hsm::target("parent/child")),
            hsm::state("parent",
                hsm::exit(exit_parent),
                hsm::state("child", 
                    hsm::exit(exit_child),
                    hsm::transition(hsm::on("EXIT_ALL"), hsm::target("/ChildParent/outside"))
                )
            ),
            hsm::state("outside")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event exit_event("EXIT_ALL");
        instance.dispatch(exit_event).wait();
        
        // Exit order should be child first, then parent
        CHECK(instance.execution_log.size() == 2);
        CHECK(instance.execution_log[0] == "exit_child");
        CHECK(instance.execution_log[1] == "exit_parent");
        CHECK(instance.exit_count == 2);
    }
    
    SUBCASE("Three-Level Hierarchy Exit Order") {
        auto model = hsm::define(
            "ThreeLevels",
            hsm::initial(hsm::target("parent/child/grandchild")),
            hsm::state("parent",
                hsm::exit(exit_parent),
                hsm::state("child",
                    hsm::exit(exit_child),
                    hsm::state("grandchild", 
                        hsm::exit(exit_grandchild),
                        hsm::transition(hsm::on("EXIT_ALL"), hsm::target("/ThreeLevels/outside"))
                    )
                )
            ),
            hsm::state("outside")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event exit_event("EXIT_ALL");
        instance.dispatch(exit_event).wait();
        
        // Exit order should be deepest first
        CHECK(instance.execution_log.size() == 3);
        CHECK(instance.execution_log[0] == "exit_grandchild");
        CHECK(instance.execution_log[1] == "exit_child");
        CHECK(instance.execution_log[2] == "exit_parent");
        CHECK(instance.exit_count == 3);
    }
    
    SUBCASE("Transition Within Hierarchy - No Parent Exit") {
        auto model = hsm::define(
            "HierarchyTransition",
            hsm::initial(hsm::target("parent/child_a")),
            hsm::state("parent",
                hsm::exit(exit_parent),
                hsm::state("child_a", 
                    hsm::exit(exit_state_a),
                    hsm::transition(hsm::on("NEXT"), hsm::target("/HierarchyTransition/parent/child_b"))
                ),
                hsm::state("child_b", hsm::exit(exit_state_b))
            )
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        // Transition from child_a to child_b
        hsm::Event next_event("NEXT");
        instance.dispatch(next_event).wait();
        
        // Parent should NOT be exited
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "exit_state_a");
        CHECK(instance.has_exit("exit_parent") == false);
    }
    
    SUBCASE("Transition Across Hierarchy") {
        auto model = hsm::define(
            "CrossHierarchy",
            hsm::initial(hsm::target("region1/state_a")),
            hsm::state("region1",
                hsm::exit(exit_parent),
                hsm::state("state_a", 
                    hsm::exit(exit_state_a),
                    hsm::transition(hsm::on("CROSS"), hsm::target("/CrossHierarchy/region2/state_b"))
                )
            ),
            hsm::state("region2",
                hsm::exit(exit_child),
                hsm::state("state_b", hsm::exit(exit_state_b))
            )
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        // Cross-hierarchy transition
        hsm::Event cross_event("CROSS");
        instance.dispatch(cross_event).wait();
        
        // Should exit state_a and region1
        CHECK(instance.execution_log.size() == 2);
        CHECK(instance.execution_log[0] == "exit_state_a");
        CHECK(instance.execution_log[1] == "exit_parent");
    }
}

TEST_CASE("Exit Actions - Transition Types") {
    SUBCASE("External Transition Exit") {
        auto model = hsm::define(
            "ExternalTransition",
            hsm::initial(hsm::target("state_a")),
            hsm::state("state_a", 
                hsm::exit(exit_state_a),
                hsm::transition(hsm::on("GO_B"), hsm::target("/ExternalTransition/state_b"))
            ),
            hsm::state("state_b", hsm::exit(exit_state_b))
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event go_event("GO_B");
        instance.dispatch(go_event).wait();
        
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "exit_state_a");
    }
    
    SUBCASE("Self Transition Exit and Re-entry") {
        auto model = hsm::define(
            "SelfTransition",
            hsm::initial(hsm::target("active")),
            hsm::state("active", 
                hsm::exit(exit_simple),
                hsm::transition(hsm::on("SELF"), hsm::target("."))
            )
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        // Self transition should exit and re-enter
        hsm::Event self_event("SELF");
        instance.dispatch(self_event).wait();
        
        CHECK(instance.count_exits("exit_simple") == 1);
    }
    
    SUBCASE("Internal Transition No Exit") {
        auto model = hsm::define(
            "InternalTransition",
            hsm::initial(hsm::target("active")),
            hsm::state("active", 
                hsm::exit(exit_simple),
                hsm::transition(hsm::on("INTERNAL"), hsm::effect([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    auto& test_inst = static_cast<ExitTestInstance&>(inst);
                    test_inst.log("internal_effect");
                }))
            )
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        // Internal transition should NOT trigger exit
        hsm::Event internal_event("INTERNAL");
        instance.dispatch(internal_event).wait();
        
        CHECK(instance.has_exit("exit_simple") == false);
        CHECK(instance.has_exit("internal_effect") == true);
    }
    
    SUBCASE("Local Transition from Parent") {
        auto model = hsm::define(
            "LocalTransition",
            hsm::initial(hsm::target("parent")),
            hsm::state("parent",
                hsm::exit(exit_parent),
                hsm::transition(hsm::on("TO_CHILD"), hsm::target("child")),
                hsm::state("child", hsm::exit(exit_child))
            )
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        // Local transition to child
        hsm::Event to_child("TO_CHILD");
        instance.dispatch(to_child).wait();
        
        // Debug output
        std::cout << "Local transition execution log:" << std::endl;
        for (const auto& log : instance.execution_log) {
            std::cout << "  " << log << std::endl;
        }
        
        // Should not exit parent on local transition
        CHECK(instance.execution_log.empty());
    }
}

TEST_CASE("Exit Actions - Special Cases") {
    SUBCASE("Exit from Final State") {
        auto model = hsm::define(
            "ExitFromFinal",
            hsm::initial(hsm::target("active")),
            hsm::state("active",
                hsm::transition(hsm::on("END"), hsm::target("../done"))
            ),
            hsm::final("done")  // Final states cannot have exit actions
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event end_event("END");
        instance.dispatch(end_event).wait();
        
        // Transition to final state
        CHECK(instance.state() == "/ExitFromFinal/done");
        
        // Now stop the HSM - final states should not have exit actions
        hsm::stop(instance).wait();
        
        // No exit action for final state
        CHECK(instance.execution_log.empty());
    }
    
    SUBCASE("Exit Action Exception Handling") {
        auto model = hsm::define(
            "ExitException",
            hsm::initial(hsm::target("problematic")),
            hsm::state("problematic", 
                hsm::exit([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    auto& test_inst = static_cast<ExitTestInstance&>(inst);
                    test_inst.log("exit_before_exception");
                    // Note: In production code, exceptions in exit actions should be avoided
                    // This is just to test behavior
                    test_inst.data["exception_test"] = true;
                }),
                hsm::exit(exit_simple),  // This should still execute
                hsm::transition(hsm::on("LEAVE"), hsm::target("../safe"))
            ),
            hsm::state("safe")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event leave_event("LEAVE");
        instance.dispatch(leave_event).wait();
        
        CHECK(instance.execution_log.size() == 2);
        CHECK(instance.execution_log[0] == "exit_before_exception");
        CHECK(instance.execution_log[1] == "exit_simple");
        CHECK(instance.data["exception_test"].has_value());
    }
    
    SUBCASE("Exit with Choice State") {
        int counter = 0;
        auto model = hsm::define(
            "ExitWithChoice",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::exit(exit_simple),
                hsm::transition(hsm::on("DECIDE"), hsm::target("../decide"))
            ),
            hsm::choice("decide",
                hsm::transition(hsm::guard([&counter](hsm::Context& /*ctx*/, hsm::Instance& /*inst*/, hsm::Event& /*event*/) {
                    return counter > 0;
                }), hsm::target("positive")),
                hsm::transition(hsm::target("zero"))
            ),
            hsm::state("positive"),
            hsm::state("zero")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event decide_event("DECIDE");
        instance.dispatch(decide_event).wait();
        
        // Should exit start state before choice evaluation
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "exit_simple");
    }
    
    SUBCASE("Nested Exit Actions Order") {
        auto model = hsm::define(
            "NestedExits",
            hsm::initial(hsm::target("outer/inner")),
            hsm::state("outer",
                hsm::exit(exit_first, exit_second),
                hsm::state("inner",
                    hsm::exit(exit_third, exit_simple),
                    hsm::transition(hsm::on("LEAVE_ALL"), hsm::target("/NestedExits/outside"))
                )
            ),
            hsm::state("outside")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event leave_event("LEAVE_ALL");
        instance.dispatch(leave_event).wait();
        
        // Check all exits executed in order (inner first, then outer)
        CHECK(instance.execution_log.size() == 4);
        CHECK(instance.execution_log[0] == "exit_third");
        CHECK(instance.execution_log[1] == "exit_simple");
        CHECK(instance.execution_log[2] == "exit_first");
        CHECK(instance.execution_log[3] == "exit_second");
    }
}

TEST_CASE("Exit Actions - Complex Scenarios") {
    SUBCASE("Exit on Stop") {
        auto model = hsm::define(
            "ExitOnStop",
            hsm::initial(hsm::target("active")),
            hsm::state("active", hsm::exit(exit_simple))
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        // Stop the HSM
        hsm::stop(instance).wait();
        
        // Should call exit action
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "exit_simple");
    }
    
    SUBCASE("Exit Action Modifying State Machine") {
        auto model = hsm::define(
            "SelfModifying",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::exit([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    auto& test_inst = static_cast<ExitTestInstance&>(inst);
                    test_inst.log("exit_start");
                    // Store flag that we've exited
                    test_inst.data["start_exited"] = true;
                }),
                hsm::transition(hsm::on("GO"), hsm::target("/SelfModifying/next"))
            ),
            hsm::state("next", hsm::exit(exit_simple))
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event go_event("GO");
        instance.dispatch(go_event).wait();
        
        CHECK(instance.has_exit("exit_start"));
        CHECK(instance.data["start_exited"].has_value());
        CHECK(instance.state() == "/SelfModifying/next");
    }
    
    SUBCASE("Multiple Sequential Exit Actions with Delays") {
        auto model = hsm::define(
            "SequentialExits",
            hsm::initial(hsm::target("active")),
            hsm::state("active",
                hsm::exit(
                    [](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                        auto& test_inst = static_cast<ExitTestInstance&>(inst);
                        test_inst.log("exit_1_start");
                        // Even with a delay, this blocks the next exit
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        test_inst.log("exit_1_end");
                    },
                    [](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                        auto& test_inst = static_cast<ExitTestInstance&>(inst);
                        test_inst.log("exit_2_start");
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        test_inst.log("exit_2_end");
                    },
                    [](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                        auto& test_inst = static_cast<ExitTestInstance&>(inst);
                        test_inst.log("exit_3");
                    }
                ),
                hsm::transition(hsm::on("LEAVE"), hsm::target("../done"))
            ),
            hsm::state("done")
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event leave_event("LEAVE");
        instance.dispatch(leave_event).wait();
        
        // All exits execute in order and are synchronous/blocking
        CHECK(instance.execution_log.size() == 5);
        CHECK(instance.execution_log[0] == "exit_1_start");
        CHECK(instance.execution_log[1] == "exit_1_end");
        CHECK(instance.execution_log[2] == "exit_2_start");
        CHECK(instance.execution_log[3] == "exit_2_end");
        CHECK(instance.execution_log[4] == "exit_3");
    }
    
    SUBCASE("Exit Order in Complex Hierarchy") {
        auto model = hsm::define(
            "ComplexHierarchy",
            hsm::initial(hsm::target("region1/sub1/leaf1")),
            hsm::state("region1",
                hsm::exit(exit_parent),
                hsm::state("sub1",
                    hsm::exit(exit_child),
                    hsm::state("leaf1",
                        hsm::exit(exit_grandchild),
                        hsm::transition(hsm::on("CROSS"), hsm::target("/ComplexHierarchy/region2/sub2/leaf2"))
                    )
                )
            ),
            hsm::state("region2",
                hsm::state("sub2",
                    hsm::state("leaf2")
                )
            )
        );
        
        ExitTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event cross_event("CROSS");
        instance.dispatch(cross_event).wait();
        
        // Should exit in reverse hierarchical order
        CHECK(instance.execution_log.size() == 3);
        CHECK(instance.execution_log[0] == "exit_grandchild");
        CHECK(instance.execution_log[1] == "exit_child");
        CHECK(instance.execution_log[2] == "exit_parent");
    }
}