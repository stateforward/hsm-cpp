#include <chrono>
#include <iostream>
#include <thread>

#include "hsm.hpp"

using namespace std::chrono_literals;

int main() {
  std::cout << "Testing time-based transitions (after and every)" << std::endl;

  // Create a simple state machine with time-based transitions
  auto model = hsm::define(
      "TimerHSM", hsm::state("idle"), hsm::state("active"),
      hsm::state("timeout"),

      // Transition from idle to active after 1 second
      hsm::transition(
          hsm::source("idle"), hsm::target("active"),
          hsm::after<std::chrono::seconds, hsm::Instance>([](hsm::Context& /*ctx*/, hsm::Instance& /*inst*/, hsm::Event& /*event*/) {
            std::cout
                << "After timer: transitioning from idle to active after 1s"
                << std::endl;
            return 1s;
          })),

      // Transition from active to timeout after 2 seconds
      hsm::transition(
          hsm::source("active"), hsm::target("timeout"),
          hsm::after<std::chrono::seconds, hsm::Instance>([](hsm::Context& /*ctx*/, hsm::Instance& /*inst*/, hsm::Event& /*event*/) {
            std::cout
                << "After timer: transitioning from active to timeout after 2s"
                << std::endl;
            return 2s;
          })),

      // Every 500ms while in active state, print a message
      hsm::transition(hsm::source("active"),
                      hsm::target("active"),  // Self-transition
                      hsm::every<std::chrono::milliseconds, hsm::Instance>([](hsm::Context& /*ctx*/, hsm::Instance& /*inst*/, hsm::Event& /*event*/) {
                        std::cout << "Every timer: still active (every 500ms)"
                                  << std::endl;
                        return 500ms;
                      })),

      hsm::initial(hsm::target("idle")));

  // Create HSM instance
  hsm::Instance instance;
  hsm::start(instance, model);

  std::cout << "Initial state: " << instance.state() << std::endl;

  // Let the state machine run for a few seconds to see the time-based
  // transitions
  std::cout << "Running for 5 seconds to observe time-based transitions..."
            << std::endl;

  for (int i = 0; i < 50; ++i) {
    std::this_thread::sleep_for(100ms);
    std::cout << "Current state: " << instance.state() << std::endl;
  }

  std::cout << "Final state: " << instance.state() << std::endl;
  hsm::stop(instance).wait();
  std::cout << "Time-based transition test completed!" << std::endl;

  return 0;
}