#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#include "hsm.hpp"

// Simple test instance for debugging
class SimpleTimerInstance : public hsm::Instance {
public:
    std::vector<std::string> log;
    std::atomic<bool> transition_fired{false};
    
    void add_log(const std::string& message) {
        log.push_back(message);
        std::cout << "LOG: " << message << std::endl;
    }
};

// Simple duration function
std::chrono::milliseconds simple_duration(hsm::Context& /*ctx*/, SimpleTimerInstance& /*inst*/, hsm::Event& /*event*/) {
    return std::chrono::milliseconds(25);  // Very short for quick testing
}

TEST_CASE("Simple Timer Debug") {
    std::cout << "\n=== Starting Simple Timer Debug Test ===" << std::endl;
    
    auto model = hsm::define(
        "SimpleTimer",
        hsm::initial(hsm::target("start")),
        hsm::state("start",
            hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                auto& test_inst = static_cast<SimpleTimerInstance&>(inst);
                test_inst.add_log("entered_start");
            }),
            hsm::transition(hsm::after<std::chrono::milliseconds, SimpleTimerInstance>(simple_duration),
                           hsm::target("../finish"),
                           hsm::effect([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                               auto& test_inst = static_cast<SimpleTimerInstance&>(inst);
                               test_inst.add_log("timer_effect");
                               test_inst.transition_fired = true;
                           }))
        ),
        hsm::state("finish",
            hsm::entry([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                auto& test_inst = static_cast<SimpleTimerInstance&>(inst);
                test_inst.add_log("entered_finish");
            })
        )
    );

    SimpleTimerInstance instance;
    
    std::cout << "Model members:" << std::endl;
    for (const auto& [name, member] : model->members) {
        std::cout << "  " << name << std::endl;
    }
    
    std::cout << "Starting HSM..." << std::endl;
    hsm::start(instance, model);
    
    std::cout << "Initial state: " << instance.state() << std::endl;
    CHECK(instance.state() == "/SimpleTimer/start");
    
    std::cout << "Initial log size: " << instance.log.size() << std::endl;
    for (size_t i = 0; i < instance.log.size(); ++i) {
        std::cout << "  [" << i << "]: " << instance.log[i] << std::endl;
    }
    
    std::cout << "Waiting for timer..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "After wait - state: " << instance.state() << std::endl;
    std::cout << "Timer fired: " << instance.transition_fired.load() << std::endl;
    std::cout << "Final log size: " << instance.log.size() << std::endl;
    for (size_t i = 0; i < instance.log.size(); ++i) {
        std::cout << "  [" << i << "]: " << instance.log[i] << std::endl;
    }
    
    // Check if timer worked
    CHECK(instance.transition_fired.load());
    CHECK(instance.state() == "/SimpleTimer/finish");
}