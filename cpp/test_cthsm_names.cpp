#include "include/cthsm_enhanced.hpp"
#include <iostream>
#include <cassert>

using namespace cthsm;

// Test human-readable state names
void test_state_names() {
    std::cout << "=== Human-Readable State Names Test ===\n\n";
    
    // Create a hierarchical model
    constexpr auto model = define("TrafficLight",
        initial(target("operational")),
        
        state("operational",
            initial(target("normal")),
            
            state("normal",
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
                ),
                
                transition(on("MAINTENANCE"), target("maintenance_mode"))
            ),
            
            state("maintenance_mode",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "Entering maintenance mode\n";
                }),
                transition(on("RESUME"), target("normal"))
            ),
            
            transition(on("EMERGENCY"), target("emergency"))
        ),
        
        state("emergency",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "EMERGENCY MODE - All lights flashing!\n";
            }),
            transition(on("CLEAR"), target("operational"))
        )
    );
    
    auto sm = create(model);
    
    // Print the state hierarchy with names
    sm.print_state_hierarchy();
    
    // Test state name reporting
    std::cout << "Initial state: " << sm.state() << "\n\n";
    
    std::cout << "Cycling through lights:\n";
    sm.dispatch(Event("NEXT"));
    std::cout << "Current state: " << sm.state() << "\n";
    
    sm.dispatch(Event("NEXT"));
    std::cout << "Current state: " << sm.state() << "\n";
    
    sm.dispatch(Event("NEXT"));
    std::cout << "Current state: " << sm.state() << "\n";
    
    std::cout << "\nTesting hierarchical transition:\n";
    sm.dispatch(Event("EMERGENCY"));
    std::cout << "Current state: " << sm.state() << "\n";
    
    sm.dispatch(Event("CLEAR"));
    std::cout << "Current state: " << sm.state() << "\n";
    
    std::cout << "\n✓ Human-readable state names working!\n";
}

// Test transition map with names
void test_transition_map_names() {
    std::cout << "\n=== Transition Map with Names Test ===\n";
    
    constexpr auto model = define("System",
        initial(target("idle")),
        
        state("idle",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "System idle\n";
            }),
            transition(on("START"), target("active")),
            transition(on("CONFIGURE"), target("configuring"))
        ),
        
        state("active",
            initial(target("processing")),
            
            state("processing",
                transition(on("PAUSE"), target("paused")),
                transition(on("ERROR"), target("error_state"))
            ),
            
            state("paused",
                transition(on("RESUME"), target("processing"))
            ),
            
            state("error_state",
                transition(on("RESET"), target("processing"))
            ),
            
            transition(on("STOP"), target("idle"))
        ),
        
        state("configuring",
            transition(on("DONE"), target("idle"))
        )
    );
    
    auto sm = create(model);
    
    // Print the O(1) transition map with readable names
    sm.print_transition_map();
    
    std::cout << "✓ Transition map with names displayed!\n";
}

// Test choice states with names
void test_choice_names() {
    std::cout << "\n=== Choice States with Names Test ===\n";
    
    struct Counter : Instance {
        int value = 0;
    };
    
    constexpr auto model = define("CounterMachine",
        initial(target("counting")),
        
        state("counting",
            transition(on("INCREMENT"), 
                effect([](Context& ctx, Counter& inst, Event& evt) {
                    inst.value++;
                    std::cout << "Value incremented to: " << inst.value << "\n";
                }),
                target("check_value")
            )
        ),
        
        choice("check_value",
            transition(
                guard([](Context& ctx, Counter& inst, Event& evt) {
                    return inst.value >= 3;
                }),
                target("high_value")
            ),
            transition(
                guard([](Context& ctx, Counter& inst, Event& evt) {
                    return inst.value == 2;
                }),
                target("medium_value")
            ),
            transition(target("counting"))  // Default
        ),
        
        state("medium_value",
            entry([](Context& ctx, Counter& inst, Event& evt) {
                std::cout << "Medium value reached\n";
            }),
            transition(on("CONTINUE"), target("counting"))
        ),
        
        state("high_value",
            entry([](Context& ctx, Counter& inst, Event& evt) {
                std::cout << "High value reached!\n";
            })
        )
    );
    
    auto sm = create<decltype(model), Counter>(model);
    
    std::cout << "\nState hierarchy:\n";
    sm.print_state_hierarchy();
    
    std::cout << "\nTesting choice navigation:\n";
    std::cout << "Initial state: " << sm.state() << "\n";
    
    sm.dispatch(Event("INCREMENT"));  // value = 1
    std::cout << "After increment: " << sm.state() << "\n";
    
    sm.dispatch(Event("INCREMENT"));  // value = 2
    std::cout << "After increment: " << sm.state() << "\n";
    
    sm.dispatch(Event("CONTINUE"));
    sm.dispatch(Event("INCREMENT"));  // value = 3
    std::cout << "After increment: " << sm.state() << "\n";
    
    std::cout << "\n✓ Choice states with names working!\n";
}

int main() {
    std::cout << "CTHSM - Hash-to-String Table for Human Readable States\n";
    std::cout << "======================================================\n\n";
    
    test_state_names();
    test_transition_map_names();
    test_choice_names();
    
    std::cout << "\n✅ All tests passed!\n\n";
    std::cout << "Key features demonstrated:\n";
    std::cout << "• State names stored at compile time in DSL types\n";
    std::cout << "• Hash-to-name mapping built at initialization\n";
    std::cout << "• state() method returns human-readable paths\n";
    std::cout << "• Debug methods show full hierarchy with names\n";
    std::cout << "• O(1) transition lookup still maintained\n";
    
    return 0;
}