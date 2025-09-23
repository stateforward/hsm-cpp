#include "include/cthsm.hpp"
#include <iostream>
#include <cassert>

using namespace cthsm;

// Test that define returns a compile-time model
void test_compile_time_model() {
    // Define a simple light switch model at compile time
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
    
    // The model is a compile-time constant expression
    // Its type encodes the entire state machine structure
    
    // Create runtime state machine from compile-time model
    auto sm = create(model);
    
    // Test transitions
    sm.dispatch(Event("TOGGLE"));
    sm.dispatch(Event("TOGGLE"));
    
    std::cout << "Compile-time model test passed!\n";
}

// Test hierarchical state machine
void test_hierarchical() {
    // Define hierarchical model at compile time
    constexpr auto model = define("TrafficLight",
        initial(target("operational")),
        state("operational",
            initial(target("red")),
            state("red",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "Red light\n";
                }),
                transition(on("NEXT"), target("green"))
            ),
            state("green",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "Green light\n";
                }),
                transition(on("NEXT"), target("yellow"))
            ),
            state("yellow",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "Yellow light\n";
                }),
                transition(on("NEXT"), target("red"))
            ),
            transition(on("EMERGENCY"), target("flashing"))
        ),
        state("flashing",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Emergency flashing\n";
            }),
            transition(on("RESUME"), target("operational"))
        )
    );
    
    auto sm = create(model);
    
    // Test normal operation
    sm.dispatch(Event("NEXT"));  // red -> green
    sm.dispatch(Event("NEXT"));  // green -> yellow
    sm.dispatch(Event("NEXT"));  // yellow -> red
    
    // Test hierarchical transition
    sm.dispatch(Event("EMERGENCY"));  // any operational state -> flashing
    sm.dispatch(Event("RESUME"));     // flashing -> operational (red)
    
    std::cout << "Hierarchical test passed!\n";
}

// Test choice states
void test_choice() {
    struct Counter : Instance {
        int count = 0;
    };
    
    constexpr auto model = define("Counter",
        initial(target("active")),
        state("active",
            transition(on("INCREMENT"), 
                effect([](Context& ctx, Counter& inst, Event& evt) {
                    inst.count++;
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
            transition(target("active"))  // Guardless fallback
        ),
        final("done")
    );
    
    auto sm = create<decltype(model), Counter>(model);
    
    // Increment counter
    sm.dispatch(Event("INCREMENT"));  // count = 1, back to active
    sm.dispatch(Event("INCREMENT"));  // count = 2, back to active
    sm.dispatch(Event("INCREMENT"));  // count = 3, goes to done
    
    assert(sm.is_completed());
    std::cout << "Choice state test passed!\n";
}

int main() {
    test_compile_time_model();
    test_hierarchical();
    test_choice();
    
    std::cout << "\nAll tests passed! CTHSM with compile-time model definition is working.\n";
    return 0;
}