#include <chrono>
#include <iostream>
#include <string>

#include "hsm.hpp"

// Simple enum-based state machine for comparison
enum class SimpleState { Idle, Active, Processing };

class SimpleStateMachine {
 public:
  SimpleState current_state = SimpleState::Idle;
  int transition_count = 0;

  void dispatch(const std::string& event) {
    switch (current_state) {
      case SimpleState::Idle:
        if (event == "start") {
          current_state = SimpleState::Active;
          transition_count++;
        }
        break;
      case SimpleState::Active:
        if (event == "process") {
          current_state = SimpleState::Processing;
          transition_count++;
        }
        break;
      case SimpleState::Processing:
        if (event == "finish") {
          current_state = SimpleState::Idle;
          transition_count++;
        }
        break;
    }
  }

  std::string state() const {
    switch (current_state) {
      case SimpleState::Idle:
        return "Idle";
      case SimpleState::Active:
        return "Active";
      case SimpleState::Processing:
        return "Processing";
    }
    return "Unknown";
  }
};

void noBehavior(hsm::Context& /*ctx*/, hsm::Instance& /*instance*/,
                hsm::Event& /*event*/) {
  // Do nothing
}

int main() {
  const int iterations = 100000;

  std::cout << "=== State Machine Performance Comparison ===" << std::endl;
  std::cout << "Iterations: " << iterations << std::endl;

  // Test simple enum-based state machine
  {
    std::cout << "\n--- Simple Enum State Machine ---" << std::endl;
    SimpleStateMachine simple_sm;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
      simple_sm.dispatch("start");
      simple_sm.dispatch("process");
      simple_sm.dispatch("finish");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Total time: " << duration.count() << " μs" << std::endl;
    std::cout << "Transitions: " << simple_sm.transition_count << std::endl;
    std::cout << "μs per transition: "
              << static_cast<double>(duration.count()) /
                     simple_sm.transition_count
              << std::endl;
    std::cout << "Transitions per second: "
              << static_cast<int>(
                     simple_sm.transition_count /
                     (static_cast<double>(duration.count()) / 1000000.0))
              << std::endl;
    std::cout << "Final state: " << simple_sm.state() << std::endl;
  }

  // Test HSM implementation
  {
    std::cout << "\n--- HSM Implementation ---" << std::endl;

    auto model = hsm::define(
        "ComparisonHSM",
        hsm::state("idle", hsm::entry(noBehavior), hsm::exit(noBehavior)),
        hsm::state("active", hsm::entry(noBehavior), hsm::exit(noBehavior)),
        hsm::state("processing", hsm::entry(noBehavior), hsm::exit(noBehavior)),
        hsm::transition(hsm::on("start"), hsm::source("idle"),
                        hsm::target("active"), hsm::effect(noBehavior)),
        hsm::transition(hsm::on("process"), hsm::source("active"),
                        hsm::target("processing"), hsm::effect(noBehavior)),
        hsm::transition(hsm::on("finish"), hsm::source("processing"),
                        hsm::target("idle"), hsm::effect(noBehavior)),
        hsm::initial(hsm::target("idle")));

    hsm::Instance instance;
    hsm::start(instance, model);

    hsm::Event start_event("start");
    hsm::Event process_event("process");
    hsm::Event finish_event("finish");

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
      instance.dispatch(start_event).wait();
      instance.dispatch(process_event).wait();
      instance.dispatch(finish_event).wait();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    int total_transitions = iterations * 3;
    std::cout << "Total time: " << duration.count() << " μs" << std::endl;
    std::cout << "Transitions: " << total_transitions << std::endl;
    std::cout << "μs per transition: "
              << static_cast<double>(duration.count()) / total_transitions
              << std::endl;
    std::cout << "Transitions per second: "
              << static_cast<int>(
                     total_transitions /
                     (static_cast<double>(duration.count()) / 1000000.0))
              << std::endl;
    std::cout << "Final state: " << instance.state() << std::endl;
    hsm::stop(instance).wait();
  }

  std::cout << "\n=== Memory Footprint Analysis ===" << std::endl;
  std::cout << "SimpleStateMachine size: " << sizeof(SimpleStateMachine)
            << " bytes" << std::endl;
  std::cout << "HSM size: " << sizeof(hsm::HSM) << " bytes" << std::endl;
  std::cout << "Model size: " << sizeof(hsm::Model) << " bytes" << std::endl;

  return 0;
}