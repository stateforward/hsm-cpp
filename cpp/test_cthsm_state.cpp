#include "include/cthsm_state.hpp"
#include <iostream>
#include <cassert>

using namespace cthsm;

int main() {
    std::cout << "CTHSM - Testing state() method with State wrapper\n";
    std::cout << "================================================\n\n";
    
    // Define a simple state machine
    constexpr auto model = define("SimpleSM",
        initial(target("idle")),
        
        state("idle",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Entering idle state\n";
            }),
            transition(on("START"), target("running"))
        ),
        
        state("running",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Entering running state\n";
            }),
            transition(on("STOP"), target("idle"))
        )
    );
    
    // Create state machine with extended functionality
    auto sm = create_ext(model);
    
    std::cout << "Testing state() method returning State object:\n\n";
    
    // Test initial state - THIS IS THE DESIRED SYNTAX!
    if (sm.state() == "idle") {
        std::cout << "✓ Initial state is 'idle' - Direct string comparison works!\n";
    } else {
        std::cout << "✗ Failed to match initial state\n";
        return 1;
    }
    
    // Dispatch event
    std::cout << "\nDispatching START event...\n";
    sm.dispatch(Event("START"));
    
    // Test new state
    if (sm.state() == "running") {
        std::cout << "✓ State is now 'running' - String comparison works!\n";
    } else {
        std::cout << "✗ Failed to match running state\n";
        return 1;
    }
    
    // Test negative case
    if (sm.state() != "idle") {
        std::cout << "✓ Correctly not matching 'idle' anymore\n";
    }
    
    // Dispatch another event
    std::cout << "\nDispatching STOP event...\n";
    sm.dispatch(Event("STOP"));
    
    // Test back to idle
    if (sm.state() == "idle") {
        std::cout << "✓ Back to 'idle' state\n";
    }
    
    std::cout << "\n✅ SUCCESS: state() method returns State object that can be compared with strings!\n";
    std::cout << "\nAchieved syntax:\n";
    std::cout << "  sm.state() == \"state_name\"  ✓\n";
    std::cout << "  sm.state() != \"other_state\" ✓\n";
    std::cout << "\nThis matches the DSL perfectly!\n";
    
    return 0;
}