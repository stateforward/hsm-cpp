#include <variant>
#include <tuple>
#include <iostream>
#include <vector>
#include <cstring>

// Simplified state types for demonstration
template<typename... Children>
struct state_t {
    const char* name;
    std::tuple<Children...> children;
};

struct entry_t { 
    auto func() { std::cout << "entry\n"; }
};

struct transition_t {
    const char* event;
    const char* target;
};

// Example states with different types
using RedState = state_t<entry_t, transition_t>;
using GreenState = state_t<entry_t, transition_t, transition_t>;  // Different type!

// Could we use variant?
using StateVariant = std::variant<RedState, GreenState>;

int main() {
    // Yes, we can store different state types!
    std::vector<StateVariant> states;
    states.push_back(RedState{"red", {entry_t{}, transition_t{"TIMER", "green"}}});
    states.push_back(GreenState{"green", {entry_t{}, transition_t{"TIMER", "yellow"}, transition_t{"EMERGENCY", "red"}}});
    
    // But here's the problem:
    std::cout << "Problems with variant approach:\n\n";
    
    // Problem 1: How do we look up a state by name/hash at runtime?
    std::cout << "1. No O(1) lookup by state hash\n";
    const char* target_state = "green";  // Runtime value
    // We'd have to iterate through all states
    for (const auto& state : states) {
        std::visit([&](const auto& s) {
            if (strcmp(s.name, target_state) == 0) {
                std::cout << "   Found state (but this is O(n)!)\n";
            }
        }, state);
    }
    
    // Problem 2: The variant type must know ALL possible state types at compile time
    std::cout << "\n2. Variant must list every possible state type:\n";
    std::cout << "   variant<State1, State2, State3, State4, ...>\n";
    
    // Problem 3: Each model would need a different variant type
    std::cout << "\n3. Each state machine model needs different variant:\n";
    std::cout << "   TrafficLight: variant<RedState, GreenState, YellowState>\n";
    std::cout << "   DoorSM: variant<OpenState, ClosedState, LockedState>\n";
    std::cout << "   Can't have generic StateMachine class!\n";
    
    // Problem 4: How do we build hierarchical relationships?
    std::cout << "\n4. Can't easily build parent-child relationships\n";
    std::cout << "   Would need compile-time parent type in each child\n";
    
    // The core issue:
    std::cout << "\n5. The model type would leak into runtime:\n";
    std::cout << "   StateMachine<variant<State1, State2, ...>> sm;\n";
    std::cout << "   Different variant for each model!\n";
    
    std::cout << "\nConclusion: Variant would make each StateMachine type unique,\n";
    std::cout << "preventing generic interfaces and limiting flexibility.\n";
    
    return 0;
}