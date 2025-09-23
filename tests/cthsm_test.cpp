#include <iostream>
#include <cassert>
#include "../include/cthsm.hpp"

// Simple state machine instance for testing
struct LightInstance {
    int transition_count = 0;
    
    void on_turn_on() { 
        ++transition_count;
        std::cout << "Light turned ON (count: " << transition_count << ")\n"; 
    }
    
    void on_turn_off() { 
        ++transition_count;
        std::cout << "Light turned OFF (count: " << transition_count << ")\n"; 
    }
    
    bool can_turn_on() const { 
        return transition_count < 5; // Allow max 5 transitions
    }
};

// Define states and events
constexpr auto off_state = cthsm::make_state<"off">();
constexpr auto on_state = cthsm::make_state<"on">();
constexpr auto turn_on_event = cthsm::make_event<"turn_on">();
constexpr auto turn_off_event = cthsm::make_event<"turn_off">();

// Define effects
constexpr auto on_effect = [](LightInstance& instance) { instance.on_turn_on(); };
constexpr auto off_effect = [](LightInstance& instance) { instance.on_turn_off(); };

// Define guard
constexpr auto can_turn_on_guard = [](const LightInstance& instance) { return instance.can_turn_on(); };

// Define transitions
constexpr auto transition1 = cthsm::make_transition_with_guard_and_effect<
    decltype(off_state), decltype(on_state), decltype(turn_on_event), 
    can_turn_on_guard, on_effect
>(off_state, on_state, turn_on_event);

constexpr auto transition2 = cthsm::make_transition_with_effect<
    decltype(on_state), decltype(off_state), decltype(turn_off_event), 
    off_effect
>(on_state, off_state, turn_off_event);

// Define model
constexpr auto light_model = cthsm::make_model<decltype(off_state), decltype(off_state), decltype(on_state)>();

// Compile state machine
using LightMachine = cthsm::compile<decltype(light_model), decltype(transition1), decltype(transition2)>;

void test_basic_functionality() {
    std::cout << "Testing basic CTHSM functionality...\n";
    
    LightMachine machine;
    LightInstance instance;
    
    // Test initial state
    assert(machine.current_state() == 0); // off state index
    std::cout << "✓ Initial state is correct\n";
    
    // Test valid transition
    bool handled = machine.dispatch(turn_on_event, &instance);
    assert(handled);
    assert(machine.current_state() == 1); // on state index
    assert(instance.transition_count == 1);
    std::cout << "✓ Transition off -> on works\n";
    
    // Test return transition
    handled = machine.dispatch(turn_off_event, &instance);
    assert(handled);
    assert(machine.current_state() == 0); // back to off state
    assert(instance.transition_count == 2);
    std::cout << "✓ Transition on -> off works\n";
    
    // Test guard condition - should work for first few transitions
    for (int i = 0; i < 3; ++i) {
        handled = machine.dispatch(turn_on_event, &instance);
        assert(handled);
        handled = machine.dispatch(turn_off_event, &instance);
        assert(handled);
    }
    std::cout << "✓ Multiple transitions work with guard\n";
    
    // Test guard blocking transition (should fail after 5 total transitions)
    handled = machine.dispatch(turn_on_event, &instance);
    assert(!handled); // Guard should block this
    assert(machine.current_state() == 0); // Should remain in off state
    std::cout << "✓ Guard correctly blocks transition\n";
    
    // Test reset
    machine.reset();
    assert(machine.current_state() == 0);
    std::cout << "✓ Reset works\n";
    
    std::cout << "All tests passed! 🎉\n";
}

void test_performance() {
    std::cout << "\nTesting performance...\n";
    
    LightMachine machine;
    LightInstance instance;
    
    constexpr int iterations = 1000000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        machine.dispatch(turn_on_event, &instance);
        machine.dispatch(turn_off_event, &instance);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    double transitions_per_second = (2.0 * iterations) / (duration.count() / 1e9);
    double avg_transition_time = duration.count() / (2.0 * iterations);
    
    std::cout << "Performance results:\n";
    std::cout << "  Transitions per second: " << static_cast<long>(transitions_per_second) << "\n";
    std::cout << "  Average transition time: " << avg_transition_time << " ns\n";
    std::cout << "  Total transitions: " << (2 * iterations) << "\n";
}

int main() {
    test_basic_functionality();
    test_performance();
    return 0;
}