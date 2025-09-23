#include "include/cthsm_hash.hpp"
#include <iostream>

using namespace cthsm;

int main() {
    // Test what hash values we're getting
    constexpr auto model = define("TrafficLight",
        initial(target("red")),
        state("red", transition(on("NEXT"), target("green"))),
        state("green", transition(on("NEXT"), target("yellow"))),
        state("yellow", transition(on("NEXT"), target("red")))
    );
    
    auto sm = create_ext(model);
    
    std::cout << "Current state string: " << sm.state() << "\n";
    std::cout << "Current state hash: " << sm.state_hash().value() << "\n";
    std::cout << "Expected 'red' hash: " << hash("red") << "\n";
    
    // The issue is that the state hash includes the model name
    uint32_t model_hash = hash("TrafficLight");
    uint32_t red_full_hash = combine_hashes(model_hash, hash("red"));
    std::cout << "Expected full 'red' hash: " << red_full_hash << "\n";
    
    // Test if we need to include the full path
    if (sm.state_hash().value() == red_full_hash) {
        std::cout << "\n✓ State hash includes parent model hash\n";
    }
    
    return 0;
}