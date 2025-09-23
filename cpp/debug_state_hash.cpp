#include "include/cthsm.hpp"
#include <iostream>

using namespace cthsm;

int main() {
    std::cout << "Debugging state hash values:\n\n";
    
    // Test individual hashes
    std::cout << "hash(\"red\") = " << hash("red") << "\n";
    std::cout << "hash(\"green\") = " << hash("green") << "\n";
    std::cout << "hash(\"yellow\") = " << hash("yellow") << "\n";
    std::cout << "hash(\"TrafficLight\") = " << hash("TrafficLight") << "\n";
    
    // Test combined hashes
    uint32_t model_hash = hash("TrafficLight");
    std::cout << "\nCombined hashes:\n";
    std::cout << "combine_hashes(TrafficLight, red) = " << combine_hashes(model_hash, hash("red")) << "\n";
    std::cout << "combine_hashes(TrafficLight, green) = " << combine_hashes(model_hash, hash("green")) << "\n";
    std::cout << "combine_hashes(TrafficLight, yellow) = " << combine_hashes(model_hash, hash("yellow")) << "\n";
    
    // Create a simple state machine
    constexpr auto model = define("TrafficLight",
        initial(target("red")),
        state("red", transition(on("NEXT"), target("green"))),
        state("green", transition(on("NEXT"), target("yellow"))),
        state("yellow", transition(on("NEXT"), target("red")))
    );
    
    auto sm = StateMachine(model);
    
    std::cout << "\nActual state string: " << sm.state() << "\n";
    
    // Parse the hash from state string
    std::string_view state_str = sm.state();
    uint32_t parsed_hash = 0;
    for (char c : state_str) {
        if (c >= '0' && c <= '9') {
            parsed_hash = parsed_hash * 10 + (c - '0');
        }
    }
    std::cout << "Parsed hash from state string: " << parsed_hash << "\n";
    
    // Check which hash it matches
    if (parsed_hash == combine_hashes(model_hash, hash("red"))) {
        std::cout << "✓ Matches combined hash (TrafficLight/red)\n";
    } else if (parsed_hash == hash("red")) {
        std::cout << "✓ Matches direct hash (red)\n";
    } else {
        std::cout << "✗ Doesn't match expected hashes\n";
    }
    
    return 0;
}