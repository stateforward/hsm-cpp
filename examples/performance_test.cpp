#include <sys/resource.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>

#include "hsm.hpp"

// Get memory usage in KB
long getMemoryUsage() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss;  // Peak memory usage in KB (Linux)
}

// Get current memory usage from /proc/self/status
long getCurrentMemoryUsage() {
  FILE* file = fopen("/proc/self/status", "r");
  if (!file) return -1;

  char line[128];
  long vmrss = -1;

  while (fgets(line, 128, file) != nullptr) {
    if (strncmp(line, "VmRSS:", 6) == 0) {
      sscanf(line, "VmRSS: %ld kB", &vmrss);
      break;
    }
  }
  fclose(file);
  return vmrss;
}

// Simple no-op behavior function
void noBehavior(hsm::Context& /*ctx*/, hsm::Instance& /*instance*/,
                hsm::Event& /*event*/) {
  // Do nothing
}

// More complex behavior for testing
void complexBehavior(hsm::Context& /*ctx*/, hsm::Instance& /*instance*/,
                     hsm::Event& /*event*/) {
  // Simulate some work
  volatile int sum = 0;
  for (int i = 0; i < 100; ++i) {
    sum += i;
  }
}

class TestInstance : public hsm::Instance {
 public:
  int transition_count = 0;

  void increment_transitions() { transition_count++; }
};

int main() {
  std::cout << "=== C++ HSM Performance Analysis ===" << std::endl;

  // Test different iteration counts
  std::vector<int> test_sizes = {1000, 10000, 100000};

  for (int iterations : test_sizes) {
    std::cout << "\n--- Testing with " << iterations << " iterations ---"
              << std::endl;

    long mem_before = getCurrentMemoryUsage();
    std::cout << "Memory before test: " << mem_before << " KB" << std::endl;

    // Create model
    auto start_model = std::chrono::high_resolution_clock::now();

    auto model = hsm::define(
        "PerfTestHSM",
        hsm::state("idle", hsm::entry(noBehavior), hsm::exit(noBehavior)),
        hsm::state("active", hsm::entry(complexBehavior),
                   hsm::exit(complexBehavior)),
        hsm::state("processing", hsm::entry(noBehavior), hsm::exit(noBehavior)),
        hsm::transition(hsm::on("start"), hsm::source("idle"),
                        hsm::target("active"), hsm::effect(noBehavior)),
        hsm::transition(hsm::on("process"), hsm::source("active"),
                        hsm::target("processing"),
                        hsm::effect(complexBehavior)),
        hsm::transition(hsm::on("finish"), hsm::source("processing"),
                        hsm::target("idle"), hsm::effect(noBehavior)),
        hsm::initial(hsm::target("idle")));

    auto end_model = std::chrono::high_resolution_clock::now();
    auto model_time = std::chrono::duration_cast<std::chrono::microseconds>(
        end_model - start_model);

    long mem_after_model = getCurrentMemoryUsage();
    std::cout << "Memory after model creation: " << mem_after_model << " KB"
              << std::endl;
    std::cout << "Model creation time: " << model_time.count() << " μs"
              << std::endl;
    std::cout << "Model memory overhead: " << (mem_after_model - mem_before)
              << " KB" << std::endl;
    std::cout << "Model members count: " << model->members.size() << std::endl;

    // Create HSM instance
    TestInstance instance;
    hsm::start(instance, model);

    long mem_after_hsm = getCurrentMemoryUsage();
    std::cout << "Memory after HSM creation: " << mem_after_hsm << " KB"
              << std::endl;
    std::cout << "HSM memory overhead: " << (mem_after_hsm - mem_after_model)
              << " KB" << std::endl;

    // Create events
    hsm::Event start_event("start");
    hsm::Event process_event("process");
    hsm::Event finish_event("finish");

    // Benchmark transitions
    auto start_bench = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
      instance.dispatch(start_event).wait();
      instance.dispatch(process_event).wait();
      instance.dispatch(finish_event).wait();
      instance.transition_count += 3;
    }

    auto end_bench = std::chrono::high_resolution_clock::now();
    auto bench_time = std::chrono::duration_cast<std::chrono::microseconds>(
        end_bench - start_bench);

    long mem_after_bench = getCurrentMemoryUsage();
    std::cout << "Memory after benchmark: " << mem_after_bench << " KB"
              << std::endl;
    std::cout << "Memory growth during benchmark: "
              << (mem_after_bench - mem_after_hsm) << " KB" << std::endl;

    // Performance metrics
    double total_transitions = static_cast<double>(iterations * 3);
    double transitions_per_second =
        total_transitions /
        (static_cast<double>(bench_time.count()) / 1000000.0);
    double microseconds_per_transition =
        static_cast<double>(bench_time.count()) / total_transitions;

    std::cout << "Total transitions: " << static_cast<int>(total_transitions)
              << std::endl;
    std::cout << "Total benchmark time: " << bench_time.count() << " μs"
              << std::endl;
    std::cout << "Transitions per second: "
              << static_cast<int>(transitions_per_second) << std::endl;
    std::cout << "Microseconds per transition: " << microseconds_per_transition
              << " μs" << std::endl;
    std::cout << "Final state: " << instance.state() << std::endl;

    // Memory efficiency
    if (mem_after_bench > 0 && total_transitions > 0) {
      double memory_per_transition =
          static_cast<double>(mem_after_bench - mem_before) * 1024.0 /
          total_transitions;  // bytes per transition
      std::cout << "Memory per transition: " << memory_per_transition
                << " bytes" << std::endl;
    }
  }

  std::cout << "\n=== Memory Layout Analysis ===" << std::endl;

  // Analyze memory layout of different components
  std::cout << "sizeof(hsm::State): " << sizeof(hsm::State) << " bytes"
            << std::endl;
  std::cout << "sizeof(hsm::Transition): " << sizeof(hsm::Transition)
            << " bytes" << std::endl;
  std::cout << "sizeof(hsm::Behavior): " << sizeof(hsm::Behavior) << " bytes"
            << std::endl;
  std::cout << "sizeof(hsm::Event): " << sizeof(hsm::Event) << " bytes"
            << std::endl;
  std::cout << "sizeof(hsm::HSM): " << sizeof(hsm::HSM) << " bytes"
            << std::endl;
  std::cout << "sizeof(hsm::Model): " << sizeof(hsm::Model) << " bytes"
            << std::endl;
  std::cout << "sizeof(hsm::ElementVariant): " << sizeof(hsm::ElementVariant)
            << " bytes" << std::endl;

  std::cout << "\n=== Scalability Test ===" << std::endl;

  // Test with many states
  auto large_model_start = std::chrono::high_resolution_clock::now();

  auto large_model = hsm::define("LargeHSM");
  // Note: We can't easily add many states with the current builder pattern
  // This would require a programmatic way to build models

  auto large_model_end = std::chrono::high_resolution_clock::now();
  auto large_model_time = std::chrono::duration_cast<std::chrono::microseconds>(
      large_model_end - large_model_start);

  std::cout << "Large model creation time: " << large_model_time.count()
            << " μs" << std::endl;

  return 0;
}