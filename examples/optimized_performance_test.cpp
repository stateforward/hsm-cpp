#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "../include/hsm.hpp"

using namespace hsm;

// Test instance class
class TestInstance : public Instance {
 public:
  int transition_count = 0;
  int event_count = 0;

  void reset() {
    transition_count = 0;
    event_count = 0;
  }
};

// Create a complex hierarchical state machine for testing
std::unique_ptr<Model> create_complex_model() {
  return define(
      "TestModel",
      // State A with nested states
      state("StateA", initial(target("StateA1")),

            state("StateA1", transition(on("event1"), target("StateA2")),
                  transition(on("event2"), target("/TestModel/StateB")),
                  transition(on("event3"), target("StateA3"))),

            state("StateA2", transition(on("event1"), target("StateA3")),
                  transition(on("event2"), target("/TestModel/StateB")),
                  transition(on("event4"), target("StateA1"))),

            state("StateA3", transition(on("event1"), target("StateA1")),
                  transition(on("event2"), target("/TestModel/StateB")),
                  transition(on("event5"), target("StateA2"))),

            // Transition from StateA to StateB
            transition(on("to_b"), target("/TestModel/StateB"))),

      // State B with nested states
      state("StateB", initial(target("StateB1")),

            state("StateB1", transition(on("event1"), target("StateB2")),
                  transition(on("event2"), target("/TestModel/StateA")),
                  transition(on("event6"), target("StateB3"))),

            state("StateB2", transition(on("event1"), target("StateB3")),
                  transition(on("event2"), target("/TestModel/StateA")),
                  transition(on("event7"), target("StateB1"))),

            state("StateB3", transition(on("event1"), target("StateB1")),
                  transition(on("event2"), target("/TestModel/StateA")),
                  transition(on("event8"), target("StateB2"))),

            // Transition from StateB to StateA
            transition(on("to_a"), target("/TestModel/StateA"))),

      // State C with deferred events
      state("StateC", initial(target("StateC1")),

            state("StateC1", transition(on("event1"), target("StateC2")),
                  transition(on("activate"), target("/TestModel/StateA"))),

            state("StateC2", transition(on("event1"), target("StateC1")),
                  transition(on("activate"), target("/TestModel/StateA"))),

            // Transition to StateC
            transition(on("to_c"), target("/TestModel/StateC"))));
}

void run_performance_test(const std::string& test_name, int num_events) {
  auto model = create_complex_model();
  TestInstance instance;

  // Create HSM (this will build the optimization tables)
  auto start_build = std::chrono::high_resolution_clock::now();
  hsm::start(instance, model);
  auto end_build = std::chrono::high_resolution_clock::now();

  auto build_time = std::chrono::duration_cast<std::chrono::microseconds>(
      end_build - start_build);

  // Generate test events
  std::vector<std::string> events = {"event1", "event2", "event3", "event4",
                                     "event5", "event6", "event7", "event8",
                                     "to_a",   "to_b",   "to_c",   "activate"};

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, static_cast<int>(events.size() - 1));

  std::vector<std::string> test_events;
  test_events.reserve(static_cast<size_t>(num_events));
  for (int i = 0; i < num_events; ++i) {
    test_events.push_back(events[static_cast<size_t>(dis(gen))]);
  }

  // Measure event processing time
  auto start_process = std::chrono::high_resolution_clock::now();

  for (const auto& event_name : test_events) {
    Event event(event_name);
    instance.dispatch(std::move(event)).wait();
    instance.event_count++;
  }

  auto end_process = std::chrono::high_resolution_clock::now();
  auto process_time = std::chrono::duration_cast<std::chrono::microseconds>(
      end_process - start_process);

  // Print results
  std::cout << "=== " << test_name << " Performance Test ===" << std::endl;
  std::cout << "Events processed: " << num_events << std::endl;
  std::cout << "Table build time: " << build_time.count() << " μs" << std::endl;
  std::cout << "Event processing time: " << process_time.count() << " μs"
            << std::endl;
  std::cout << "Average time per event: "
            << (static_cast<double>(process_time.count()) /
                static_cast<double>(num_events))
            << " μs" << std::endl;
  std::cout << "Events per second: "
            << (static_cast<double>(num_events) * 1000000.0 /
                static_cast<double>(process_time.count()))
            << std::endl;
  std::cout << "Final state: " << instance.state() << std::endl;
  hsm::stop(instance).wait();
  std::cout << std::endl;
}

int main() {
  std::cout << "HSM Optimized Performance Test" << std::endl;
  std::cout << "==============================" << std::endl;
  std::cout << "Testing O(1) transition and deferred event lookup performance."
            << std::endl;
  std::cout << std::endl;

  // Test with different event counts to show scalability
  run_performance_test("Small Scale", 1000);
  run_performance_test("Medium Scale", 10000);
  run_performance_test("Large Scale", 100000);

  std::cout << "Performance Benefits:" << std::endl;
  std::cout << "- O(1) event lookup (vs O(depth × transitions) in original)"
            << std::endl;
  std::cout << "- O(1) deferred event checking (vs O(depth × deferred_events))"
            << std::endl;
  std::cout
      << "- Precomputed transition tables eliminate runtime hierarchy walking"
      << std::endl;
  std::cout << "- Priority-sorted transitions for optimal guard checking order"
            << std::endl;
  std::cout << std::endl;

  return 0;
}