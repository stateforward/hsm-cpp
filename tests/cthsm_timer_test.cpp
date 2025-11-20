#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>
#include <chrono>
#include <deque>
#include <functional>

#include "cthsm/cthsm.hpp"

using namespace cthsm;

// Simplified Mock TaskProvider that matches cthsm template expectations (value semantics)
struct MockTaskProvider {
  struct TaskHandle {
    std::shared_ptr<bool> active; // Shared state to track liveness if needed
    
    void join() {} 
    bool joinable() const { return false; }
  };

  // Capture task function
  std::function<void()> last_task;
  std::vector<std::function<void()>> tasks;
  std::vector<std::chrono::milliseconds> sleeps;

  TaskHandle create_task(
      std::function<void()> task_function, const std::string& /*task_name*/ = "",
      size_t /*stack_size*/ = 0, int /*priority*/ = 0) {
    tasks.push_back(task_function);
    return TaskHandle{std::make_shared<bool>(true)};
  }

  void sleep_for(std::chrono::milliseconds duration) {
    sleeps.push_back(duration);
  }
};

// Global hooks to inspect provider state from tests
static std::vector<std::function<void()>> pending_tasks;
static std::vector<std::chrono::milliseconds> sleep_calls;

struct TestTaskProvider {
  struct TaskHandle {
      void join() {}
      bool joinable() const { return false; }
  };

  TaskHandle create_task(
      std::function<void()> task_function, const std::string&,
      size_t, int) {
    pending_tasks.push_back(std::move(task_function));
    return TaskHandle{};
  }

  void sleep_for(std::chrono::milliseconds duration) {
    sleep_calls.push_back(duration);
  }
};

struct TimerInstance : public Instance {
};

auto timer_100ms(Instance&) { return std::chrono::milliseconds(100); }
auto timer_200ms(Instance&) { return std::chrono::milliseconds(200); }

TEST_CASE("Timers - After") {
  pending_tasks.clear();
  sleep_calls.clear();

  constexpr auto model =
      define("machine", initial(target("idle")),
             state("idle", transition(after(timer_100ms), target("timeout"))),
             state("timeout"));

  compile<model, TimerInstance, TestTaskProvider> sm;
  TimerInstance inst;
  
  sm.start(inst);

  // Start spawns "init" transitions.
  // "idle" entry spawns timer task.
  
  REQUIRE(pending_tasks.size() == 1);
  
  // Run the timer task
  auto task = pending_tasks.front();
  pending_tasks.clear();
  
  // Task should call sleep_for then dispatch
  task();
  
  REQUIRE(sleep_calls.size() == 1);
  CHECK(sleep_calls[0].count() == 100);
  
  // Task dispatched event, state should update
  CHECK(sm.state() == "/machine/timeout");
}

TEST_CASE("Timers - Cancel on External Transition") {
  pending_tasks.clear();
  sleep_calls.clear();

  constexpr auto model =
      define("machine", initial(target("idle")),
             state("idle", transition(after(timer_200ms), target("timeout")),
                   transition(on("stop"), target("stopped"))),
             state("timeout"), state("stopped"));

  compile<model, TimerInstance, TestTaskProvider> sm;
  TimerInstance inst;
  sm.start(inst);

  REQUIRE(pending_tasks.size() == 1);
  
  // Don't run task yet. Dispatch "stop".
  sm.dispatch(inst, std::string_view("stop"));
  CHECK(sm.state() == "/machine/stopped");
  
  // Now if we run the timer task (simulating race or lingering thread), it should check context.
  auto task = pending_tasks.front();
  task(); // Should sleep (maybe) then check context and NOT dispatch
  
  // sleep_for is called inside the task.
  // But if task was cancelled?
  // cthsm sets context signal on exit.
  // Our TestTaskProvider doesn't check signal during sleep_for (it's mocked).
  // But timer_thunk checks c.is_set() after sleep.
  
  CHECK(sleep_calls.size() == 1); // It slept
  CHECK(sm.state() == "/machine/stopped"); // Still stopped, didn't go to timeout
}
