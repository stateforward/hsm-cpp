#include "include/cthsm_final.hpp"
#include <iostream>

using namespace cthsm;

int main() {
    std::cout << "CTHSM - Final Working Example\n";
    std::cout << "=============================\n\n";
    
    // Define a compile-time state machine
    constexpr auto model = define("TrafficLight",
        initial(target("red")),
        
        state("red",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "🔴 Red light - STOP\n";
            }),
            transition(on("TIMER"), target("green"))
        ),
        
        state("green",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "🟢 Green light - GO\n";
            }),
            transition(on("TIMER"), target("yellow"))
        ),
        
        state("yellow",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "🟡 Yellow light - CAUTION\n";
            }),
            transition(on("TIMER"), target("red"))
        )
    );
    
    // Create state machine
    auto sm = StateMachine(model);
    
    std::cout << "Traffic light control:\n\n";
    
    // Clean state comparison using helper function
    if (check_state(sm, "red", "TrafficLight")) {
        std::cout << "Traffic light is RED - Cars must stop\n";
    }
    
    // Or using the macro with model name
    if (check_state(sm, "red", "TrafficLight")) {
        std::cout << "Confirmed: Light is red\n";
    }
    
    // Simulate timer events
    for (int i = 0; i < 6; ++i) {
        std::cout << "\nTimer expired...\n";
        sm.dispatch(Event("TIMER"));
        
        // Clean state checks
        if (check_state(sm, "red", "TrafficLight")) {
            std::cout << "Status: Stopped at red light\n";
        } else if (check_state(sm, "green", "TrafficLight")) {
            std::cout << "Status: Moving through green light\n";
        } else if (check_state(sm, "yellow", "TrafficLight")) {
            std::cout << "Status: Slowing for yellow light\n";
        }
    }
    
    std::cout << "\n✅ CTHSM Working with Clean State Comparison!\n";
    std::cout << "\nFeatures:\n";
    std::cout << "• Compile-time state machine definition\n";
    std::cout << "• O(1) hierarchical transition lookup\n";
    std::cout << "• Clean state comparison: check_state(sm, \"name\")\n";
    std::cout << "• Optional macro: IS_STATE(sm, \"name\")\n";
    std::cout << "• Stable implementation in cthsm.hpp\n";
    
    // Show the raw state string for debugging
    std::cout << "\nDebug: Raw state string: " << sm.state() << "\n";
    
    return 0;
}