#include <iostream>
#include "hsm.hpp"

// Example demonstrating the variadic entry syntax
int main() {
    // Define a simple state machine with multiple entry actions
    auto model = hsm::define(
        "VariadicExample",
        hsm::initial(hsm::target("active")),
        hsm::state("active",
            // Single entry() call with multiple actions - clean and concise!
            hsm::entry(
                [](hsm::Context&, hsm::Instance&, hsm::Event&) {
                    std::cout << "First entry action" << std::endl;
                },
                [](hsm::Context&, hsm::Instance&, hsm::Event&) {
                    std::cout << "Second entry action" << std::endl;
                },
                [](hsm::Context&, hsm::Instance&, hsm::Event&) {
                    std::cout << "Third entry action" << std::endl;
                }
            )
        )
    );
    
    // Create and start the state machine
    hsm::Instance instance;
    hsm::start(instance, model);
    
    std::cout << "State machine started in state: " << instance.state() << std::endl;
    
    hsm::stop(instance).wait();
    
    return 0;
}