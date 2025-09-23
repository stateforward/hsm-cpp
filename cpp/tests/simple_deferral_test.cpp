#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "hsm.hpp"

// Simple test instance
class SimpleDeferralInstance : public hsm::Instance {
public:
    std::vector<std::string> log;
    
    void add_log(const std::string& message) {
        log.push_back(message);
        std::cout << "LOG: " << message << std::endl;
    }
};

TEST_CASE("Simple Deferral Runtime Test") {
    std::cout << "\n=== Starting Simple Deferral Test ===\n" << std::endl;
    
    auto model = hsm::define(
        "SimpleDeferral",
        hsm::initial(hsm::target("busy")),
        hsm::state("busy",
            hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                auto& test_inst = static_cast<SimpleDeferralInstance&>(inst);
                test_inst.add_log("entered_busy");
            }),
            hsm::defer("REQUEST"),
            hsm::transition(hsm::on("READY"), hsm::target("../ready"))
        ),
        hsm::state("ready",
            hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                auto& test_inst = static_cast<SimpleDeferralInstance&>(inst);
                test_inst.add_log("entered_ready");
            }),
            hsm::transition(hsm::on("REQUEST"), 
                hsm::target("../done"),
                hsm::effect([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                    auto& test_inst = static_cast<SimpleDeferralInstance&>(inst);
                    test_inst.add_log("processed_REQUEST");
                }))
        ),
        hsm::state("done",
            hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                auto& test_inst = static_cast<SimpleDeferralInstance&>(inst);
                test_inst.add_log("entered_done");
            })
        )
    );

    SimpleDeferralInstance instance;
    
    std::cout << "Starting HSM..." << std::endl;
    hsm::start(instance, model);
    
    std::cout << "Initial state: " << instance.state() << std::endl;
    CHECK(instance.state() == "/SimpleDeferral/busy");
    
    std::cout << "\nSending REQUEST event (should be deferred)..." << std::endl;
    instance.dispatch(hsm::Event("REQUEST")).wait();
    
    std::cout << "State after REQUEST: " << instance.state() << std::endl;
    CHECK(instance.state() == "/SimpleDeferral/busy");
    
    // Check that REQUEST was not processed
    bool found_processed = false;
    for (const auto& entry : instance.log) {
        if (entry == "processed_REQUEST") {
            found_processed = true;
            break;
        }
    }
    CHECK_FALSE(found_processed);
    
    std::cout << "\nSending READY event..." << std::endl;
    instance.dispatch(hsm::Event("READY")).wait();
    
    std::cout << "State after READY: " << instance.state() << std::endl;
    
    // If deferral works, we should be in "done" state (REQUEST was processed)
    // If deferral doesn't work, we would be in "ready" state
    CHECK(instance.state() == "/SimpleDeferral/done");
    
    // Check that REQUEST was processed
    found_processed = false;
    for (const auto& entry : instance.log) {
        if (entry == "processed_REQUEST") {
            found_processed = true;
            break;
        }
    }
    CHECK(found_processed);
    
    std::cout << "\nFinal log:" << std::endl;
    for (size_t i = 0; i < instance.log.size(); ++i) {
        std::cout << "  [" << i << "]: " << instance.log[i] << std::endl;
    }
    
    // Verify execution order
    REQUIRE(instance.log.size() >= 4);
    CHECK(instance.log[0] == "entered_busy");
    CHECK(instance.log[1] == "entered_ready");
    CHECK(instance.log[2] == "processed_REQUEST");
    CHECK(instance.log[3] == "entered_done");
}