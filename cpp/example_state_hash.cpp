#include "include/cthsm_hash.hpp"
#include <iostream>

using namespace cthsm;

int main() {
    std::cout << "CTHSM - Clean State Comparison Example\n";
    std::cout << "======================================\n\n";
    
    // Define a traffic light model
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
    
    // Create state machine with extended functionality
    auto traffic_light = create_ext(model);
    
    // Clean state comparison syntax!
    std::cout << "Traffic light control:\n\n";
    
    // Check initial state
    if (traffic_light.state_hash() == "red") {
        std::cout << "Traffic light is RED - Cars must stop\n";
    }
    
    // Simulate timer events
    for (int i = 0; i < 6; ++i) {
        std::cout << "\nTimer expired...\n";
        traffic_light.dispatch(Event("TIMER"));
        
        // Clean comparison with string literals
        if (traffic_light.state_hash() == "red") {
            std::cout << "Status: Stopped at red light\n";
        } else if (traffic_light.state_hash() == "green") {
            std::cout << "Status: Moving through green light\n";
        } else if (traffic_light.state_hash() == "yellow") {
            std::cout << "Status: Slowing for yellow light\n";
        }
    }
    
    std::cout << "\n✅ Clean state comparison syntax achieved!\n";
    std::cout << "\nKey benefits:\n";
    std::cout << "• No need to define state constants\n";
    std::cout << "• Direct comparison with string literals\n";
    std::cout << "• Readable and maintainable code\n";
    std::cout << "• Works with existing CTHSM implementation\n";
    
    return 0;
}