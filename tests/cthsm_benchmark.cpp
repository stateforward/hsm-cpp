#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "cthsm/cthsm.hpp"

// Struct to hold benchmark results
struct BenchmarkResult {
  std::string name;
  double transitionsPerSecond;
  double percentChange;
  size_t memoryUsedBytes;
  size_t peakMemoryBytes;  // Not easily measurable portably without OS headers
  int iterations;
};

// Dummy memory usage (hard to measure portably/consistently in this script
// without headers)
size_t getCurrentMemoryUsage() {
  return 0;  // Placeholder
}

using namespace cthsm;

struct BenchmarkInstance : public Instance {
  // Data for activities or actions if needed
};

// Behaviors
void noBehavior(Context&, Instance&, const EventBase&) {}

void activityBehavior(Context&, Instance&, const EventBase&) {
  std::this_thread::yield();
}

// Generic benchmark runner
template <typename SM, typename InstanceType>
BenchmarkResult runBenchmark(const std::string& scenarioName,
                             const std::string& event1Name,
                             const std::string& event2Name,
                             int warmupIterations = 1000,
                             int benchmarkIterations = 10000,
                             double* baselineSpeed = nullptr) {
  BenchmarkResult result;
  result.name = scenarioName;
  result.iterations = benchmarkIterations;

  // Create instance and start
  InstanceType instance;
  SM sm;
  sm.start(instance);

  // Warmup
  for (int i = 0; i < warmupIterations; i++) {
    sm.dispatch(instance, EventBase{event1Name});
    sm.dispatch(instance, EventBase{event2Name});
  }

  // Benchmark
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < benchmarkIterations; i++) {
    sm.dispatch(instance, EventBase{event1Name});
    sm.dispatch(instance, EventBase{event2Name});
  }

  auto end = std::chrono::high_resolution_clock::now();

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
  std::cout << std::endl;

  return result;
}

void writeResultsToCSV(const std::vector<BenchmarkResult>& results,
                       const std::string& filename) {
  std::ofstream csv(filename);
  csv << "Scenario,Transitions/sec,Change %,Iterations\n";
  for (const auto& result : results) {
    csv << "\"" << result.name << "\"," << std::fixed << std::setprecision(0)
        << result.transitionsPerSecond << "," << std::fixed
        << std::setprecision(1) << result.percentChange << ","
        << result.iterations << "\n";
  }
  csv.close();
  std::cout << "\nResults written to " << filename << std::endl;
}

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

// Define models as constexpr
// 1. Nested states (no actions)
constexpr auto model1 =
    define("TestHSM1",
           state("parent", state("child1"), state("child2"),
                 initial(target("/TestHSM1/parent/child1")),
                 transition(on("toChild2"), source("/TestHSM1/parent/child1"),
                            target("/TestHSM1/parent/child2")),
                 transition(on("toChild1"), source("/TestHSM1/parent/child2"),
                            target("/TestHSM1/parent/child1"))),
           initial(target("/TestHSM1/parent")));

// 1.a With entry
constexpr auto model1a = define(
    "TestHSM1a",
    state("parent", entry(noBehavior), state("child1", entry(noBehavior)),
          state("child2", entry(noBehavior)),
          initial(target("/TestHSM1a/parent/child1")),
          transition(on("toChild2"), source("/TestHSM1a/parent/child1"),
                     target("/TestHSM1a/parent/child2")),
          transition(on("toChild1"), source("/TestHSM1a/parent/child2"),
                     target("/TestHSM1a/parent/child1"))),
    initial(target("/TestHSM1a/parent")));

// 1.b Entry + Activity
constexpr auto model1b =
    define("TestHSM1b",
           state("parent", entry(noBehavior), activity(activityBehavior),
                 state("child1", entry(noBehavior), activity(activityBehavior)),
                 state("child2", entry(noBehavior), activity(activityBehavior)),
                 initial(target("/TestHSM1b/parent/child1")),
                 transition(on("toChild2"), source("/TestHSM1b/parent/child1"),
                            target("/TestHSM1b/parent/child2")),
                 transition(on("toChild1"), source("/TestHSM1b/parent/child2"),
                            target("/TestHSM1b/parent/child1"))),
           initial(target("/TestHSM1b/parent")));

// 1.c Entry + Exit + Activity
constexpr auto model1c =
    define("TestHSM1c",
           state("parent", entry(noBehavior), exit(noBehavior),
                 activity(activityBehavior),
                 state("child1", entry(noBehavior), exit(noBehavior),
                       activity(activityBehavior)),
                 state("child2", entry(noBehavior), exit(noBehavior),
                       activity(activityBehavior)),
                 initial(target("/TestHSM1c/parent/child1")),
                 transition(on("toChild2"), source("/TestHSM1c/parent/child1"),
                            target("/TestHSM1c/parent/child2")),
                 transition(on("toChild1"), source("/TestHSM1c/parent/child2"),
                            target("/TestHSM1c/parent/child1"))),
           initial(target("/TestHSM1c/parent")));

// 1.d Entry + Exit + Activity + Effect
constexpr auto model1d = define(
    "TestHSM1d",
    state("parent", entry(noBehavior), exit(noBehavior),
          activity(activityBehavior),
          state("child1", entry(noBehavior), exit(noBehavior),
                activity(activityBehavior)),
          state("child2", entry(noBehavior), exit(noBehavior),
                activity(activityBehavior)),
          initial(target("/TestHSM1d/parent/child1")),
          transition(on("toChild2"), source("/TestHSM1d/parent/child1"),
                     target("/TestHSM1d/parent/child2"), effect(noBehavior)),
          transition(on("toChild1"), source("/TestHSM1d/parent/child2"),
                     target("/TestHSM1d/parent/child1"), effect(noBehavior))),
    initial(target("/TestHSM1d/parent")));

// Deep Nesting
constexpr auto modelDeep = define(
    "TestHSMDeep",
    state("level1", entry(noBehavior), exit(noBehavior),
          state("level2", entry(noBehavior), exit(noBehavior),
                state("level3a", entry(noBehavior), exit(noBehavior)),
                state("level3b", entry(noBehavior), exit(noBehavior)),
                initial(target("/TestHSMDeep/level1/level2/level3a")),
                transition(on("toLevel3b"),
                           source("/TestHSMDeep/level1/level2/level3a"),
                           target("/TestHSMDeep/level1/level2/level3b")),
                transition(on("toLevel3a"),
                           source("/TestHSMDeep/level1/level2/level3b"),
                           target("/TestHSMDeep/level1/level2/level3a"))),
          initial(target("/TestHSMDeep/level1/level2"))),
    initial(target("/TestHSMDeep/level1")));

// Cross Hierarchy
constexpr auto modelCross =
    define("TestHSMCrossHierarchy",
           state("parent1", entry(noBehavior), exit(noBehavior),
                 state("child1", entry(noBehavior), exit(noBehavior)),
                 initial(target("/TestHSMCrossHierarchy/parent1/child1"))),
           state("parent2", entry(noBehavior), exit(noBehavior),
                 state("child2", entry(noBehavior), exit(noBehavior)),
                 initial(target("/TestHSMCrossHierarchy/parent2/child2"))),
           transition(on("toParent2"), source("/TestHSMCrossHierarchy/parent1"),
                      target("/TestHSMCrossHierarchy/parent2")),
           transition(on("toParent1"), source("/TestHSMCrossHierarchy/parent2"),
                      target("/TestHSMCrossHierarchy/parent1")),
           initial(target("/TestHSMCrossHierarchy/parent1")));

// Invalid Events
constexpr auto modelInvalid = define(
    "TestHSMInvalidEvents",
    state(
        "level1",
        state("level2",
              state("level3",
                    transition(
                        on("validEvent"),
                        target("/TestHSMInvalidEvents/level1/level2/level3"))),
              initial(target("/TestHSMInvalidEvents/level1/level2/level3"))),
        initial(target("/TestHSMInvalidEvents/level1/level2"))),
    initial(target("/TestHSMInvalidEvents/level1")));

int main() {
  std::cout << "CTHSM Benchmark" << std::endl;
  std::cout << "=================" << std::endl;

  std::vector<BenchmarkResult> allResults;
  double baselineSpeed = 0;

  allResults.push_back(
      runBenchmark<compile<model1, BenchmarkInstance>, BenchmarkInstance>(
          "1. Nested states (no entry/exit/activity)", "toChild2", "toChild1",
          1000, 10000, &baselineSpeed));

  allResults.push_back(
      runBenchmark<compile<model1a, BenchmarkInstance>, BenchmarkInstance>(
          "1.a With entry", "toChild2", "toChild1", 1000, 10000,
          &baselineSpeed));

  allResults.push_back(
      runBenchmark<compile<model1b, BenchmarkInstance>, BenchmarkInstance>(
          "1.b With entry+activity", "toChild2", "toChild1", 1000, 10000,
          &baselineSpeed));

  allResults.push_back(
      runBenchmark<compile<model1c, BenchmarkInstance>, BenchmarkInstance>(
          "1.c With entry+exit+activity", "toChild2", "toChild1", 1000, 10000,
          &baselineSpeed));

  allResults.push_back(
      runBenchmark<compile<model1d, BenchmarkInstance>, BenchmarkInstance>(
          "1.d With entry+exit+activity+effect", "toChild2", "toChild1", 1000,
          10000, &baselineSpeed));

  allResults.push_back(
      runBenchmark<compile<modelDeep, BenchmarkInstance>, BenchmarkInstance>(
          "Deep nesting", "toLevel3b", "toLevel3a", 1000, 10000));

  allResults.push_back(
      runBenchmark<compile<modelCross, BenchmarkInstance>, BenchmarkInstance>(
          "Cross hierarchy", "toParent2", "toParent1", 1000, 10000));

  allResults.push_back(
      runBenchmark<compile<modelInvalid, BenchmarkInstance>, BenchmarkInstance>(
          "Invalid events", "invalid1", "invalid2", 1000, 10000));

  writeResultsToCSV(allResults, "cthsm_benchmark_results.csv");
  writeResultsToJSON(allResults, "cthsm_benchmark_results.json");

  return 0;
}
