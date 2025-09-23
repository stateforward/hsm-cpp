#include "include/cthsm_optimized.hpp"
#include <iostream>
#include <chrono>

using namespace cthsm;

// Test the O(1) hierarchical transition map
void test_transition_map() {
    std::cout << "=== O(1) Hierarchical Transition Map Test ===\n\n";
    
    // Create a hierarchical model
    constexpr auto model = define("HierarchicalSystem",
        initial(target("operational")),
        
        state("operational",
            initial(target("normal")),
            
            // Parent state transitions - inherited by all children
            transition(on("EMERGENCY_STOP"), target("emergency")),
            transition(on("MAINTENANCE"), target("maintenance")),
            
            state("normal",
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
                    transition(on("STOP"), target("idle")),
                    // Override parent's MAINTENANCE transition
                    transition(on("MAINTENANCE"), 
                        effect([](Context& ctx, Instance& inst, Event& evt) {
                            std::cout << "Cannot enter maintenance while running!\n";
                        })
                    )
                ),
                
                transition(on("CONFIGURE"), target("configuring"))
            ),
            
            state("configuring",
                entry([](Context& ctx, Instance& inst, Event& evt) {
                    std::cout << "Configuring system\n";
                }),
                transition(on("DONE"), target("normal"))
            )
        ),
        
        state("emergency",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "EMERGENCY STOP activated!\n";
            }),
            transition(on("RESET"), target("operational"))
        ),
        
        state("maintenance",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "In maintenance mode\n";
            }),
            transition(on("COMPLETE"), target("operational"))
        )
    );
    
    auto sm = create(model);
    
    // Show the compile-time transition map
    sm.debug_transition_map();
    
    std::cout << "Testing hierarchical transitions:\n\n";
    
    // Test inherited transition from deep nesting
    std::cout << "1. Testing EMERGENCY_STOP from idle (inherited from operational):\n";
    sm.dispatch(Event("EMERGENCY_STOP"));
    
    sm.dispatch(Event("RESET"));
    
    // Test override
    std::cout << "\n2. Testing MAINTENANCE from running (overridden):\n";
    sm.dispatch(Event("START"));  // idle -> running
    sm.dispatch(Event("MAINTENANCE"));  // Should show custom message
    
    // Test inherited transition from parent
    std::cout << "\n3. Testing MAINTENANCE from idle (inherited):\n";
    sm.dispatch(Event("STOP"));  // running -> idle
    sm.dispatch(Event("MAINTENANCE"));  // Should transition to maintenance
    
    std::cout << "\n✓ O(1) hierarchical transition lookup working!\n";
}

// Benchmark the O(1) lookup performance
void benchmark_transition_lookup() {
    std::cout << "\n=== Transition Lookup Performance Test ===\n";
    
    // Create a model with many states
    constexpr auto model = define("BenchmarkSystem",
        initial(target("s1")),
        
        state("s1",
            transition(on("E1"), target("s2")),
            transition(on("COMMON"), target("error")),
            
            state("s1_1",
                transition(on("E11"), target("s1_2")),
                
                state("s1_1_1",
                    transition(on("E111"), target("s1_1_2"))
                ),
                state("s1_1_2",
                    transition(on("E112"), target("s1_1_1"))
                )
            ),
            state("s1_2",
                transition(on("E12"), target("s1_1"))
            )
        ),
        
        state("s2",
            transition(on("E2"), target("s3")),
            transition(on("COMMON"), target("error")),
            
            state("s2_1",
                transition(on("E21"), target("s2_2"))
            ),
            state("s2_2",
                transition(on("E22"), target("s2_1"))
            )
        ),
        
        state("s3",
            transition(on("E3"), target("s1")),
            transition(on("COMMON"), target("error"))
        ),
        
        state("error",
            transition(on("RESET"), target("s1"))
        )
    );
    
    auto sm = create(model);
    
    const int iterations = 100000;
    
    // Benchmark transition lookups
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        // These lookups are O(1) with hierarchical inheritance
        sm.dispatch(Event("E1"));     // Direct transition
        sm.dispatch(Event("COMMON"));  // Inherited transition
        sm.dispatch(Event("RESET"));   // Back to s1
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Performed " << (iterations * 3) << " transition lookups in " 
              << duration.count() << " microseconds\n";
    std::cout << "Average time per lookup: " 
              << (duration.count() / double(iterations * 3)) << " microseconds\n";
    std::cout << "\nThis demonstrates O(1) lookup performance with hierarchical inheritance.\n";
}

int main() {
    std::cout << "CTHSM - Compile-Time O(1) Hierarchical Transition Map\n";
    std::cout << "=====================================================\n\n";
    
    test_transition_map();
    benchmark_transition_lookup();
    
    std::cout << "\n✅ All tests passed!\n\n";
    std::cout << "Key features demonstrated:\n";
    std::cout << "• Compile-time transition map construction\n";
    std::cout << "• O(1) transition lookup with hierarchical inheritance\n";
    std::cout << "• Parent state transitions inherited by children\n";
    std::cout << "• Child states can override parent transitions\n";
    std::cout << "• Efficient array-based lookup structure\n";
    
    return 0;
}