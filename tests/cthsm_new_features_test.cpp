#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <deque>
#include <functional>
#include <vector>
#include <memory>
#include <string>

#include "cthsm/cthsm.hpp"

using namespace cthsm;

// --- Shared Test Utilities ---

// A TaskProvider that captures tasks for manual execution
struct ManualTaskProvider {
  struct TaskHandle {
    std::shared_ptr<bool> active;
    void join() {}
    bool joinable() const { return false; }
  };

  std::vector<std::function<void()>> pending_tasks;
  std::vector<std::chrono::milliseconds> sleep_calls;
  
  // For 'every' timer test cancellation
  static inline std::size_t max_iterations = 3;

  TaskHandle create_task(std::function<void()> task_function, const std::string&,
                         size_t, int) {
    pending_tasks.push_back(std::move(task_function));
    return TaskHandle{std::make_shared<bool>(true)};
  }

  void sleep_for(std::chrono::milliseconds duration, cthsm::Context* ctx = nullptr) {
    sleep_calls.push_back(duration);
    // For 'every' timer tests to terminate:
    if (ctx && sleep_calls.size() >= max_iterations) {
      ctx->set();
    }
  }

  void run_one() {
    if (pending_tasks.empty()) return;
    auto task = pending_tasks.front();
    pending_tasks.erase(pending_tasks.begin());
    task();
  }
  
  void clear() {
    pending_tasks.clear();
    sleep_calls.clear();
  }
};

// Global instance to access from tests if needed (though we pass it by value usually, 
// but cthsm takes provider by value, so we need to be careful if we want to inspect state.
// The previous tests used a global or static members. 
// Let's use a global for simplicity as seen in cthsm_timer_test.cpp
static ManualTaskProvider global_provider;

struct TestProvider {
  struct TaskHandle {
    void join() {}
    bool joinable() const { return false; }
  };

  TaskHandle create_task(std::function<void()> f, const std::string& n, size_t s, int p) {
    global_provider.create_task(std::move(f), n, s, p);
    return TaskHandle{};
  }

  void sleep_for(std::chrono::milliseconds d, cthsm::Context* ctx = nullptr) {
    global_provider.sleep_for(d, ctx);
  }
};


struct TestInstance : public Instance {
  int counter = 0;
};

// --- Test Cases ---

TEST_CASE("Deep History Restoration") {
  // P -> (S1 -> S2). P has deep history.
  // Initial path: P -> S1 -> S2.
  // Leave P.
  // Return to P via deep history -> should go to S2.
  
  constexpr auto model = define(
      "DeepHistoryMachine", 
      initial(target("P")),
      state("P",
            initial(target("S1")),
            state("S1", transition(on("NEXT"), target("S2"))),
            state("S2"),
            transition(on("LEAVE"), target("Outside"))
      ),
      state("Outside",
            transition(on("BACK_DEEP"), target(deep_history("/DeepHistoryMachine/P"))),
            transition(on("BACK_DEFAULT"), target("P")) // Should go to S1
      )
  );

  compile<model> sm;
  Instance inst;
  sm.start(inst);

  CHECK(sm.state() == "/DeepHistoryMachine/P/S1");
  
  sm.dispatch(inst, EventBase{"NEXT"});
  CHECK(sm.state() == "/DeepHistoryMachine/P/S2");
  
  sm.dispatch(inst, EventBase{"LEAVE"});
  CHECK(sm.state() == "/DeepHistoryMachine/Outside");
  
  // Test Deep History
  sm.dispatch(inst, EventBase{"BACK_DEEP"});
  CHECK(sm.state() == "/DeepHistoryMachine/P/S2");
  
  // Reset and test default entry for contrast
  sm.dispatch(inst, EventBase{"LEAVE"});
  CHECK(sm.state() == "/DeepHistoryMachine/Outside");
  
  sm.dispatch(inst, EventBase{"BACK_DEFAULT"});
  CHECK(sm.state() == "/DeepHistoryMachine/P/S1");
}

TEST_CASE("Shallow History Restoration") {
  // P -> (S1 -> S2). P has shallow history.
  // S1 has substates: S1a -> S1b.
  // Initial path: P -> S1 -> S1a -> S1b.
  // Leave P.
  // Return to P via shallow history.
  // Shallow history remembers S1 (direct child), but forgets S1b (grandchild).
  // Should enter S1, then follow S1's initial -> S1a.
  
  constexpr auto model = define(
      "ShallowHistoryMachine",
      initial(target("P")),
      state("P",
            initial(target("S1")),
            state("S1",
                  initial(target("S1a")),
                  state("S1a", transition(on("NEXT"), target("S1b"))),
                  state("S1b")
            ),
            state("S2"),
            transition(on("LEAVE"), target("Outside"))
      ),
      state("Outside",
            transition(on("BACK_SHALLOW"), target(shallow_history("/ShallowHistoryMachine/P"))),
            transition(on("BACK_DEEP"), target(deep_history("/ShallowHistoryMachine/P")))
      )
  );

  compile<model> sm;
  Instance inst;
  sm.start(inst);

  CHECK(sm.state() == "/ShallowHistoryMachine/P/S1/S1a");
  
  sm.dispatch(inst, EventBase{"NEXT"});
  CHECK(sm.state() == "/ShallowHistoryMachine/P/S1/S1b");
  
  sm.dispatch(inst, EventBase{"LEAVE"});
  CHECK(sm.state() == "/ShallowHistoryMachine/Outside");
  
  // Test Shallow History
  // Should restore S1, then init to S1a
  sm.dispatch(inst, EventBase{"BACK_SHALLOW"});
  CHECK(sm.state() == "/ShallowHistoryMachine/P/S1/S1a");
  
  // Verify Deep History behaviour for contrast (should go to S1b)
  // First allow it to go to S1b again
  sm.dispatch(inst, EventBase{"NEXT"});
  CHECK(sm.state() == "/ShallowHistoryMachine/P/S1/S1b");
  
  sm.dispatch(inst, EventBase{"LEAVE"});
  CHECK(sm.state() == "/ShallowHistoryMachine/Outside");
  
  sm.dispatch(inst, EventBase{"BACK_DEEP"});
  CHECK(sm.state() == "/ShallowHistoryMachine/P/S1/S1b");
}

TEST_CASE("Hierarchical Completion") {
  // Composite state with a final substate.
  // When final substate is reached, composite should trigger completion transition.
  
  constexpr auto model = define(
      "HierCompMachine",
      initial(target("Composite")),
      state("Composite",
            initial(target("Step1")),
            state("Step1", transition(on("NEXT"), target("SubFinal"))),
            final("SubFinal"),
            // Completion transition on Composite
            transition(target("Done"))
      ),
      state("Done")
  );

  compile<model> sm;
  Instance inst;
  sm.start(inst);

  CHECK(sm.state() == "/HierCompMachine/Composite/Step1");
  
  sm.dispatch(inst, EventBase{"NEXT"});
  // Step1 -> SubFinal.
  // SubFinal is final -> triggers completion on Composite -> Done.
  CHECK(sm.state() == "/HierCompMachine/Done");
}

TEST_CASE("Completion with Asynchronous Activity") {
  // State with activity.
  // Completion transition should NOT fire until activity completes.
  
  global_provider.clear();
  
  struct MyActivity {
    void operator()(TestInstance& i) const { i.counter++; }
  };

  constexpr auto model = define(
      "AsyncCompMachine",
      initial(target("Working")),
      state("Working",
            activity(MyActivity{}),
            transition(target("Done"))
      ),
      state("Done")
  );

  compile<model, TestInstance, TestProvider> sm;
  TestInstance inst;
  sm.start(inst);

  // Should be in Working, waiting for activity
  CHECK(sm.state() == "/AsyncCompMachine/Working");
  CHECK(inst.counter == 0);
  
  REQUIRE(global_provider.pending_tasks.size() == 1);
  
  // Execute activity
  global_provider.run_one();
  
  // Activity finished -> completion transition fires
  CHECK(inst.counter == 1);
  CHECK(sm.state() == "/AsyncCompMachine/Done");
}

TEST_CASE("'every' Timer Dispatch") {
  // State with 'every' timer.
  // Should fire multiple times until state exited.
  
  global_provider.clear();
  ManualTaskProvider::max_iterations = 3; // Allow 3 sleeps (fires event 2 times, 3rd sleep cancels)
  
  struct TimerInst : public Instance {
    int ticks = 0;
  };
  
  auto on_tick = [](Context&, TimerInst& i, const EventBase&) {
    i.ticks++;
  };
  
  constexpr auto model = define(
      "EveryMachine",
      initial(target("Counting")),
      state("Counting",
            transition(every([](TimerInst&){ return std::chrono::milliseconds(10); }),
                       effect(on_tick))
      )
  );

  compile<model, TimerInst, TestProvider> sm;
  TimerInst inst;
  sm.start(inst);

  CHECK(sm.state() == "/EveryMachine/Counting");
  
  REQUIRE(global_provider.pending_tasks.size() == 1);
  
  // Run the timer loop
  global_provider.run_one();
  
  // With max_iterations = 3:
  // 1. sleep(10) -> dispatch tick (ticks=1)
  // 2. sleep(10) -> dispatch tick (ticks=2)
  // 3. sleep(10) -> ctx set -> break
  // Total ticks = 2
  
  CHECK(global_provider.sleep_calls.size() == 3);
  CHECK(inst.ticks == 2);
}
