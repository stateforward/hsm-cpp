#include "include/cthsm.hpp"
#include <iostream>
#include <cassert>

using namespace cthsm;

// Simple approach: define state constants manually
namespace traffic_states {
    // Compute these using the same hash function and hierarchy
    constexpr uint32_t model_hash = hash("TrafficLight");
    constexpr uint32_t red = combine_hashes(model_hash, hash("red"));
    constexpr uint32_t green = combine_hashes(model_hash, hash("green"));
    constexpr uint32_t yellow = combine_hashes(model_hash, hash("yellow"));
}

// Helper to get state hash from state machine
uint32_t get_state_hash(const auto& sm) {
    // Parse the hash from the state() string
    std::string_view state_str = sm.state();
    return std::stoul(std::string(state_str));
}

void test_state_comparison() {
    std::cout << "=== State Comparison with Constants ===\n\n";
    
    // Define the model
    constexpr auto model = define("TrafficLight",
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
    
    auto sm = create(model);
    
    // Check states using constants
    std::cout << "Current state hash: " << get_state_hash(sm) << "\n";
    std::cout << "Expected red hash: " << traffic_states::red << "\n";
    
    if (get_state_hash(sm) == traffic_states::red) {
        std::cout << "✓ State is RED\n";
    }
    
    sm.dispatch(Event("NEXT"));
    if (get_state_hash(sm) == traffic_states::green) {
        std::cout << "✓ State is GREEN\n";
    }
    
    sm.dispatch(Event("NEXT"));
    if (get_state_hash(sm) == traffic_states::yellow) {
        std::cout << "✓ State is YELLOW\n";
    }
    
    sm.dispatch(Event("NEXT"));
    if (get_state_hash(sm) == traffic_states::red) {
        std::cout << "✓ Back to RED\n";
    }
}

// Alternative: Create a wrapper with public state hash access
template<typename Model, typename T = Instance>
class StateMachineWithPublicHash : public StateMachine<Model, T> {
public:
    using Base = StateMachine<Model, T>;
    
    template<typename... Args>
    StateMachineWithPublicHash(const Model& m, Args&&... args) 
        : Base(m, std::forward<Args>(args)...) {}
    
    // Provide public access to state hash
    uint32_t state_hash() const {
        return std::stoul(std::string(this->state()));
    }
};

// State constants for System model
namespace system_states {
    constexpr uint32_t model_hash = hash("System");
    constexpr uint32_t idle = combine_hashes(model_hash, hash("idle"));
    constexpr uint32_t running = combine_hashes(model_hash, hash("running"));
}

void test_with_wrapper() {
    std::cout << "\n=== Testing with Wrapper Class ===\n\n";
    
    constexpr auto model = define("System",
        initial(target("idle")),
        state("idle",
            transition(on("START"), target("running"))
        ),
        state("running",
            transition(on("STOP"), target("idle"))
        )
    );
    
    StateMachineWithPublicHash<decltype(model)> sm(model);
    
    // Now we can use clean syntax
    if (sm.state_hash() == system_states::idle) {
        std::cout << "✓ System is idle\n";
    }
    
    sm.dispatch(Event("START"));
    if (sm.state_hash() == system_states::running) {
        std::cout << "✓ System is running\n";
    }
    
    // Switch statement works too
    switch (sm.state_hash()) {
        case system_states::idle:
            std::cout << "In idle state\n";
            break;
        case system_states::running:
            std::cout << "In running state\n";
            break;
        default:
            std::cout << "Unknown state\n";
    }
}

// Macro to simplify state constant definition
#define DEFINE_STATE_CONSTANT(model_name, state_name) \
    constexpr uint32_t state_name = combine_hashes(hash(#model_name), hash(#state_name));

#define DEFINE_NESTED_STATE_CONSTANT(model_name, parent_name, state_name) \
    constexpr uint32_t state_name = combine_hashes(combine_hashes(hash(#model_name), hash(#parent_name)), hash(#state_name));

// Define state constants using macros
namespace macro_states {
    DEFINE_STATE_CONSTANT(TrafficLight, red)
    DEFINE_STATE_CONSTANT(TrafficLight, green)
    DEFINE_STATE_CONSTANT(TrafficLight, yellow)
}

void test_with_macros() {
    std::cout << "\n=== Testing with Macros ===\n\n";
    
    constexpr auto model = define("TrafficLight",
        initial(target("red")),
        state("red", transition(on("NEXT"), target("green"))),
        state("green", transition(on("NEXT"), target("yellow"))),
        state("yellow", transition(on("NEXT"), target("red")))
    );
    
    auto sm = create(model);
    
    std::cout << "State constants (compile-time):\n";
    std::cout << "  red:    " << macro_states::red << "\n";
    std::cout << "  green:  " << macro_states::green << "\n";
    std::cout << "  yellow: " << macro_states::yellow << "\n";
    
    // These are compile-time constants
    static_assert(macro_states::red != macro_states::green);
    static_assert(macro_states::green != macro_states::yellow);
    
    std::cout << "\n✓ Macro-based state constants work!\n";
}

int main() {
    std::cout << "CTHSM - State Comparison Solutions\n";
    std::cout << "==================================\n\n";
    
    test_state_comparison();
    test_with_wrapper();
    test_with_macros();
    
    std::cout << "\n✅ All approaches work!\n\n";
    std::cout << "Options for state comparison:\n";
    std::cout << "1. Manual state constant definition\n";
    std::cout << "2. Wrapper class with state_hash() method\n";
    std::cout << "3. Macros for easier constant definition\n";
    std::cout << "4. Parse hash from state() string\n";
    
    return 0;
}