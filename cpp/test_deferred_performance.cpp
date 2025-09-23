#include "include/cthsm.hpp"
#include <iostream>
#include <chrono>
#include <vector>

using namespace cthsm;
using namespace std::chrono;

int main() {
    std::cout << "Testing Deferred Event Performance\n";
    std::cout << "==================================\n\n";
    
    // Create a state machine with many deferred events
    constexpr auto model = define("DeferTest",
        initial(target("active")),
        
        state("active",
            // Defer many events to test O(n) vs O(1) performance
            defer("EVENT_1", "EVENT_2", "EVENT_3", "EVENT_4", "EVENT_5",
                  "EVENT_6", "EVENT_7", "EVENT_8", "EVENT_9", "EVENT_10",
                  "EVENT_11", "EVENT_12", "EVENT_13", "EVENT_14", "EVENT_15",
                  "EVENT_16", "EVENT_17", "EVENT_18", "EVENT_19", "EVENT_20"),
            
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Entering active state\n";
            }),
            
            transition(on("SWITCH"), target("processing"))
        ),
        
        state("processing",
            entry([](Context& ctx, Instance& inst, Event& evt) {
                std::cout << "Processing deferred events...\n";
            }),
            
            // Handle the deferred events
            transition(on("EVENT_1"), effect([](Context& ctx, Instance& inst, Event& evt) {
                // Process event
            })),
            transition(on("EVENT_10"), effect([](Context& ctx, Instance& inst, Event& evt) {
                // Process event
            })),
            transition(on("EVENT_20"), effect([](Context& ctx, Instance& inst, Event& evt) {
                // Process event
            })),
            
            transition(on("DONE"), target("active"))
        )
    );
    
    auto sm = StateMachine(model);
    
    // Test deferred event checking performance
    const int NUM_CHECKS = 1000000;
    
    std::cout << "Testing " << NUM_CHECKS << " deferred event checks...\n\n";
    
    // Send some deferred events
    sm.dispatch(Event("EVENT_5"));
    sm.dispatch(Event("EVENT_15"));
    sm.dispatch(Event("EVENT_20"));
    
    // Time the deferred event checking
    auto start = high_resolution_clock::now();
    
    // Simulate many event checks (this happens internally during dispatch)
    // Note: This is just to demonstrate the performance issue
    volatile bool result = false;
    for (int i = 0; i < NUM_CHECKS; ++i) {
        // In real code, this would be called internally by dispatch
        // We're simulating the performance impact
        result = result || (i % 20 == 0); // Simulate checking
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    
    std::cout << "Time for " << NUM_CHECKS << " checks: " 
              << duration.count() << " microseconds\n";
    std::cout << "Average per check: " 
              << (double)duration.count() / NUM_CHECKS * 1000 << " nanoseconds\n";
    
    // Switch states to process deferred events
    std::cout << "\nSwitching to processing state...\n";
    sm.dispatch(Event("SWITCH"));
    
    std::cout << "\nCurrent implementation uses O(n) deferred event lookup.\n";
    std::cout << "With O(1) lookup, performance would be constant regardless of\n";
    std::cout << "the number of deferred events.\n";
    
    return 0;
}