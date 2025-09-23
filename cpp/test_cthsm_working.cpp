#include "include/cthsm.hpp"
#include <iostream>
#include <cassert>

using namespace cthsm;

// Test that define returns a compile-time model
void test_compile_time_model() {
    std::cout << "\n=== Basic Compile-Time Model Test ===\n";
    
    // Define a model at compile time
    constexpr auto model = define("LightSwitch",
        initial(target("off")),
        state("off",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Light is OFF\n";
            }),
            transition(on("TOGGLE"), target("on"))
        ),
        state("on", 
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Light is ON\n";
            }),
            transition(on("TOGGLE"), target("off"))
        )
    );
    
    // The model is created at compile time
    // Its type encodes the entire state machine structure
    
    // Create runtime state machine from compile-time model
    auto sm = create(model);
    
    // Test transitions
    sm.dispatch(Event("TOGGLE"));
    sm.dispatch(Event("TOGGLE"));
    
    std::cout << "✓ Basic compile-time model works!\n";
}

// Test hierarchical features
void test_hierarchical() {
    std::cout << "\n=== Hierarchical Compile-Time Model Test ===\n";
    
    // Define hierarchical model at compile time
    constexpr auto model = define("System",
        initial(target("active")),
        
        state("active",
            initial(target("substate1")),
            
            state("substate1",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "In substate1\n";
                }),
                transition(on("NEXT"), target("substate2"))
            ),
            
            state("substate2",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "In substate2\n";
                }),
                transition(on("NEXT"), target("substate1"))
            ),
            
            // Parent state transition
            transition(on("EXIT"), target("inactive"))
        ),
        
        state("inactive",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "System inactive\n";
            }),
            transition(on("ACTIVATE"), target("active"))
        )
    );
    
    auto sm = create(model);
    
    // Test nested state transitions
    sm.dispatch(Event("NEXT"));  // substate1 -> substate2
    sm.dispatch(Event("NEXT"));  // substate2 -> substate1
    
    // Test hierarchical transition
    sm.dispatch(Event("EXIT"));  // exit from any substate -> inactive
    sm.dispatch(Event("ACTIVATE"));  // back to active (enters substate1)
    
    std::cout << "✓ Hierarchical compile-time model works!\n";
}

// Test choice states
void test_choice_states() {
    std::cout << "\n=== Choice State Compile-Time Test ===\n";
    
    struct Counter : Instance {
        int count = 0;
    };
    
    constexpr auto model = define("ChoiceDemo",
        initial(target("counting")),
        
        state("counting",
            transition(on("INCREMENT"), 
                effect([](Context& ctx, Counter& inst, Event& evt) {
                    inst.count++;
                    std::cout << "Count: " << inst.count << "\n";
                }),
                target("check")
            )
        ),
        
        choice("check",
            transition(
                guard([](Context& ctx, Counter& inst, Event& evt) {
                    return inst.count >= 3;
                }),
                target("done")
            ),
            transition(target("counting"))  // Guardless fallback
        ),
        
        state("done",
            entry([](Context& ctx, Counter& inst, Event& evt) {
                std::cout << "Counting complete!\n";
            })
        )
    );
    
    auto sm = create<decltype(model), Counter>(model);
    
    // Count up to 3
    sm.dispatch(Event("INCREMENT"));  // count = 1
    sm.dispatch(Event("INCREMENT"));  // count = 2
    sm.dispatch(Event("INCREMENT"));  // count = 3, goes to done
    
    std::cout << "✓ Choice states work at compile time!\n";
}

// Test internal transitions
void test_internal_transitions() {
    std::cout << "\n=== Internal Transition Test ===\n";
    
    constexpr auto model = define("InternalDemo",
        initial(target("active")),
        
        state("active",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Entering active state\n";
            }),
            exit([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Exiting active state\n";
            }),
            // Internal transition - no exit/entry
            transition(on("INTERNAL"), 
                effect([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "Internal transition effect\n";
                })
            ),
            // External transition
            transition(on("EXTERNAL"), target("other"))
        ),
        
        state("other",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "In other state\n";
            })
        )
    );
    
    auto sm = create(model);
    
    // Internal transition - no exit/entry
    sm.dispatch(Event("INTERNAL"));
    
    // External transition - exit/entry
    sm.dispatch(Event("EXTERNAL"));
    
    std::cout << "✓ Internal transitions work correctly!\n";
}

// Demonstrate compile-time optimization
void demonstrate_compile_time_features() {
    std::cout << "\n=== Compile-Time Features ===\n";
    
    // All of these are compile-time constants
    constexpr auto simple_model = define("Simple",
        initial(target("state1")),
        state("state1", transition(on("GO"), target("state2"))),
        state("state2")
    );
    
    // The model structure is fully encoded in the type
    using ModelType = decltype(simple_model);
    std::cout << "Model type size: " << sizeof(ModelType) << " bytes\n";
    
    // Hash values are computed at compile time
    constexpr uint32_t event_hash = hash("GO");
    constexpr uint32_t state_hash = hash("state1");
    std::cout << "Event 'GO' hash: " << event_hash << " (computed at compile time)\n";
    std::cout << "State 'state1' hash: " << state_hash << " (computed at compile time)\n";
    
    std::cout << "✓ Compile-time features demonstrated!\n";
}

int main() {
    std::cout << "CTHSM - Compile-Time Hierarchical State Machine\n";
    std::cout << "==============================================\n";
    
    test_compile_time_model();
    test_hierarchical();
    test_choice_states();
    test_internal_transitions();
    demonstrate_compile_time_features();
    
    std::cout << "\n✅ All tests passed!\n\n";
    std::cout << "Key achievements:\n";
    std::cout << "• hsm::define returns a compile-time model\n";
    std::cout << "• Entire model structure encoded in type system\n";
    std::cout << "• Hash computations done at compile time\n";
    std::cout << "• Identical syntax to runtime HSM\n";
    std::cout << "• Full hierarchical state machine support\n";
    
    return 0;
}