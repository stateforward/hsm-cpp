#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <any>
#include <unordered_map>

#include "hsm.hpp"

// Test instance to track behavior execution
class InitialTestInstance : public hsm::Instance {
public:
    std::vector<std::string> execution_log;
    std::atomic<int> effect_count{0};
    std::atomic<int> entry_count{0};
    std::unordered_map<std::string, std::any> data;

    void log(const std::string& message) { 
        execution_log.push_back(message); 
    }

    void clear_log() { 
        execution_log.clear(); 
        effect_count = 0;
        entry_count = 0;
    }
};

// Test behavior functions that log execution
void initial_effect_simple(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<InitialTestInstance&>(inst);
    test_inst.log("initial_effect_simple");
    test_inst.effect_count++;
}

void initial_effect_data(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<InitialTestInstance&>(inst);
    test_inst.log("initial_effect_data");
    test_inst.data["initial_setup"] = std::string("done");
    test_inst.effect_count++;
}

void entry_start(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<InitialTestInstance&>(inst);
    test_inst.log("entry_start");
    test_inst.entry_count++;
}

void entry_nested_start(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<InitialTestInstance&>(inst);
    test_inst.log("entry_nested_start");
    test_inst.entry_count++;
}

void entry_parent(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<InitialTestInstance&>(inst);
    test_inst.log("entry_parent");
    test_inst.entry_count++;
}

TEST_CASE("Initial Transitions - Comprehensive Testing") {
    SUBCASE("Basic Initial Transition") {
        auto model = hsm::define(
            "BasicInitial",
            hsm::initial(hsm::target("start")),
            hsm::state("start", hsm::entry(entry_start)),
            hsm::state("other")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Check that initial transition worked
        CHECK(instance.state() == "/BasicInitial/start");
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "entry_start");
        CHECK(instance.entry_count == 1);
    }

    SUBCASE("Initial Transition with Effect") {
        auto model = hsm::define(
            "InitialWithEffect",
            hsm::initial(hsm::target("start"), hsm::effect(initial_effect_simple)),
            hsm::state("start", hsm::entry(entry_start)),
            hsm::state("other")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Check that both effect and entry were executed
        CHECK(instance.state() == "/InitialWithEffect/start");
        CHECK(instance.execution_log.size() == 2);
        CHECK(instance.execution_log[0] == "initial_effect_simple");
        CHECK(instance.execution_log[1] == "entry_start");
        CHECK(instance.effect_count == 1);
        CHECK(instance.entry_count == 1);
    }

    SUBCASE("Initial Transition with Data Setup") {
        auto model = hsm::define(
            "InitialWithData",
            hsm::initial(hsm::target("start"), hsm::effect(initial_effect_data)),
            hsm::state("start", hsm::entry(entry_start)),
            hsm::state("ready")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Check that data was set up during initial transition
        CHECK(instance.state() == "/InitialWithData/start");
        CHECK(instance.data.count("initial_setup") == 1);
        CHECK(std::any_cast<std::string>(instance.data["initial_setup"]) == "done");
        CHECK(instance.execution_log.size() == 2);
    }

    SUBCASE("Hierarchical Initial Transition - Simple") {
        auto model = hsm::define(
            "HierarchicalInitial",
            hsm::initial(hsm::target("parent/child1")),
            hsm::state("parent",
                hsm::entry(entry_parent),
                hsm::state("child1", hsm::entry(entry_nested_start)),
                hsm::state("child2")
            ),
            hsm::state("other")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Check that hierarchical initial transition worked
        CHECK(instance.state() == "/HierarchicalInitial/parent/child1");
        CHECK(instance.execution_log.size() == 2);
        CHECK(instance.execution_log[0] == "entry_parent");
        CHECK(instance.execution_log[1] == "entry_nested_start");
        CHECK(instance.entry_count == 2);
    }

    SUBCASE("Nested Initial Transitions") {
        auto model = hsm::define(
            "NestedInitials",
            hsm::initial(hsm::target("parent/child1")),
            hsm::state("parent",
                hsm::entry(entry_parent),
                hsm::state("child1", hsm::entry(entry_nested_start)),
                hsm::state("child2")
            ),
            hsm::state("other")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Check that initial transition to nested state works correctly
        CHECK(instance.state() == "/NestedInitials/parent/child1");
        CHECK(instance.execution_log.size() == 2);
        CHECK(instance.execution_log[0] == "entry_parent");
        CHECK(instance.execution_log[1] == "entry_nested_start");
        CHECK(instance.entry_count == 2);
    }

    SUBCASE("Initial Transition to Root State") {
        auto model = hsm::define(
            "InitialToRoot",
            hsm::initial(hsm::target("/InitialToRoot/direct")),
            hsm::state("direct", hsm::entry(entry_start)),
            hsm::state("other")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Check absolute path initial transition
        CHECK(instance.state() == "/InitialToRoot/direct");
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "entry_start");
    }

    SUBCASE("Multiple Level Hierarchical Initial") {
        auto model = hsm::define(
            "DeepHierarchy",
            hsm::initial(hsm::target("level1/level2/level3")),
            hsm::state("level1",
                hsm::entry(entry_parent),
                hsm::state("level2",
                    hsm::entry(entry_nested_start),
                    hsm::state("level3", hsm::entry(entry_start)),
                    hsm::state("level3_alt")
                ),
                hsm::state("level2_alt")
            ),
            hsm::state("other")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Check deep hierarchical initial transition
        CHECK(instance.state() == "/DeepHierarchy/level1/level2/level3");
        CHECK(instance.execution_log.size() == 3);
        CHECK(instance.execution_log[0] == "entry_parent");
        CHECK(instance.execution_log[1] == "entry_nested_start");
        CHECK(instance.execution_log[2] == "entry_start");
        CHECK(instance.entry_count == 3);
    }

    SUBCASE("Initial Transition Order Verification") {
        std::vector<std::string> expected_order = {
            "initial_effect_simple",
            "entry_parent",
            "entry_nested_start"
        };

        auto model = hsm::define(
            "OrderVerification",
            hsm::initial(hsm::target("parent/child"), hsm::effect(initial_effect_simple)),
            hsm::state("parent",
                hsm::entry(entry_parent),
                hsm::state("child", hsm::entry(entry_nested_start))
            )
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Verify execution order: initial effect first, then parent entry, then child entry
        CHECK(instance.state() == "/OrderVerification/parent/child");
        CHECK(instance.execution_log == expected_order);
    }

    SUBCASE("Initial Transition with Relative Path") {
        auto model = hsm::define(
            "RelativePath",
            hsm::initial(hsm::target("../RelativePath/start")),  // Relative to root
            hsm::state("start", hsm::entry(entry_start)),
            hsm::state("other")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Should resolve to absolute path and work correctly
        CHECK(instance.state() == "/RelativePath/start");
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "entry_start");
    }

    SUBCASE("Initial Transition - State Data Persistence") {
        auto model = hsm::define(
            "DataPersistence",
            hsm::initial(hsm::target("start"), hsm::effect(initial_effect_data)),
            hsm::state("start", 
                hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    // Verify data set during initial effect is accessible
                    auto& test_inst = static_cast<InitialTestInstance&>(inst);
                    if (test_inst.data.count("initial_setup") > 0) {
                        test_inst.log("data_verified");
                    }
                })
            )
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        CHECK(instance.state() == "/DataPersistence/start");
        CHECK(instance.execution_log.size() == 2);
        CHECK(instance.execution_log[0] == "initial_effect_data");
        CHECK(instance.execution_log[1] == "data_verified");
    }
}

TEST_CASE("Initial Transitions - Edge Cases") {
    SUBCASE("No Initial Transition Specified") {
        // Machine without explicit initial - should have no active state initially
        auto model = hsm::define(
            "NoInitial",
            hsm::state("start"),
            hsm::state("other")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Without initial transition, no state should be active
        CHECK(instance.state() == "");
    }

    SUBCASE("Initial to Final State") {
        auto model = hsm::define(
            "InitialToFinal",
            hsm::initial(hsm::target("end")),
            hsm::state("start"),
            hsm::final("end")
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Should be able to transition directly to final state
        CHECK(instance.state() == "/InitialToFinal/end");
    }

    SUBCASE("Multiple Parallel Initial Contexts") {
        // Test direct initial transition to nested state
        auto model = hsm::define(
            "ParallelInitials",
            hsm::initial(hsm::target("region1/state1")),
            hsm::state("region1",
                hsm::state("state1", hsm::entry(entry_start)),
                hsm::state("state2")
            ),
            hsm::state("region2",
                hsm::state("state3"),
                hsm::state("state4")
            )
        );

        InitialTestInstance instance;
        hsm::start(instance, model);

        // Should enter region1/state1 based on machine-level initial
        CHECK(instance.state() == "/ParallelInitials/region1/state1");
        CHECK(instance.execution_log.size() == 1);
        CHECK(instance.execution_log[0] == "entry_start");
    }
}