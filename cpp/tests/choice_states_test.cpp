#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#include "hsm.hpp"

// Test instance for choice states
class ChoiceStatesInstance : public hsm::Instance {
public:
    std::vector<std::string> execution_log;
    std::atomic<int> value{0};
    std::atomic<bool> condition_a{false};
    std::atomic<bool> condition_b{false};
    
    void log(const std::string& message) {
        execution_log.push_back(message);
    }
    
    void clear() {
        execution_log.clear();
        value = 0;
        condition_a = false;
        condition_b = false;
    }
};

// Action functions
void log_entry(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ChoiceStatesInstance&>(inst);
    test_inst.log("entry_" + name);
}

void log_effect(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ChoiceStatesInstance&>(inst);
    test_inst.log("effect_" + name);
}

void increment_value(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ChoiceStatesInstance&>(inst);
    test_inst.value++;
    test_inst.log("increment_value");
}

void set_condition_a(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ChoiceStatesInstance&>(inst);
    test_inst.condition_a = true;
    test_inst.log("set_condition_a");
}

void set_condition_b(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<ChoiceStatesInstance&>(inst);
    test_inst.condition_b = true;
    test_inst.log("set_condition_b");
}

// Guard functions
bool guard_value_positive(hsm::Context& /*ctx*/, ChoiceStatesInstance& inst, hsm::Event& /*event*/) {
    return inst.value.load() > 0;
}

bool guard_value_even(hsm::Context& /*ctx*/, ChoiceStatesInstance& inst, hsm::Event& /*event*/) {
    return (inst.value.load() % 2) == 0;
}

bool guard_value_greater_than_5(hsm::Context& /*ctx*/, ChoiceStatesInstance& inst, hsm::Event& /*event*/) {
    return inst.value.load() > 5;
}

bool guard_condition_a(hsm::Context& /*ctx*/, ChoiceStatesInstance& inst, hsm::Event& /*event*/) {
    return inst.condition_a.load();
}

bool guard_condition_b(hsm::Context& /*ctx*/, ChoiceStatesInstance& inst, hsm::Event& /*event*/) {
    return inst.condition_b.load();
}

bool guard_always_true(hsm::Context& /*ctx*/, ChoiceStatesInstance& /*inst*/, hsm::Event& /*event*/) {
    return true;
}

bool guard_always_false(hsm::Context& /*ctx*/, ChoiceStatesInstance& /*inst*/, hsm::Event& /*event*/) {
    return false;
}

TEST_CASE("Choice States - Basic Functionality") {
    SUBCASE("Simple Choice with Two Options") {
        auto model = hsm::define(
            "SimpleChoice",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("start", ctx, inst, event);
                }),
                hsm::transition(hsm::on("EVALUATE"), hsm::target("../choice"))
            ),
            hsm::choice("choice",
                hsm::transition(hsm::guard(guard_value_positive), hsm::target("../positive")),
                hsm::transition(hsm::target("../negative"))  // Guardless fallback
            ),
            hsm::state("positive",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("positive", ctx, inst, event);
                })
            ),
            hsm::state("negative",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("negative", ctx, inst, event);
                })
            )
        );

        SUBCASE("Positive Path") {
            ChoiceStatesInstance instance;
            instance.value = 5;  // Positive value
            hsm::start(instance, model);
            
            CHECK(instance.state() == "/SimpleChoice/start");
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/SimpleChoice/positive");
            
            REQUIRE(instance.execution_log.size() >= 2);
            CHECK(instance.execution_log[0] == "entry_start");
            CHECK(instance.execution_log[1] == "entry_positive");
        }

        SUBCASE("Negative Path (Fallback)") {
            ChoiceStatesInstance instance;
            instance.value = -3;  // Negative value
            hsm::start(instance, model);
            
            CHECK(instance.state() == "/SimpleChoice/start");
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/SimpleChoice/negative");
            
            REQUIRE(instance.execution_log.size() >= 2);
            CHECK(instance.execution_log[0] == "entry_start");
            CHECK(instance.execution_log[1] == "entry_negative");
        }
    }

    SUBCASE("Choice with Multiple Guards") {
        auto model = hsm::define(
            "MultipleGuards",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("start", ctx, inst, event);
                }),
                hsm::transition(hsm::on("EVALUATE"), hsm::target("../choice"))
            ),
            hsm::choice("choice",
                hsm::transition(hsm::guard(guard_value_greater_than_5), hsm::target("../large")),
                hsm::transition(hsm::guard(guard_value_positive), hsm::target("../small_positive")),
                hsm::transition(hsm::guard(guard_value_even), hsm::target("../even")),
                hsm::transition(hsm::target("../other"))  // Guardless fallback
            ),
            hsm::state("large",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("large", ctx, inst, event);
                })
            ),
            hsm::state("small_positive",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("small_positive", ctx, inst, event);
                })
            ),
            hsm::state("even",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("even", ctx, inst, event);
                })
            ),
            hsm::state("other",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("other", ctx, inst, event);
                })
            )
        );

        SUBCASE("Large Value (First Guard)") {
            ChoiceStatesInstance instance;
            instance.value = 10;  // > 5, should take first guard
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/MultipleGuards/large");
        }

        SUBCASE("Small Positive Value (Second Guard)") {
            ChoiceStatesInstance instance;
            instance.value = 3;  // > 0 but <= 5, should take second guard
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/MultipleGuards/small_positive");
        }

        SUBCASE("Even Negative Value (Third Guard)") {
            ChoiceStatesInstance instance;
            instance.value = -4;  // negative but even, should take third guard
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/MultipleGuards/even");
        }

        SUBCASE("Odd Negative Value (Fallback)") {
            ChoiceStatesInstance instance;
            instance.value = -3;  // negative and odd, should take fallback
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/MultipleGuards/other");
        }
    }
}

TEST_CASE("Choice States - Guardless Fallback Requirements") {
    SUBCASE("Choice Must Have Guardless Fallback") {
        // This tests that a choice state works correctly with a guardless fallback
        auto model = hsm::define(
            "GuardlessFallback",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("CHOOSE"), hsm::target("../choice"))
            ),
            hsm::choice("choice",
                hsm::transition(hsm::guard(guard_always_false), hsm::target("../never")),
                hsm::transition(hsm::guard(guard_always_false), hsm::target("../also_never")),
                hsm::transition(hsm::target("../fallback"))  // REQUIRED guardless fallback
            ),
            hsm::state("never"),
            hsm::state("also_never"),
            hsm::state("fallback",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("fallback", ctx, inst, event);
                })
            )
        );

        ChoiceStatesInstance instance;
        hsm::start(instance, model);
        
        instance.dispatch(hsm::Event("CHOOSE")).wait();
        
        // Should always go to fallback since all guards are false
        CHECK(instance.state() == "/GuardlessFallback/fallback");
        
        bool found_fallback_entry = false;
        for (const auto& log : instance.execution_log) {
            if (log == "entry_fallback") {
                found_fallback_entry = true;
                break;
            }
        }
        CHECK(found_fallback_entry);
    }

    SUBCASE("Fallback Used When All Guards Fail") {
        auto model = hsm::define(
            "AllGuardsFail",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("TEST"), hsm::target("../choice"))
            ),
            hsm::choice("choice",
                hsm::transition(hsm::guard(guard_condition_a), hsm::target("../option_a")),
                hsm::transition(hsm::guard(guard_condition_b), hsm::target("../option_b")),
                hsm::transition(hsm::target("../default"))  // Fallback
            ),
            hsm::state("option_a"),
            hsm::state("option_b"),
            hsm::state("default",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("default", ctx, inst, event);
                })
            )
        );

        ChoiceStatesInstance instance;
        // Both conditions are false by default
        hsm::start(instance, model);
        
        instance.dispatch(hsm::Event("TEST")).wait();
        CHECK(instance.state() == "/AllGuardsFail/default");
    }
}

TEST_CASE("Choice States - Effects on Transitions") {
    SUBCASE("Choice Transitions with Effects") {
        auto model = hsm::define(
            "ChoiceWithEffects",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("GO"), hsm::target("../choice"))
            ),
            hsm::choice("choice",
                hsm::transition(
                    hsm::guard(guard_condition_a),
                    hsm::target("../path_a"),
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_effect("choice_to_a", ctx, inst, event);
                    })
                ),
                hsm::transition(
                    hsm::guard(guard_condition_b),
                    hsm::target("../path_b"),
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_effect("choice_to_b", ctx, inst, event);
                    })
                ),
                hsm::transition(
                    hsm::target("../default"),
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_effect("choice_to_default", ctx, inst, event);
                    })
                )
            ),
            hsm::state("path_a",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("path_a", ctx, inst, event);
                })
            ),
            hsm::state("path_b",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("path_b", ctx, inst, event);
                })
            ),
            hsm::state("default",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("default", ctx, inst, event);
                })
            )
        );

        SUBCASE("Path A with Effect") {
            ChoiceStatesInstance instance;
            instance.condition_a = true;
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("GO")).wait();
            CHECK(instance.state() == "/ChoiceWithEffects/path_a");
            
            // Check that effect was executed
            bool found_effect = false;
            for (const auto& log : instance.execution_log) {
                if (log == "effect_choice_to_a") {
                    found_effect = true;
                    break;
                }
            }
            CHECK(found_effect);
        }

        SUBCASE("Default Path with Effect") {
            ChoiceStatesInstance instance;
            // Both conditions false, should use default
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("GO")).wait();
            CHECK(instance.state() == "/ChoiceWithEffects/default");
            
            // Check that default effect was executed
            bool found_effect = false;
            for (const auto& log : instance.execution_log) {
                if (log == "effect_choice_to_default") {
                    found_effect = true;
                    break;
                }
            }
            CHECK(found_effect);
        }
    }
}

TEST_CASE("Choice States - Hierarchical Scenarios") {
    SUBCASE("Choice Inside Nested State") {
        auto model = hsm::define(
            "NestedChoice",
            hsm::initial(hsm::target("container")),
            hsm::state("container",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("container", ctx, inst, event);
                }),
                hsm::initial(hsm::target("start")),
                hsm::state("start",
                    hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_entry("start", ctx, inst, event);
                    }),
                    hsm::transition(hsm::on("DECIDE"), hsm::target("../choice"))
                ),
                hsm::choice("choice",
                    hsm::transition(hsm::guard(guard_value_positive), hsm::target("../positive")),
                    hsm::transition(hsm::target("../negative"))
                ),
                hsm::state("positive",
                    hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_entry("positive", ctx, inst, event);
                    })
                ),
                hsm::state("negative",
                    hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_entry("negative", ctx, inst, event);
                    })
                ),
                hsm::transition(hsm::on("EXIT"), hsm::target("../outside"))
            ),
            hsm::state("outside",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("outside", ctx, inst, event);
                })
            )
        );

        ChoiceStatesInstance instance;
        instance.value = 7;  // Positive
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/NestedChoice/container/start");
        
        instance.dispatch(hsm::Event("DECIDE")).wait();
        CHECK(instance.state() == "/NestedChoice/container/positive");
        
        // Exit the container state
        instance.dispatch(hsm::Event("EXIT")).wait();
        CHECK(instance.state() == "/NestedChoice/outside");
        
        // Verify execution order
        REQUIRE(instance.execution_log.size() >= 4);
        CHECK(instance.execution_log[0] == "entry_container");
        CHECK(instance.execution_log[1] == "entry_start");
        CHECK(instance.execution_log[2] == "entry_positive");
        CHECK(instance.execution_log[3] == "entry_outside");
    }

    SUBCASE("Multiple Choice States in Sequence") {
        auto model = hsm::define(
            "SequentialChoices",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("FIRST"), hsm::target("../choice1"))
            ),
            hsm::choice("choice1",
                hsm::transition(hsm::guard(guard_condition_a), hsm::target("../middle_a")),
                hsm::transition(hsm::target("../middle_b"))
            ),
            hsm::state("middle_a",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("middle_a", ctx, inst, event);
                }),
                hsm::transition(hsm::on("SECOND"), hsm::target("../choice2"))
            ),
            hsm::state("middle_b",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("middle_b", ctx, inst, event);
                }),
                hsm::transition(hsm::on("SECOND"), hsm::target("../choice2"))
            ),
            hsm::choice("choice2",
                hsm::transition(hsm::guard(guard_value_even), hsm::target("../end_even")),
                hsm::transition(hsm::target("../end_odd"))
            ),
            hsm::state("end_even",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("end_even", ctx, inst, event);
                })
            ),
            hsm::state("end_odd",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("end_odd", ctx, inst, event);
                })
            )
        );

        ChoiceStatesInstance instance;
        instance.condition_a = true;
        instance.value = 6;  // Even
        hsm::start(instance, model);
        
        instance.dispatch(hsm::Event("FIRST")).wait();
        CHECK(instance.state() == "/SequentialChoices/middle_a");
        
        instance.dispatch(hsm::Event("SECOND")).wait();
        CHECK(instance.state() == "/SequentialChoices/end_even");
        
        // Check execution path
        REQUIRE(instance.execution_log.size() >= 2);
        CHECK(instance.execution_log[0] == "entry_middle_a");
        CHECK(instance.execution_log[1] == "entry_end_even");
    }
}

TEST_CASE("Choice States - Error Conditions and Edge Cases") {
    SUBCASE("Choice State Cannot Be Initial Target") {
        // Choice states should not be direct initial targets
        // This should work but immediately resolve to a regular state
        auto model = hsm::define(
            "InitialChoice",
            hsm::initial(hsm::target("choice")),
            hsm::choice("choice",
                hsm::transition(hsm::guard(guard_always_true), hsm::target("../target"))
            ),
            hsm::state("target",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("target", ctx, inst, event);
                })
            )
        );

        ChoiceStatesInstance instance;
        hsm::start(instance, model);
        
        // Should resolve directly to target state
        CHECK(instance.state() == "/InitialChoice/target");
    }

    SUBCASE("Rapid Choice Transitions") {
        auto model = hsm::define(
            "RapidChoices",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("GO"), hsm::target("../choice"))
            ),
            hsm::choice("choice",
                hsm::transition(hsm::guard(guard_value_positive), hsm::target("../positive")),
                hsm::transition(hsm::target("../negative"))
            ),
            hsm::state("positive",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("positive", ctx, inst, event);
                }),
                hsm::transition(hsm::on("NEXT"), hsm::target("../choice"))
            ),
            hsm::state("negative",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("negative", ctx, inst, event);
                }),
                hsm::transition(hsm::on("NEXT"), hsm::target("../choice"))
            )
        );

        ChoiceStatesInstance instance;
        instance.value = 5;  // Start positive
        hsm::start(instance, model);
        
        // First choice
        instance.dispatch(hsm::Event("GO")).wait();
        CHECK(instance.state() == "/RapidChoices/positive");
        
        // Change condition and go through choice again
        instance.value = -2;
        instance.dispatch(hsm::Event("NEXT")).wait();
        CHECK(instance.state() == "/RapidChoices/negative");
        
        // Change condition again
        instance.value = 3;
        instance.dispatch(hsm::Event("NEXT")).wait();
        CHECK(instance.state() == "/RapidChoices/positive");
    }

    SUBCASE("Choice with Complex Guard Expressions") {
        auto model = hsm::define(
            "ComplexGuards",
            hsm::initial(hsm::target("start")),
            hsm::state("start",
                hsm::transition(hsm::on("EVALUATE"), hsm::target("../choice"))
            ),
            hsm::choice("choice",
                hsm::transition(
                    hsm::guard([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                        auto& test_inst = static_cast<ChoiceStatesInstance&>(inst);
                        return test_inst.value.load() > 0 && test_inst.condition_a.load();
                    }),
                    hsm::target("../positive_and_a")
                ),
                hsm::transition(
                    hsm::guard([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                        auto& test_inst = static_cast<ChoiceStatesInstance&>(inst);
                        return test_inst.value.load() > 0 && test_inst.condition_b.load();
                    }),
                    hsm::target("../positive_and_b")
                ),
                hsm::transition(
                    hsm::guard([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                        auto& test_inst = static_cast<ChoiceStatesInstance&>(inst);
                        return test_inst.value.load() > 0;
                    }),
                    hsm::target("../just_positive")
                ),
                hsm::transition(hsm::target("../other"))
            ),
            hsm::state("positive_and_a",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("positive_and_a", ctx, inst, event);
                })
            ),
            hsm::state("positive_and_b",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("positive_and_b", ctx, inst, event);
                })
            ),
            hsm::state("just_positive",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("just_positive", ctx, inst, event);
                })
            ),
            hsm::state("other",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("other", ctx, inst, event);
                })
            )
        );

        SUBCASE("Positive and A") {
            ChoiceStatesInstance instance;
            instance.value = 5;
            instance.condition_a = true;
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/ComplexGuards/positive_and_a");
        }

        SUBCASE("Just Positive") {
            ChoiceStatesInstance instance;
            instance.value = 3;
            // Neither condition_a nor condition_b is set
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/ComplexGuards/just_positive");
        }

        SUBCASE("Fallback") {
            ChoiceStatesInstance instance;
            instance.value = -1;  // Negative
            hsm::start(instance, model);
            
            instance.dispatch(hsm::Event("EVALUATE")).wait();
            CHECK(instance.state() == "/ComplexGuards/other");
        }
    }
}