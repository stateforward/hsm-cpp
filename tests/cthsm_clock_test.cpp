#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "cthsm/cthsm.hpp"

using namespace std::chrono_literals;

#include <iostream>

// --- Simulated Clock ---
struct SimulatedClock {
  using duration = std::chrono::milliseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<SimulatedClock>;
  static constexpr bool is_steady = true;

  static std::atomic<rep> current_time_ms;
  static std::mutex mutex;
  static std::condition_variable cv;

  static time_point now() noexcept {
    return time_point(duration(current_time_ms.load()));
  }

  static void advance(duration d) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      current_time_ms += d.count();
    }
    cv.notify_all();
  }

  static void reset() {
    current_time_ms = 0;
  }
};

std::atomic<SimulatedClock::rep> SimulatedClock::current_time_ms{0};
std::mutex SimulatedClock::mutex;
std::condition_variable SimulatedClock::cv;

// --- Simulated Task Provider ---
struct SimulatedTaskProvider {
  static std::atomic<int> sleepers;

  struct TaskHandle {
    std::thread t;
    void join() {
      if (t.joinable()) {
          if (t.get_id() == std::this_thread::get_id()) {
              t.detach(); // Self-join prevention
          } else {
              t.join();
          }
      }
    }
    bool joinable() const { return t.joinable(); }
  };

  template <typename F>
  TaskHandle create_task(F&& f, const char* /*name*/ = nullptr,
                         size_t /*stack*/ = 0, int /*prio*/ = 0) {
    return TaskHandle{std::thread(std::forward<F>(f))};
  }

  void sleep_for(std::chrono::milliseconds d, cthsm::Context* ctx = nullptr) {
    auto start = SimulatedClock::now();
    auto end = start + d;
    
    // std::cout << "Task sleeping for " << d.count() << "ms until " << end.time_since_epoch().count() << std::endl;

    sleepers++;
    
    std::unique_lock<std::mutex> lock(SimulatedClock::mutex);
    bool ready = SimulatedClock::cv.wait_for(lock, 10ms, [&]() {
        if (ctx && ctx->is_set()) return true; 
        return SimulatedClock::now() >= end;
    });
    
    while (!ready) {
        if (ctx && ctx->is_set()) { sleepers--; return; }
        ready = SimulatedClock::cv.wait_for(lock, 10ms, [&]() {
            if (ctx && ctx->is_set()) return true; 
            return SimulatedClock::now() >= end;
        });
    }
    sleepers--;
    // std::cout << "Task woke up at " << SimulatedClock::now().time_since_epoch().count() << std::endl;
  }
};

std::atomic<int> SimulatedTaskProvider::sleepers{0};

// --- Helper Behavior ---
struct TrackedInstance : cthsm::Instance {
  std::vector<std::string> events;
  std::mutex m;
  void add_event(std::string name) { 
      std::lock_guard<std::mutex> l(m);
      events.push_back(std::move(name)); 
  }
  bool has_event(const std::string& name) {
      std::lock_guard<std::mutex> l(m);
      if (events.empty()) return false;
      return events.back() == name;
  }
  bool events_empty() {
      std::lock_guard<std::mutex> l(m);
      return events.empty();
  }
};

void log_enter(cthsm::Context&, TrackedInstance& i, const cthsm::EventBase& /*e*/) {
  i.add_event("enter");
}

// --- Test Cases ---

TEST_CASE("Simulated Clock - After Transition") {
  SimulatedClock::reset();
  SimulatedTaskProvider::sleepers = 0;
  using namespace cthsm;

  constexpr auto model = define(
      "clock_test",
      initial(target("idle")),
      state("idle",
            transition(after([]{ return 100ms; }), target("done"))),
      state("done", entry(log_enter))
  );

  using SM = compile<model, TrackedInstance, SimulatedTaskProvider, SimulatedClock>;
  TrackedInstance inst;
  SM sm;
  sm.start(inst);

  CHECK(sm.state() == "/clock_test/idle");

  // Wait for timer task to start sleeping
  while (SimulatedTaskProvider::sleepers == 0) std::this_thread::sleep_for(1ms);

  // Advance time partially - should not transition yet
  SimulatedClock::advance(50ms);
  // We need a small real sleep to let threads process (SimulatedTaskProvider uses real threads)
  std::this_thread::sleep_for(10ms); 
  CHECK(sm.state() == "/clock_test/idle");
  CHECK(inst.events_empty());

  // Advance past the threshold
  SimulatedClock::advance(60ms); // Total 110ms
  
  // Wait for thread to wake up and process
  // In a real deterministic sim we'd have better synchronization, but for this integration test:
  int retries = 0;
  while (sm.state() != "/clock_test/done" && retries++ < 50) {
      std::this_thread::sleep_for(10ms);
  }

  CHECK(sm.state() == "/clock_test/done");
  CHECK(!inst.events_empty());
  CHECK(inst.has_event("enter"));
}

TEST_CASE("Simulated Clock - At Transition") {
  SimulatedClock::reset();
  SimulatedTaskProvider::sleepers = 0;
  using namespace cthsm;

  constexpr auto get_deadline = []() {
      return SimulatedClock::time_point(200ms);
  };

  constexpr auto model = define(
      "at_test",
      initial(target("idle")),
      state("idle",
            transition(at(get_deadline), target("done"))),
      state("done")
  );

  using SM = compile<model, TrackedInstance, SimulatedTaskProvider, SimulatedClock>;
  TrackedInstance inst;
  SM sm;
  sm.start(inst);

  CHECK(sm.state() == "/at_test/idle");

  // Wait for task
  while (SimulatedTaskProvider::sleepers == 0) std::this_thread::sleep_for(1ms);

  // Current time 0, deadline 200ms.
  SimulatedClock::advance(100ms);
  std::this_thread::sleep_for(10ms);
  CHECK(sm.state() == "/at_test/idle");

  SimulatedClock::advance(150ms); // Total 250ms
  
  int retries = 0;
  while (sm.state() != "/at_test/done" && retries++ < 20) {
      std::this_thread::sleep_for(10ms);
  }

  CHECK(sm.state() == "/at_test/done");
}

/*
TEST_CASE("Simulated Clock - Every Transition") {
    SimulatedClock::reset();
    SimulatedTaskProvider::sleepers = 0;
    using namespace cthsm;
  
    struct Counter : TrackedInstance {
        std::atomic<int> ticks{0};
    };

    // Increment counter behavior
    auto tick_action = [](Context&, Counter& c, const EventBase&) {
        c.ticks++;
    };
  
    constexpr auto model = define(
        "every_test",
        initial(target("counting")),
        state("counting",
              transition(every([]{ return 50ms; }), effect(tick_action)))
    );
  
    using SM = compile<model, Counter, SimulatedTaskProvider, SimulatedClock>;
    Counter inst;
    SM sm;
    sm.start(inst);
  
    CHECK(inst.ticks == 0);
  
    // Wait for task
    while (SimulatedTaskProvider::sleepers == 0) std::this_thread::sleep_for(1ms);

    // Advance 50ms -> 1 tick
    SimulatedClock::advance(50ms);
    
    // Wait for tick
    int retries = 0;
    while (inst.ticks < 1 && retries++ < 20) std::this_thread::sleep_for(10ms);
    CHECK(inst.ticks == 1);
    
    // Advance another 50ms -> 2nd tick
    SimulatedClock::advance(50ms);
    
    retries = 0;
    while (inst.ticks < 2 && retries++ < 20) std::this_thread::sleep_for(10ms);
    CHECK(inst.ticks == 2);

    // Advance 100ms -> should get 2 more ticks ideally, but "every" loop is:
    // sleep -> dispatch -> loop. 
    // If we advance a lot at once, the loop wakes up once, dispatches, sleeps again.
    // It catches up one by one.
    
    SimulatedClock::advance(50ms);
    retries = 0;
    while (inst.ticks < 3 && retries++ < 20) std::this_thread::sleep_for(10ms);
    CHECK(inst.ticks == 3);
}
*/
