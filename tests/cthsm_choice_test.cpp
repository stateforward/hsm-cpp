#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "cthsm/cthsm.hpp"

#include <vector>
#include <string>
#include <algorithm>

using namespace cthsm;

struct ChoiceInstance : public Instance {
    std::vector<std::string> execution_log;
    int value{0};
    bool condition_a{false};
    bool condition_b{false};

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

// Behaviors
void entry_start(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_start"); }
void entry_positive(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_positive"); }
void entry_negative(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_negative"); }

void entry_large(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_large"); }
void entry_small_positive(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_small_positive"); }
void entry_even(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_even"); }
void entry_other(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_other"); }

void entry_fallback(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_fallback"); }
void entry_default(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_default"); }

void entry_path_a(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_path_a"); }
void entry_path_b(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_path_b"); }

void effect_choice_to_a(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("effect_choice_to_a"); }
void effect_choice_to_b(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("effect_choice_to_b"); }
void effect_choice_to_default(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("effect_choice_to_default"); }

void entry_container(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_container"); }
void entry_outside(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_outside"); }

void entry_middle_a(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_middle_a"); }
void entry_middle_b(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_middle_b"); }
void entry_end_even(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_end_even"); }
void entry_end_odd(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_end_odd"); }

void entry_target(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_target"); }

void entry_positive_and_a(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_positive_and_a"); }
void entry_positive_and_b(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_positive_and_b"); }
void entry_just_positive(Context&, Instance& i, const Event&) { static_cast<ChoiceInstance&>(i).log("entry_just_positive"); }


// Guards
bool guard_value_positive(Context&, Instance& i, const Event&) {
    return static_cast<ChoiceInstance&>(i).value > 0;
}
bool guard_value_even(Context&, Instance& i, const Event&) {
    return (static_cast<ChoiceInstance&>(i).value % 2) == 0;
}
bool guard_value_greater_than_5(Context&, Instance& i, const Event&) {
    return static_cast<ChoiceInstance&>(i).value > 5;
}
bool guard_condition_a(Context&, Instance& i, const Event&) {
    return static_cast<ChoiceInstance&>(i).condition_a;
}
bool guard_condition_b(Context&, Instance& i, const Event&) {
    return static_cast<ChoiceInstance&>(i).condition_b;
}
bool guard_always_true(Context&, Instance&, const Event&) { return true; }
bool guard_always_false(Context&, Instance&, const Event&) { return false; }

// Complex guards
bool guard_positive_and_a(Context&, Instance& i, const Event&) {
    auto& inst = static_cast<ChoiceInstance&>(i);
    return inst.value > 0 && inst.condition_a;
}
bool guard_positive_and_b(Context&, Instance& i, const Event&) {
    auto& inst = static_cast<ChoiceInstance&>(i);
    return inst.value > 0 && inst.condition_b;
}


TEST_CASE("Choice States - Basic Functionality") {
    SUBCASE("Simple Choice with Two Options") {
        constexpr auto model = define(
            "SimpleChoice",
            initial(target("/SimpleChoice/start")),
            state("start",
                entry(entry_start),
                transition(on("EVALUATE"), target("/SimpleChoice/choice"))
            ),
            choice("choice",
                transition(guard(guard_value_positive), target("/SimpleChoice/positive")),
                transition(target("/SimpleChoice/negative"))  // Guardless fallback
            ),
            state("positive",
                entry(entry_positive)
            ),
            state("negative",
                entry(entry_negative)
            )
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;

        SUBCASE("Positive Path") {
            instance.value = 5;
            sm.start(instance);
            
            CHECK(sm.state() == "/SimpleChoice/start");
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/SimpleChoice/positive");
            
            REQUIRE(instance.execution_log.size() >= 2);
            CHECK(instance.execution_log[0] == "entry_start");
            CHECK(instance.execution_log[1] == "entry_positive");
        }

        SUBCASE("Negative Path (Fallback)") {
            instance.value = -3;
            sm.start(instance);
            
            CHECK(sm.state() == "/SimpleChoice/start");
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/SimpleChoice/negative");
            
            REQUIRE(instance.execution_log.size() >= 2);
            CHECK(instance.execution_log[0] == "entry_start");
            CHECK(instance.execution_log[1] == "entry_negative");
        }
    }

    SUBCASE("Choice with Multiple Guards") {
        constexpr auto model = define(
            "MultipleGuards",
            initial(target("/MultipleGuards/start")),
            state("start",
                entry(entry_start),
                transition(on("EVALUATE"), target("/MultipleGuards/choice"))
            ),
            choice("choice",
                transition(guard(guard_value_greater_than_5), target("/MultipleGuards/large")),
                transition(guard(guard_value_positive), target("/MultipleGuards/small_positive")),
                transition(guard(guard_value_even), target("/MultipleGuards/even")),
                transition(target("/MultipleGuards/other"))  // Guardless fallback
            ),
            state("large", entry(entry_large)),
            state("small_positive", entry(entry_small_positive)),
            state("even", entry(entry_even)),
            state("other", entry(entry_other))
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;

        SUBCASE("Large Value (First Guard)") {
            instance.value = 10;
            sm.start(instance);
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/MultipleGuards/large");
        }

        SUBCASE("Small Positive Value (Second Guard)") {
            instance.value = 3;
            sm.start(instance);
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/MultipleGuards/small_positive");
        }

        SUBCASE("Even Negative Value (Third Guard)") {
            instance.value = -4;
            sm.start(instance);
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/MultipleGuards/even");
        }

        SUBCASE("Odd Negative Value (Fallback)") {
            instance.value = -3;
            sm.start(instance);
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/MultipleGuards/other");
        }
    }
}

TEST_CASE("Choice States - Guardless Fallback Requirements") {
    SUBCASE("Choice Must Have Guardless Fallback") {
        constexpr auto model = define(
            "GuardlessFallback",
            initial(target("/GuardlessFallback/start")),
            state("start",
                transition(on("CHOOSE"), target("/GuardlessFallback/choice"))
            ),
            choice("choice",
                transition(guard(guard_always_false), target("/GuardlessFallback/never")),
                transition(guard(guard_always_false), target("/GuardlessFallback/also_never")),
                transition(target("/GuardlessFallback/fallback"))
            ),
            state("never"),
            state("also_never"),
            state("fallback", entry(entry_fallback))
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;
        sm.start(instance);
        
        sm.dispatch(instance, "CHOOSE");
        
        CHECK(sm.state() == "/GuardlessFallback/fallback");
        
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
        constexpr auto model = define(
            "AllGuardsFail",
            initial(target("/AllGuardsFail/start")),
            state("start",
                transition(on("TEST"), target("/AllGuardsFail/choice"))
            ),
            choice("choice",
                transition(guard(guard_condition_a), target("/AllGuardsFail/option_a")),
                transition(guard(guard_condition_b), target("/AllGuardsFail/option_b")),
                transition(target("/AllGuardsFail/default"))
            ),
            state("option_a"),
            state("option_b"),
            state("default", entry(entry_default))
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;
        sm.start(instance);
        
        sm.dispatch(instance, "TEST");
        CHECK(sm.state() == "/AllGuardsFail/default");
    }
}

TEST_CASE("Choice States - Effects on Transitions") {
    SUBCASE("Choice Transitions with Effects") {
        constexpr auto model = define(
            "ChoiceWithEffects",
            initial(target("/ChoiceWithEffects/start")),
            state("start",
                transition(on("GO"), target("/ChoiceWithEffects/choice"))
            ),
            choice("choice",
                transition(
                    guard(guard_condition_a),
                    target("/ChoiceWithEffects/path_a"),
                    effect(effect_choice_to_a)
                ),
                transition(
                    guard(guard_condition_b),
                    target("/ChoiceWithEffects/path_b"),
                    effect(effect_choice_to_b)
                ),
                transition(
                    target("/ChoiceWithEffects/default"),
                    effect(effect_choice_to_default)
                )
            ),
            state("path_a", entry(entry_path_a)),
            state("path_b", entry(entry_path_b)),
            state("default", entry(entry_default))
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;

        SUBCASE("Path A with Effect") {
            instance.condition_a = true;
            sm.start(instance);
            
            sm.dispatch(instance, "GO");
            CHECK(sm.state() == "/ChoiceWithEffects/path_a");
            
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
            // Both conditions false
            sm.start(instance);
            
            sm.dispatch(instance, "GO");
            CHECK(sm.state() == "/ChoiceWithEffects/default");
            
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
        constexpr auto model = define(
            "NestedChoice",
            initial(target("/NestedChoice/container/start")),
            state("container",
                entry(entry_container),
                // initial(target("/NestedChoice/container/start")), // cthsm: initial transitions are properties of state or use global initial
                // Using direct target in global initial above
                state("start",
                    entry(entry_start),
                    transition(on("DECIDE"), target("/NestedChoice/container/choice"))
                ),
                choice("choice",
                    transition(guard(guard_value_positive), target("/NestedChoice/container/positive")),
                    transition(target("/NestedChoice/container/negative"))
                ),
                state("positive", entry(entry_positive)),
                state("negative", entry(entry_negative)),
                transition(on("EXIT"), target("/NestedChoice/outside"))
            ),
            state("outside", entry(entry_outside))
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;
        instance.value = 7;
        sm.start(instance);
        
        CHECK(sm.state() == "/NestedChoice/container/start");
        
        sm.dispatch(instance, "DECIDE");
        CHECK(sm.state() == "/NestedChoice/container/positive");
        
        // Exit the container state
        sm.dispatch(instance, "EXIT");
        CHECK(sm.state() == "/NestedChoice/outside");
        
        // Verify execution order
        REQUIRE(instance.execution_log.size() >= 4);
        CHECK(instance.execution_log[0] == "entry_container");
        CHECK(instance.execution_log[1] == "entry_start");
        CHECK(instance.execution_log[2] == "entry_positive");
        CHECK(instance.execution_log[3] == "entry_outside");
    }

    SUBCASE("Multiple Choice States in Sequence") {
        constexpr auto model = define(
            "SequentialChoices",
            initial(target("/SequentialChoices/start")),
            state("start",
                transition(on("FIRST"), target("/SequentialChoices/choice1"))
            ),
            choice("choice1",
                transition(guard(guard_condition_a), target("/SequentialChoices/middle_a")),
                transition(target("/SequentialChoices/middle_b"))
            ),
            state("middle_a",
                entry(entry_middle_a),
                transition(on("SECOND"), target("/SequentialChoices/choice2"))
            ),
            state("middle_b",
                entry(entry_middle_b),
                transition(on("SECOND"), target("/SequentialChoices/choice2"))
            ),
            choice("choice2",
                transition(guard(guard_value_even), target("/SequentialChoices/end_even")),
                transition(target("/SequentialChoices/end_odd"))
            ),
            state("end_even", entry(entry_end_even)),
            state("end_odd", entry(entry_end_odd))
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;
        instance.condition_a = true;
        instance.value = 6;
        sm.start(instance);
        
        sm.dispatch(instance, "FIRST");
        CHECK(sm.state() == "/SequentialChoices/middle_a");
        
        sm.dispatch(instance, "SECOND");
        CHECK(sm.state() == "/SequentialChoices/end_even");
        
        REQUIRE(instance.execution_log.size() >= 2);
        CHECK(instance.execution_log[0] == "entry_middle_a");
        CHECK(instance.execution_log[1] == "entry_end_even");
    }
}

TEST_CASE("Choice States - Error Conditions and Edge Cases") {
    SUBCASE("Choice State Cannot Be Initial Target") {
        constexpr auto model = define(
            "InitialChoice",
            initial(target("/InitialChoice/choice")),
            choice("choice",
                transition(guard(guard_always_true), target("/InitialChoice/target"))
            ),
            state("target", entry(entry_target))
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;
        sm.start(instance);
        
        CHECK(sm.state() == "/InitialChoice/target");
    }

    SUBCASE("Rapid Choice Transitions") {
        constexpr auto model = define(
            "RapidChoices",
            initial(target("/RapidChoices/start")),
            state("start",
                transition(on("GO"), target("/RapidChoices/choice"))
            ),
            choice("choice",
                transition(guard(guard_value_positive), target("/RapidChoices/positive")),
                transition(target("/RapidChoices/negative"))
            ),
            state("positive",
                entry(entry_positive),
                transition(on("NEXT"), target("/RapidChoices/choice"))
            ),
            state("negative",
                entry(entry_negative),
                transition(on("NEXT"), target("/RapidChoices/choice"))
            )
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;
        instance.value = 5;
        sm.start(instance);
        
        sm.dispatch(instance, "GO");
        CHECK(sm.state() == "/RapidChoices/positive");
        
        instance.value = -2;
        sm.dispatch(instance, "NEXT");
        CHECK(sm.state() == "/RapidChoices/negative");
        
        instance.value = 3;
        sm.dispatch(instance, "NEXT");
        CHECK(sm.state() == "/RapidChoices/positive");
    }

    SUBCASE("Choice with Complex Guard Expressions") {
        constexpr auto model = define(
            "ComplexGuards",
            initial(target("/ComplexGuards/start")),
            state("start",
                transition(on("EVALUATE"), target("/ComplexGuards/choice"))
            ),
            choice("choice",
                transition(guard(guard_positive_and_a), target("/ComplexGuards/positive_and_a")),
                transition(guard(guard_positive_and_b), target("/ComplexGuards/positive_and_b")),
                transition(guard(guard_value_positive), target("/ComplexGuards/just_positive")),
                transition(target("/ComplexGuards/other"))
            ),
            state("positive_and_a", entry(entry_positive_and_a)),
            state("positive_and_b", entry(entry_positive_and_b)),
            state("just_positive", entry(entry_just_positive)),
            state("other", entry(entry_other))
        );

        compile<model, ChoiceInstance> sm;
        ChoiceInstance instance;

        SUBCASE("Positive and A") {
            instance.value = 5;
            instance.condition_a = true;
            sm.start(instance);
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/ComplexGuards/positive_and_a");
        }

        SUBCASE("Just Positive") {
            instance.value = 3;
            // Neither condition_a nor condition_b is set
            sm.start(instance);
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/ComplexGuards/just_positive");
        }

        SUBCASE("Fallback") {
            instance.value = -1;  // Negative
            sm.start(instance);
            
            sm.dispatch(instance, "EVALUATE");
            CHECK(sm.state() == "/ComplexGuards/other");
        }
    }
}
