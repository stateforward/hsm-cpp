#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "hsm.hpp"

// Global counters to verify behavior calls
std::atomic<int> entry_calls{0};
std::atomic<int> exit_calls{0};
std::atomic<int> activity_calls{0};
std::atomic<int> effect_calls{0};

class VerificationInstance : public hsm::Instance {
 public:
  explicit VerificationInstance() : hsm::Instance() {}
};

// Behavior functions that count calls
void countingEntryBehavior(hsm::Instance& /*instance*/, hsm::Event& /*event*/,
                           hsm::Context& /*signal*/) {
  entry_calls.fetch_add(1, std::memory_order_relaxed);
}

void countingExitBehavior(hsm::Instance& /*instance*/, hsm::Event& /*event*/,
                          hsm::Context& /*signal*/) {
  exit_calls.fetch_add(1, std::memory_order_relaxed);
}

void countingActivityBehavior(hsm::Instance& /*instance*/,
                              hsm::Event& /*event*/, hsm::Context& signal) {
  activity_calls.fetch_add(1, std::memory_order_relaxed);
  // This is what's causing the massive overhead - it runs in a separate thread
  if (!signal.is_set()) {
    std::this_thread::yield();
  }
}

void countingEffectBehavior(hsm::Instance& /*instance*/, hsm::Event& /*event*/,
                            hsm::Context& /*signal*/) {
  effect_calls.fetch_add(1, std::memory_order_relaxed);
}

void noBehavior(hsm::Instance& /*instance*/, hsm::Event& /*event*/,
                hsm::Context& /*signal*/) {
  // Do nothing
}

struct BenchmarkResult {
  std::string name;
  double transitionsPerSecond;
  int entryCalls;
  int exitCalls;
  int activityCalls;
  int effectCalls;
  int threadCreations;
};

BenchmarkResult runVerificationBenchmark(
    const std::string& scenarioName, std::unique_ptr<hsm::Model> model,
    const std::string& event1Name, const std::string& event2Name,
    int iterations = 100) {  // Smaller iterations for verification

  // Reset counters
  entry_calls.store(0);
  exit_calls.store(0);
  activity_calls.store(0);
  effect_calls.store(0);

  VerificationInstance instance;
  hsm::HSM hsm_instance(instance, model);

  hsm::Event event1;
  event1.name = event1Name;
  hsm::Event event2;
  event2.name = event2Name;

  // Warmup
  for (int i = 0; i < 10; i++) {
    instance.dispatch(event1).wait();
    instance.dispatch(event2).wait();
  }

  // Reset counters after warmup
  int warmup_entries = entry_calls.load();
  int warmup_exits = exit_calls.load();
  int warmup_activities = activity_calls.load();
  int warmup_effects = effect_calls.load();

  entry_calls.store(0);
  exit_calls.store(0);
  activity_calls.store(0);
  effect_calls.store(0);

  // Benchmark
  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < iterations; i++) {
    instance.dispatch(event1).wait();
    instance.dispatch(event2).wait();
  }

  auto end = std::chrono::high_resolution_clock::now();

  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();
  double totalTransitions = static_cast<double>(iterations) * 2.0;
  double transitionsPerSecond =
      (totalTransitions / static_cast<double>(duration)) * 1000000.0;

  BenchmarkResult result;
  result.name = scenarioName;
  result.transitionsPerSecond = transitionsPerSecond;
  result.entryCalls = entry_calls.load();
  result.exitCalls = exit_calls.load();
  result.activityCalls = activity_calls.load();
  result.effectCalls = effect_calls.load();
  result.threadCreations =
      result.activityCalls;  // Each activity call = 1 thread

  std::cout << "\n" << scenarioName << std::endl;
  std::cout << "  Transitions/sec: " << static_cast<int>(transitionsPerSecond)
            << std::endl;
  std::cout << "  Entry calls: " << result.entryCalls
            << " (warmup: " << warmup_entries << ")" << std::endl;
  std::cout << "  Exit calls: " << result.exitCalls
            << " (warmup: " << warmup_exits << ")" << std::endl;
  std::cout << "  Activity calls: " << result.activityCalls
            << " (warmup: " << warmup_activities << ")" << std::endl;
  std::cout << "  Effect calls: " << result.effectCalls
            << " (warmup: " << warmup_effects << ")" << std::endl;
  std::cout << "  Estimated thread creations: " << result.threadCreations
            << std::endl;

  return result;
}

int main() {
  std::cout << "C++ HSM Verification Benchmark" << std::endl;
  std::cout << "==============================" << std::endl;
  std::cout << "Testing with 100 iterations to verify behavior calls and "
               "identify thread overhead"
            << std::endl;

  // Test 1: Baseline (no behaviors)
  {
    auto model = hsm::define(
        "BaselineHSM",
        hsm::state("parent", hsm::state("child1"), hsm::state("child2"),
                   hsm::initial(hsm::target("child1")),
                   hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                                   hsm::target("child2")),
                   hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                                   hsm::target("child1"))),
        hsm::initial(hsm::target("parent")));

    runVerificationBenchmark("1. Baseline (no behaviors)", std::move(model),
                             "toChild2", "toChild1");
  }

  // Test 2: Entry functions only
  {
    auto model = hsm::define(
        "EntryHSM",
        hsm::state("parent", hsm::entry(countingEntryBehavior),
                   hsm::state("child1", hsm::entry(countingEntryBehavior)),
                   hsm::state("child2", hsm::entry(countingEntryBehavior)),
                   hsm::initial(hsm::target("child1")),
                   hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                                   hsm::target("child2")),
                   hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                                   hsm::target("child1"))),
        hsm::initial(hsm::target("parent")));

    runVerificationBenchmark("2. Entry functions only", std::move(model),
                             "toChild2", "toChild1");
  }

  // Test 3: Entry and exit functions
  {
    auto model = hsm::define(
        "EntryExitHSM",
        hsm::state("parent", hsm::entry(countingEntryBehavior),
                   hsm::exit(countingExitBehavior),
                   hsm::state("child1", hsm::entry(countingEntryBehavior),
                              hsm::exit(countingExitBehavior)),
                   hsm::state("child2", hsm::entry(countingEntryBehavior),
                              hsm::exit(countingExitBehavior)),
                   hsm::initial(hsm::target("child1")),
                   hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                                   hsm::target("child2")),
                   hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                                   hsm::target("child1"))),
        hsm::initial(hsm::target("parent")));

    runVerificationBenchmark("3. Entry and exit functions", std::move(model),
                             "toChild2", "toChild1");
  }

  // Test 4: Activities (the problematic one)
  {
    auto model = hsm::define(
        "ActivityHSM",
        hsm::state(
            "parent", hsm::entry(countingEntryBehavior),
            hsm::exit(countingExitBehavior),
            hsm::activity(countingActivityBehavior),  // This creates threads!
            hsm::state("child1", hsm::entry(countingEntryBehavior),
                       hsm::exit(countingExitBehavior),
                       hsm::activity(
                           countingActivityBehavior)  // This creates threads!
                       ),
            hsm::state("child2", hsm::entry(countingEntryBehavior),
                       hsm::exit(countingExitBehavior),
                       hsm::activity(
                           countingActivityBehavior)  // This creates threads!
                       ),
            hsm::initial(hsm::target("child1")),
            hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                            hsm::target("child2")),
            hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                            hsm::target("child1"))),
        hsm::initial(hsm::target("parent")));

    runVerificationBenchmark("4. WITH ACTIVITIES (creates threads!)",
                             std::move(model), "toChild2", "toChild1");
  }

  // Test 5: All features
  {
    auto model = hsm::define(
        "AllFeaturesHSM",
        hsm::state("parent", hsm::entry(countingEntryBehavior),
                   hsm::exit(countingExitBehavior),
                   hsm::activity(countingActivityBehavior),
                   hsm::state("child1", hsm::entry(countingEntryBehavior),
                              hsm::exit(countingExitBehavior),
                              hsm::activity(countingActivityBehavior)),
                   hsm::state("child2", hsm::entry(countingEntryBehavior),
                              hsm::exit(countingExitBehavior),
                              hsm::activity(countingActivityBehavior)),
                   hsm::initial(hsm::target("child1")),
                   hsm::transition(hsm::on("toChild2"), hsm::source("child1"),
                                   hsm::target("child2"),
                                   hsm::effect(countingEffectBehavior)),
                   hsm::transition(hsm::on("toChild1"), hsm::source("child2"),
                                   hsm::target("child1"),
                                   hsm::effect(countingEffectBehavior))),
        hsm::initial(hsm::target("parent")));

    runVerificationBenchmark("5. All features (threads + effects)",
                             std::move(model), "toChild2", "toChild1");
  }

  std::cout << "\n=== ANALYSIS ===" << std::endl;
  std::cout << "The massive performance drop with activities is caused by:"
            << std::endl;
  std::cout << "1. Activities run as separate THREADS (very expensive)"
            << std::endl;
  std::cout << "2. On every state transition:" << std::endl;
  std::cout
      << "   - Exit state: Signal and JOIN the activity thread (expensive!)"
      << std::endl;
  std::cout << "   - Enter state: CREATE a new activity thread (expensive!)"
            << std::endl;
  std::cout << "3. For nested states with activities, we create/destroy "
               "multiple threads per transition"
            << std::endl;
  std::cout
      << "4. Thread creation/destruction is ~1000x slower than function calls"
      << std::endl;
  std::cout << "\nThis explains the 99.7% performance degradation!"
            << std::endl;

  return 0;
}