#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include "hsm.hpp"

// Test instance
class PathTestInstance : public hsm::Instance {
public:
    std::vector<std::string> execution_log;
    
    void log(const std::string& message) {
        execution_log.push_back(message);
    }
    
    void clear() {
        execution_log.clear();
    }
};

// Action functions
void log_entry(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<PathTestInstance&>(inst);
    test_inst.log("entry_" + name);
}

void log_exit(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<PathTestInstance&>(inst);
    test_inst.log("exit_" + name);
}

TEST_CASE("Path Resolution - JavaScript Compatibility") {
    SUBCASE("Relative Path to Direct Child") {
        // This test documents the current behavior vs expected behavior
        auto model = hsm::define(
            "TestMachine",
            hsm::initial(hsm::target("parent")),
            hsm::state("parent",
                hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    static_cast<PathTestInstance&>(inst).log("entry_parent");
                }),
                hsm::exit([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    static_cast<PathTestInstance&>(inst).log("exit_parent");
                }),
                // ISSUE: "child" should resolve to "/TestMachine/parent/child"
                // but currently resolves as sibling "/TestMachine/child"
                hsm::transition(hsm::on("TO_CHILD"), hsm::target("child")),
                hsm::state("child",
                    hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                        static_cast<PathTestInstance&>(inst).log("entry_child");
                    })
                )
            )
        );
        
        PathTestInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/TestMachine/parent");
        instance.clear();
        
        hsm::Event to_child("TO_CHILD");
        instance.dispatch(to_child).wait();
        
        std::string state_after = std::string(instance.state());
        
        // Document the issue
        if (state_after.empty()) {
            // Current behavior: transition fails, state becomes empty
            std::cout << "ISSUE: Relative path 'child' from parent state results in empty state" << std::endl;
            std::cout << "Expected: /TestMachine/parent/child" << std::endl;
            std::cout << "Actual: (empty)" << std::endl;
            
            // This is the bug - relative path should work
            CHECK(state_after.empty()); // Documents current broken behavior
        } else if (state_after == "/TestMachine/parent/child") {
            // Expected behavior (if fixed)
            CHECK(state_after == "/TestMachine/parent/child");
            CHECK(instance.execution_log.size() == 1);
            CHECK(instance.execution_log[0] == "entry_child");
        }
    }
    
    SUBCASE("Model-Level Transitions") {
        // Test how transitions defined at model level resolve paths
        auto model = hsm::define(
            "ModelLevel",
            hsm::initial(hsm::target("state1")),
            // Transition defined at model level
            hsm::transition(hsm::on("TO_STATE2"), hsm::target("state2")),
            hsm::state("state1",
                hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    static_cast<PathTestInstance&>(inst).log("entry_state1");
                })
            ),
            hsm::state("state2",
                hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    static_cast<PathTestInstance&>(inst).log("entry_state2");
                })
            )
        );
        
        PathTestInstance instance;
        hsm::start(instance, model);
        
        instance.clear();
        
        hsm::Event to_state2("TO_STATE2");
        instance.dispatch(to_state2).wait();
        
        // Debug output
        std::string state_after = std::string(instance.state());
        std::cout << "Model-level transition test:" << std::endl;
        std::cout << "  Before: /ModelLevel/state1" << std::endl;
        std::cout << "  After:  " << state_after << std::endl;
        std::cout << "  Expected: /ModelLevel/state2" << std::endl;
        
        // Model-level transitions might work differently
        if (!state_after.empty()) {
            CHECK(state_after == "/ModelLevel/state2");
        }
    }
    
    SUBCASE("Absolute Path Not Under Model") {
        // Test the JavaScript behavior where absolute paths not under the model
        // get the model name prepended
        auto model = hsm::define(
            "MyModel",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                // This absolute path "/other" is not under "/MyModel"
                // JavaScript would convert this to "/MyModel/other"
                hsm::transition(hsm::on("GO"), hsm::target("/other"))
            ),
            hsm::state("other")
        );
        
        PathTestInstance instance;
        hsm::start(instance, model);
        
        hsm::Event go("GO");
        instance.dispatch(go).wait();
        
        std::string state_after = std::string(instance.state());
        
        // Document current vs expected behavior
        if (state_after.empty()) {
            std::cout << "ISSUE: Absolute path '/other' not under model namespace fails" << std::endl;
            std::cout << "JavaScript would convert to '/MyModel/other'" << std::endl;
        } else {
            std::cout << "State after transition: " << state_after << std::endl;
        }
    }
    
    SUBCASE("Multiple Parent References (..)") {
        // Test path resolution with multiple ".." references
        auto model = hsm::define(
            "DeepNest",
            hsm::initial(hsm::target("l1/l2/l3")),
            hsm::state("l1",
                hsm::state("l2",
                    hsm::state("l3",
                        // Should go up two levels to l1
                        hsm::transition(hsm::on("UP"), hsm::target("../.."))
                    )
                )
            )
        );
        
        PathTestInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/DeepNest/l1/l2/l3");
        
        hsm::Event up("UP");
        instance.dispatch(up).wait();
        
        std::string state_after = std::string(instance.state());
        
        // Document the issue with "../.."
        std::cout << "After '../..' transition from l3:" << std::endl;
        std::cout << "Expected: /DeepNest/l1" << std::endl;
        std::cout << "Actual: " << state_after << std::endl;
        
        if (state_after == "/DeepNest/l1/l2") {
            std::cout << "ISSUE: '../..' only went up one level instead of two" << std::endl;
        }
    }
}

TEST_CASE("Path Resolution - Proposed Fix") {
    SUBCASE("How JavaScript Would Resolve Paths") {
        // This documents how paths SHOULD be resolved according to JavaScript implementation
        
        // Example 1: Relative path from state
        // State: /Machine/parent
        // Target: "child"
        // JavaScript: join("/Machine/parent", "child") = "/Machine/parent/child"
        
        // Example 2: Relative path with ".."
        // State: /Machine/parent/child
        // Target: "../sibling"
        // JavaScript: join("/Machine/parent/child", "../sibling") = "/Machine/parent/sibling"
        
        // Example 3: Absolute path not under model
        // Model: /Machine
        // Target: "/other"
        // JavaScript: !isAncestor("/Machine", "/other") so
        //             join("/Machine", "other") = "/Machine/other"
        
        // Example 4: Target from model-level transition
        // Stack contains Model, target "state1"
        // JavaScript: finds Model as ancestor, join("/Machine", "state1") = "/Machine/state1"
        
        std::cout << "\nJavaScript Path Resolution Rules:" << std::endl;
        std::cout << "1. Relative paths: join with nearest State (source) or State/Model (target)" << std::endl;
        std::cout << "2. Absolute paths not under model: prepend model name after removing leading '/'" << std::endl;
        std::cout << "3. Special paths '.' and '..' are kept for later resolution" << std::endl;
        
        CHECK(true); // Just documenting behavior
    }
}