#include <gperftools/heap-profiler.h>
#include <gperftools/profiler.h>
#include <sys/resource.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "hsm.hpp"

// Define instance class similar to Python's hsm.Instance
class THSM : public hsm::Instance {
 public:
  explicit THSM() : hsm::Instance() {}
};

// Struct to hold benchmark results
struct BenchmarkResult {
  std::string name;
  double transitionsPerSecond;
  double percentChange;
  size_t memoryUsedBytes;
  size_t peakMemoryBytes;
  int iterations;
};

// Get current memory usage in bytes
size_t getCurrentMemoryUsage() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return static_cast<size_t>(usage.ru_maxrss) *
         1024;  // Convert KB to bytes on Linux
}

// Simple no-op behavior function
void noBehavior(hsm::HSM& /*sm*/, hsm::Event& /*event*/,
                hsm::Context& /*signal*/) {
  // Do nothing
}

// Activity function that runs in background
void activityBehavior(hsm::HSM& /*sm*/, hsm::Event& /*event*/,
                      hsm::Context& signal) {
  // Simple activity that just checks the signal once
  // This avoids potential deadlocks in rapid transitions
  if (!signal.is_set()) {
    // Simulate some minimal work
    std::this_thread::yield();
  }
}

// Helper function to create a benchmark scenario and measure transitions per
// second
BenchmarkResult runBenchmark(const std::string& scenarioName,
                             std::unique_ptr<hsm::Model> model,
                             const std::string& event1Name,
                             const std::string& event2Name,
                             int warmupIterations = 1000,
                             int benchmarkIterations = 10000,
                             double* baselineSpeed = nullptr) {
  BenchmarkResult result;
  result.name = scenarioName;
  result.iterations = benchmarkIterations;

  // Record memory before creating HSM
  size_t memBefore = getCurrentMemoryUsage();

  // Create instance and start it with the model
  THSM instance;
  hsm::HSM hsm_instance(instance, model);

  hsm::Event event1;
  event1.name = event1Name;
  hsm::Event event2;
  event2.name = event2Name;

  // Warmup
  for (int i = 0; i < warmupIterations; i++) {
    instance.dispatch(event1).wait();
    instance.dispatch(event2).wait();
  }

  // Benchmark
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < benchmarkIterations; i++) {
    instance.dispatch(event1).wait();
    instance.dispatch(event2).wait();
  }

  auto end = std::chrono::high_resolution_clock::now();

  // Record peak memory usage
  result.peakMemoryBytes = getCurrentMemoryUsage();
  result.memoryUsedBytes = result.peakMemoryBytes - memBefore;

  // Calculate transitions per second
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  double totalTransitions = static_cast<double>(benchmarkIterations) *
                            2.0;  // Two transitions per iteration
  result.transitionsPerSecond =
      (totalTransitions / static_cast<double>(duration)) * 1000000.0;

  // Calculate percentage change if baseline provided
  result.percentChange = 0.0;
  if (baselineSpeed != nullptr) {
    if (*baselineSpeed == 0) {
      *baselineSpeed = result.transitionsPerSecond;
    } else {
      result.percentChange =
          ((result.transitionsPerSecond - *baselineSpeed) / *baselineSpeed) *
          100.0;
    }
  }

  // Print to console
  std::cout << std::left << std::setw(65) << scenarioName << std::right
            << std::setw(12) << std::fixed << std::setprecision(0)
            << result.transitionsPerSecond << " trans/sec";

  if (baselineSpeed != nullptr &&
      *baselineSpeed != result.transitionsPerSecond) {
    std::cout << " (" << std::showpos << std::setprecision(1)
              << result.percentChange << "%" << std::noshowpos << ")";
  } else if (baselineSpeed != nullptr) {
    std::cout << " (baseline)";
  }

  std::cout << "  Memory: " << (result.memoryUsedBytes / 1024) << " KB"
            << std::endl;

  return result;
}

// Write results to CSV file
void writeResultsToCSV(const std::vector<BenchmarkResult>& results,
                       const std::string& filename) {
  std::ofstream csv(filename);

  // Write header
  csv << "Scenario,Transitions/sec,Change %,Memory (KB),Peak Memory "
         "(KB),Iterations\n";

  // Write data
  for (const auto& result : results) {
    csv << "\"" << result.name << "\"," << std::fixed << std::setprecision(0)
        << result.transitionsPerSecond << "," << std::fixed
        << std::setprecision(1) << result.percentChange << ","
        << (result.memoryUsedBytes / 1024) << ","
        << (result.peakMemoryBytes / 1024) << "," << result.iterations << "\n";
  }

  csv.close();
  std::cout << "\nResults written to " << filename << std::endl;
}

// Write results to JSON file
void writeResultsToJSON(const std::vector<BenchmarkResult>& results,
                        const std::string& filename) {
  std::ofstream json(filename);

  json << "{\n";
  json << "  \"timestamp\": \""
       << std::chrono::system_clock::now().time_since_epoch().count()
       << "\",\n";
  json << "  \"results\": [\n";

  for (size_t i = 0; i < results.size(); ++i) {
    const auto& result = results[i];
    json << "    {\n";
    json << "      \"name\": \"" << result.name << "\",\n";
    json << "      \"transitionsPerSecond\": " << result.transitionsPerSecond
         << ",\n";
    json << "      \"percentChange\": " << result.percentChange << ",\n";
    json << "      \"memoryUsedBytes\": " << result.memoryUsedBytes << ",\n";
    json << "      \"peakMemoryBytes\": " << result.peakMemoryBytes << ",\n";
    json << "      \"iterations\": " << result.iterations << "\n";
    json << "    }";
    if (i < results.size() - 1) json << ",";
    json << "\n";
  }

  json << "  ]\n";
  json << "}\n";

  json.close();
  std::cout << "Results written to " << filename << std::endl;
}

int main() {
  HeapProfilerStart("hsm_benchmark");

  std::cout << "HSM Nested State Transition Benchmark" << std::endl;
  std::cout << "=====================================" << std::endl;
  std::cout << "Running " << 1000 << " iterations per scenario (after warmup)"
            << std::endl;
  std::cout << std::endl;

  std::vector<BenchmarkResult> allResults;
  double baselineSpeed = 0;

  // 1. Baseline: Nested states without entry, exit, or activities
  {
    auto model = hsm::define(
        "TestHSM1",
        hsm::state("parent", hsm::state("child1"), hsm::state("child2"),
                   hsm::initial(hsm::target("child1")),
                   hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                                   hsm::target("child2")),
                   hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                                   hsm::target("child1"))),
        hsm::initial(hsm::target("parent")));

    allResults.push_back(runBenchmark(
        "1. Nested states (no entry/exit/activity)", std::move(model),
        "toChild2", "toChild1", 1000, 1000, &baselineSpeed));
  }

  // 1.a Nested states with entry functions only
  {
    auto model = hsm::define(
        "TestHSM1a",
        hsm::state("parent", hsm::entry(noBehavior),
                   hsm::state("child1", hsm::entry(noBehavior)),
                   hsm::state("child2", hsm::entry(noBehavior)),
                   hsm::initial(hsm::target("child1")),
                   hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                                   hsm::target("child2")),
                   hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                                   hsm::target("child1"))),
        hsm::initial(hsm::target("parent")));

    allResults.push_back(runBenchmark("1.a Nested states with entry functions",
                                      std::move(model), "toChild2", "toChild1",
                                      1000, 1000, &baselineSpeed));
  }

  // 1.b Nested states with entry and activity functions
  {
    auto model = hsm::define(
        "TestHSM1b",
        hsm::state("parent", hsm::entry(noBehavior),
                   hsm::activity(activityBehavior),
                   hsm::state("child1", hsm::entry(noBehavior),
                              hsm::activity(activityBehavior)),
                   hsm::state("child2", hsm::entry(noBehavior),
                              hsm::activity(activityBehavior)),
                   hsm::initial(hsm::target("child1")),
                   hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                                   hsm::target("child2")),
                   hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                                   hsm::target("child1"))),
        hsm::initial(hsm::target("parent")));

    allResults.push_back(runBenchmark(
        "1.b Nested states with entry and activity functions", std::move(model),
        "toChild2", "toChild1", 1000, 1000, &baselineSpeed));
  }

  // 1.c Nested states with entry, exit, and activity functions
  {
    auto model = hsm::define(
        "TestHSM1c",
        hsm::state(
            "parent", hsm::entry(noBehavior), hsm::exit(noBehavior),
            hsm::activity(activityBehavior),
            hsm::state("child1", hsm::entry(noBehavior), hsm::exit(noBehavior),
                       hsm::activity(activityBehavior)),
            hsm::state("child2", hsm::entry(noBehavior), hsm::exit(noBehavior),
                       hsm::activity(activityBehavior)),
            hsm::initial(hsm::target("child1")),
            hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                            hsm::target("child2")),
            hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                            hsm::target("child1"))),
        hsm::initial(hsm::target("parent")));

    allResults.push_back(runBenchmark(
        "1.c Nested states with entry, exit, and activity functions",
        std::move(model), "toChild2", "toChild1", 1000, 1000, &baselineSpeed));
  }

  // 1.d Nested states with entry, exit, activity functions and transition
  // effects
  {
    auto model = hsm::define(
        "TestHSM1d",
        hsm::state(
            "parent", hsm::entry(noBehavior), hsm::exit(noBehavior),
            hsm::activity(activityBehavior),
            hsm::state("child1", hsm::entry(noBehavior), hsm::exit(noBehavior),
                       hsm::activity(activityBehavior)),
            hsm::state("child2", hsm::entry(noBehavior), hsm::exit(noBehavior),
                       hsm::activity(activityBehavior)),
            hsm::initial(hsm::target("child1")),
            hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                            hsm::target("child2"), hsm::effect(noBehavior)),
            hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                            hsm::target("child1"), hsm::effect(noBehavior))),
        hsm::initial(hsm::target("parent")));

    allResults.push_back(runBenchmark(
        "1.d Nested states with entry, exit, activity, and transition effect",
        std::move(model), "toChild2", "toChild1", 1000, 1000, &baselineSpeed));
  }

  std::cout << std::endl;
  std::cout << "Additional scenarios:" << std::endl;

  // Additional test: Deep nesting (3 levels)
  {
    auto model = hsm::define(
        "TestHSMDeep",
        hsm::state(
            "level1", hsm::entry(noBehavior), hsm::exit(noBehavior),
            hsm::state(
                "level2", hsm::entry(noBehavior), hsm::exit(noBehavior),
                hsm::state("level3a", hsm::entry(noBehavior),
                           hsm::exit(noBehavior)),
                hsm::state("level3b", hsm::entry(noBehavior),
                           hsm::exit(noBehavior)),
                hsm::initial(hsm::target("level3a")),
                hsm::transition(hsm::on("toLevel3b"), hsm::source("level3a"),
                                hsm::target("level3b")),
                hsm::transition(hsm::on("toLevel3a"), hsm::source("level3b"),
                                hsm::target("level3a"))),
            hsm::initial(hsm::target("level2"))),
        hsm::initial(hsm::target("level1")));

    allResults.push_back(runBenchmark("Deep nesting (3 levels) with entry/exit",
                                      std::move(model), "toLevel3b",
                                      "toLevel3a"));
  }

  // Test exiting and entering nested states from outside
  {
    auto model = hsm::define(
        "TestHSMCrossHierarchy",
        hsm::state(
            "parent1", hsm::entry(noBehavior), hsm::exit(noBehavior),
            hsm::state("child1", hsm::entry(noBehavior), hsm::exit(noBehavior)),
            hsm::initial(hsm::target("child1"))),
        hsm::state(
            "parent2", hsm::entry(noBehavior), hsm::exit(noBehavior),
            hsm::state("child2", hsm::entry(noBehavior), hsm::exit(noBehavior)),
            hsm::initial(hsm::target("child2"))),
        hsm::transition(hsm::on("toParent2"), hsm::source("parent1"),
                        hsm::target("parent2")),
        hsm::transition(hsm::on("toParent1"), hsm::source("parent2"),
                        hsm::target("parent1")),
        hsm::initial(hsm::target("parent1")));

    allResults.push_back(
        runBenchmark("Cross-hierarchy transitions with entry/exit",
                     std::move(model), "toParent2", "toParent1"));
  }

  // Invalid event handling (graceful failure performance test)
  {
    auto model = hsm::define(
        "TestHSMInvalidEvents",
        hsm::state(
            "level1",
            hsm::state("level2",
                       hsm::state("level3",
                                  // Only has one valid transition
                                  hsm::transition(hsm::on("validEvent"),
                                                  hsm::target("level3"))),
                       hsm::initial(hsm::target("level3"))),
            hsm::initial(hsm::target("level2"))),
        hsm::initial(hsm::target("level1")));

    allResults.push_back(runBenchmark(
        "Invalid event handling (graceful failure)", std::move(model),
        "invalidEvent1", "invalidEvent2", 500,
        500));  // Use fewer iterations for invalid events since they're faster
  }

  HeapProfilerStop();

  // Write results to files
  writeResultsToCSV(allResults, "hsm_benchmark_results.csv");
  writeResultsToJSON(allResults, "hsm_benchmark_results.json");

  std::cout << std::endl;
  std::cout << "Benchmark completed." << std::endl;
  std::cout << std::endl;
  std::cout
      << "Note: Transitions/sec includes both state changes in each iteration."
      << std::endl;
  std::cout << "      Lower numbers indicate more overhead from "
               "entry/exit/activity functions."
            << std::endl;

  return 0;
}