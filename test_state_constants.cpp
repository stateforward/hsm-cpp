#include "include/cthsm_states.hpp"
#include <iostream>
#include <cassert>

using namespace cthsm;

// Define the model
constexpr auto traffic_light_model = define("TrafficLight",
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
    )
);

// Define state constants for the model
CTHSM_STATES(traffic_light_model)
    CTHSM_STATE(red, "red")
    CTHSM_STATE(green, "green")
    CTHSM_STATE(yellow, "yellow")
CTHSM_END_STATES

// Test simple state comparison
void test_simple_state_comparison() {
    std::cout << "=== Simple State Comparison Test ===\n\n";
    
    auto sm = create_with_hash(traffic_light_model);
    
    // Check initial state
    std::cout << "Initial state check: ";
    if (sm.state_hash() == states::red) {
        std::cout << "✓ State is RED\n";
    }
    assert(sm.state_hash() == states::red);
    
    // Transition and check
    sm.dispatch(Event("NEXT"));
    std::cout << "After NEXT: ";
    if (sm.state_hash() == states::green) {
        std::cout << "✓ State is GREEN\n";
    }
    assert(sm.state_hash() == states::green);
    
    // Another transition
    sm.dispatch(Event("NEXT"));
    std::cout << "After NEXT: ";
    if (sm.state_hash() == states::yellow) {
        std::cout << "✓ State is YELLOW\n";
    }
    assert(sm.state_hash() == states::yellow);
    
    // Back to red
    sm.dispatch(Event("NEXT"));
    std::cout << "After NEXT: ";
    if (sm.state_hash() == states::red) {
        std::cout << "✓ State is RED again\n";
    }
    assert(sm.state_hash() == states::red);
    
    std::cout << "\n✓ Simple state comparison working!\n";
}

// Test hierarchical state comparison
void test_hierarchical_state_comparison() {
    std::cout << "\n=== Hierarchical State Comparison Test ===\n\n";
    
    constexpr auto system_model = define("System",
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
            })
        )
    );
    
    // Define hierarchical state constants
    CTHSM_STATES(system_model)
        CTHSM_STATE(operational, "operational")
        CTHSM_STATE(idle, "operational", "idle")
        CTHSM_STATE(running, "operational", "running")
        CTHSM_STATE(off, "off")
    CTHSM_END_STATES
    
    auto sm = create_with_hash(system_model);
    
    // Check nested states
    std::cout << "Initial state: ";
    if (sm.state_hash() == states::idle) {
        std::cout << "✓ In operational/idle\n";
    }
    assert(sm.state_hash() == states::idle);
    
    sm.dispatch(Event("START"));
    std::cout << "After START: ";
    if (sm.state_hash() == states::running) {
        std::cout << "✓ In operational/running\n";
    }
    assert(sm.state_hash() == states::running);
    
    sm.dispatch(Event("SHUTDOWN"));
    std::cout << "After SHUTDOWN: ";
    if (sm.state_hash() == states::off) {
        std::cout << "✓ In off state\n";
    }
    assert(sm.state_hash() == states::off);
    
    std::cout << "\n✓ Hierarchical state comparison working!\n";
}

// Test switch statement with state constants
void test_switch_statement() {
    std::cout << "\n=== Switch Statement Test ===\n\n";
    
    auto sm = create_with_hash(traffic_light_model);
    
    auto check_light = [&sm]() {
        switch (sm.state_hash()) {
            case states::red:
                std::cout << "STOP - Red light\n";
                break;
            case states::yellow:
                std::cout << "CAUTION - Yellow light\n";
                break;
            case states::green:
                std::cout << "GO - Green light\n";
                break;
            default:
                std::cout << "Unknown state!\n";
        }
    };
    
    // Test all states
    check_light();  // Red
    sm.dispatch(Event("NEXT"));
    check_light();  // Green
    sm.dispatch(Event("NEXT"));
    check_light();  // Yellow
    
    std::cout << "\n✓ Switch statement working with state constants!\n";
}

// Demonstrate compile-time nature
void demonstrate_compile_time() {
    std::cout << "\n=== Compile-Time State Constants ===\n\n";
    
    std::cout << "State hash values (computed at compile time):\n";
    std::cout << "  states::red    = " << states::red << "\n";
    std::cout << "  states::green  = " << states::green << "\n";
    std::cout << "  states::yellow = " << states::yellow << "\n";
    
    // These are all compile-time constants
    static_assert(states::red != states::green);
    static_assert(states::green != states::yellow);
    static_assert(states::yellow != states::red);
    
    std::cout << "\n✓ State constants are compile-time values!\n";
}

int main() {
    std::cout << "CTHSM - State Constants for Clean Comparison\n";
    std::cout << "===========================================\n\n";
    
    test_simple_state_comparison();
    test_hierarchical_state_comparison();
    test_switch_statement();
    demonstrate_compile_time();
    
    std::cout << "\n✅ All tests passed!\n\n";
    std::cout << "Key features:\n";
    std::cout << "• Clean syntax: sm.state_hash() == states::red\n";
    std::cout << "• Compile-time state constants\n";
    std::cout << "• Works with switch statements\n";
    std::cout << "• Type-safe state comparisons\n";
    std::cout << "• Hierarchical state support\n";
    
    return 0;
}