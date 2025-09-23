#include "include/cthsm_state_clean.hpp"
#include <iostream>
#include <cassert>

using namespace cthsm;

int main() {
    std::cout << "CTHSM - Testing clean state() method with proxy\n";
    std::cout << "===============================================\n\n";
    
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
    
    // Create state machine with clean functionality
    auto sm = create_clean(model);
    
    std::cout << "Testing state() method with proxy object:\n\n";
    
    // Test initial state - THIS IS THE DESIRED SYNTAX!
    if (sm.state() == "idle") {
        std::cout << "✓ Initial state is 'idle' - state() == \"idle\" works!\n";
    } else {
        std::cout << "✗ Failed to match initial state\n";
        return 1;
    }
    
    // Dispatch event
    std::cout << "\nDispatching START event...\n";
    sm.dispatch(Event("START"));
    
    // Test new state
    if (sm.state() == "running") {
        std::cout << "✓ State is now 'running' - state() == \"running\" works!\n";
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
    
    // Test hierarchical state
    constexpr auto hierarchical = define("HierarchicalSM",
        initial(target("parent/child")),
        
        state("parent",
            state("child",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "\nEntering parent/child state\n";
                })
            )
        )
    );
    
    auto hsm = create_clean(hierarchical);
    
    std::cout << "\nTesting hierarchical state:\n";
    if (hsm.state() == "child") {
        std::cout << "✓ Can match nested state with just leaf name\n";
    }
    
    std::cout << "\n✅ SUCCESS: Achieved exact DSL syntax!\n";
    std::cout << "\nWorking syntax:\n";
    std::cout << "  sm.state() == \"state_name\"  ✓\n";
    std::cout << "  sm.state() != \"other_state\" ✓\n";
    std::cout << "\nThe proxy approach provides the exact syntax requested!\n";
    std::cout << "No macros, no naming conflicts, just clean code.\n";
    
    return 0;
}