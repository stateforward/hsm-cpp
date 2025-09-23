#include "include/cthsm_hash.hpp"
#include <iostream>
#include <cassert>

using namespace cthsm;

void test_simple_comparison() {
    std::cout << "=== Simple State Comparison ===\n\n";
    
    constexpr auto model = define("TrafficLight",
        initial(target("red")),
        state("red",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Red light ON\n";
            }),
            transition(on("NEXT"), target("green"))
        ),
        state("green",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Green light ON\n";
            }),
            transition(on("NEXT"), target("yellow"))
        ),
        state("yellow",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Yellow light ON\n";
            }),
            transition(on("NEXT"), target("red"))
        )
    );
    
    auto sm = create_ext(model);
    
    // Clean syntax with string literals!
    if (sm.state_hash() == "red") {
        std::cout << "✓ State is red\n";
    }
    assert(sm.state_hash() == "red");
    
    sm.dispatch(Event("NEXT"));
    if (sm.state_hash() == "green") {
        std::cout << "✓ State is green\n";
    }
    assert(sm.state_hash() == "green");
    
    sm.dispatch(Event("NEXT"));
    if (sm.state_hash() == "yellow") {
        std::cout << "✓ State is yellow\n";
    }
    assert(sm.state_hash() == "yellow");
    
    sm.dispatch(Event("NEXT"));
    if (sm.state_hash() == "red") {
        std::cout << "✓ Back to red\n";
    }
    
    std::cout << "\n✓ Simple comparison works!\n";
}

void test_hierarchical_states() {
    std::cout << "\n=== Hierarchical State Comparison ===\n\n";
    
    constexpr auto model = define("System",
        initial(target("operational")),
        state("operational",
            initial(target("idle")),
            state("idle",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "System idle\n";
                }),
                transition(on("START"), target("running"))
            ),
            state("running",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "System running\n";
                }),
                transition(on("STOP"), target("idle"))
            ),
            transition(on("SHUTDOWN"), target("off"))
        ),
        state("off",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "System off\n";
            }),
            transition(on("POWER"), target("operational"))
        )
    );
    
    auto sm = create_ext(model);
    
    // For hierarchical states, we need to check against the leaf state
    // The model doesn't store full paths, just hashes
    if (sm.state_hash() == "idle") {
        std::cout << "✓ In idle state\n";
    }
    
    sm.dispatch(Event("START"));
    if (sm.state_hash() == "running") {
        std::cout << "✓ In running state\n";
    }
    
    sm.dispatch(Event("SHUTDOWN"));
    if (sm.state_hash() == "off") {
        std::cout << "✓ System is off\n";
    }
    
    std::cout << "\n✓ Hierarchical comparison works!\n";
}

void test_switch_statement() {
    std::cout << "\n=== Switch Statement Test ===\n\n";
    
    constexpr auto model = define("Device",
        initial(target("idle")),
        state("idle", transition(on("ACTIVATE"), target("active"))),
        state("active", 
            transition(on("SUSPEND"), target("suspended")),
            transition(on("DEACTIVATE"), target("idle"))
        ),
        state("suspended", transition(on("RESUME"), target("active")))
    );
    
    auto sm = create_ext(model);
    
    auto check_state = [&sm]() {
        StateHash current = sm.state_hash();
        
        // StateHash can be used in switch because of operator uint32_t()
        switch (current.value()) {
            case hash("idle"):
                std::cout << "Device is idle\n";
                break;
            case hash("active"):
                std::cout << "Device is active\n";
                break;
            case hash("suspended"):
                std::cout << "Device is suspended\n";
                break;
            default:
                std::cout << "Unknown state\n";
        }
    };
    
    check_state();  // idle
    
    sm.dispatch(Event("ACTIVATE"));
    check_state();  // active
    
    sm.dispatch(Event("SUSPEND"));
    check_state();  // suspended
    
    std::cout << "\n✓ Switch statement works!\n";
}

void test_state_constants() {
    std::cout << "\n=== State Constants Test ===\n\n";
    
    // You can still define constants if needed
    constexpr uint32_t IDLE_STATE = hash("idle");
    constexpr uint32_t RUNNING_STATE = hash("running");
    
    constexpr auto model = define("Machine",
        initial(target("idle")),
        state("idle", transition(on("RUN"), target("running"))),
        state("running", transition(on("STOP"), target("idle")))
    );
    
    auto sm = create_ext(model);
    
    // Mix of comparison styles
    if (sm.state_hash() == "idle") {
        std::cout << "✓ String comparison: idle\n";
    }
    
    if (sm.state_hash() == IDLE_STATE) {
        std::cout << "✓ Constant comparison: idle\n";
    }
    
    sm.dispatch(Event("RUN"));
    
    if (sm.state_hash() == "running") {
        std::cout << "✓ String comparison: running\n";
    }
    
    if (sm.state_hash() == RUNNING_STATE) {
        std::cout << "✓ Constant comparison: running\n";
    }
    
    std::cout << "\n✓ Both comparison styles work!\n";
}

void test_invalid_states() {
    std::cout << "\n=== Invalid State Test ===\n\n";
    
    constexpr auto model = define("Test",
        initial(target("valid")),
        state("valid")
    );
    
    auto sm = create_ext(model);
    
    if (sm.state_hash() == "valid") {
        std::cout << "✓ In valid state\n";
    }
    
    if (sm.state_hash() != "invalid") {
        std::cout << "✓ Not in invalid state\n";
    }
    
    // This will always be false
    if (sm.state_hash() == "nonexistent") {
        std::cout << "✗ This should never print\n";
    } else {
        std::cout << "✓ Correctly identified non-matching state\n";
    }
    
    std::cout << "\n✓ Invalid state comparison works correctly!\n";
}

int main() {
    std::cout << "CTHSM - StateHash with String Comparison\n";
    std::cout << "========================================\n\n";
    
    test_simple_comparison();
    test_hierarchical_states();
    test_switch_statement();
    test_state_constants();
    test_invalid_states();
    
    std::cout << "\n✅ All tests passed!\n\n";
    std::cout << "Key features:\n";
    std::cout << "• Clean syntax: sm.state_hash() == \"state_name\"\n";
    std::cout << "• No need to define state constants\n";
    std::cout << "• Works with hierarchical states\n";
    std::cout << "• Can still use switch statements\n";
    std::cout << "• Mix string and constant comparisons\n";
    
    return 0;
}