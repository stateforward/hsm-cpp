#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "hsm.hpp"

// Test instance for timer transitions
class TimerTestInstance : public hsm::Instance {
 public:
  std::vector<std::string> execution_log;
  std::atomic<int> timer_count{0};
  std::atomic<bool> timer_fired{false};

  ~TimerTestInstance() {
    if (__hsm) {
      hsm::stop(*this).wait();
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }

  void log(const std::string& message) { execution_log.push_back(message); }

  void clear() {
    execution_log.clear();
    timer_count = 0;
    timer_fired = false;
  }
};

// Timer duration functions
std::chrono::milliseconds short_duration(hsm::Context& /*ctx*/,
                                         TimerTestInstance& /*inst*/,
                                         hsm::Event& /*event*/) {
  return std::chrono::milliseconds(50);
}

std::chrono::milliseconds medium_duration(hsm::Context& /*ctx*/,
                                          TimerTestInstance& /*inst*/,
                                          hsm::Event& /*event*/) {
  return std::chrono::milliseconds(100);
}

std::chrono::milliseconds zero_duration(hsm::Context& /*ctx*/,
                                        TimerTestInstance& /*inst*/,
                                        hsm::Event& /*event*/) {
  return std::chrono::milliseconds(0);
}

// Action functions
void log_entry(const std::string& name, hsm::Context& /*ctx*/,
               hsm::Instance& inst, hsm::Event& /*event*/) {
  auto& test_inst = static_cast<TimerTestInstance&>(inst);
  test_inst.log("entry_" + name);
}

void log_exit(const std::string& name, hsm::Context& /*ctx*/,
              hsm::Instance& inst, hsm::Event& /*event*/) {
  auto& test_inst = static_cast<TimerTestInstance&>(inst);
  test_inst.log("exit_" + name);
}

void timer_effect(hsm::Context& /*ctx*/, hsm::Instance& inst,
                  hsm::Event& /*event*/) {
  auto& test_inst = static_cast<TimerTestInstance&>(inst);
  test_inst.log("timer_effect");
  test_inst.timer_fired = true;
}

void count_timer(hsm::Context& /*ctx*/, hsm::Instance& inst,
                 hsm::Event& /*event*/) {
  auto& test_inst = static_cast<TimerTestInstance&>(inst);
  test_inst.timer_count++;
  test_inst.log("timer_" + std::to_string(test_inst.timer_count.load()));
}

TEST_CASE("Timer Transitions - After Functionality") {
  SUBCASE("Basic After Timer") {
    auto model = hsm::define(
        "AfterTimer", hsm::initial(hsm::target("waiting")),
        hsm::state("waiting",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("waiting", ctx, inst, event);
                   }),
                   hsm::transition(
                       hsm::after<std::chrono::milliseconds, TimerTestInstance>(
                           short_duration),
                       hsm::target("../done"), hsm::effect(timer_effect))),
        hsm::state("done", hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                         hsm::Event& event) {
                     log_entry("done", ctx, inst, event);
                   })));

    TimerTestInstance instance;
    hsm::start(instance, model);

    if (std::getenv("HSM_DEBUG_TIMERS")) {
      std::cout << "[DEBUG] Test instance address: " << &instance << std::endl;
    }

    CHECK(instance.state() == "/AfterTimer/waiting");
    CHECK_FALSE(instance.timer_fired.load());

    // Wait for timer to fire
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    CHECK(instance.timer_fired.load());
    CHECK(instance.state() == "/AfterTimer/done");

    // Check execution order
    REQUIRE(instance.execution_log.size() >= 3);
    CHECK(instance.execution_log[0] == "entry_waiting");
    CHECK(instance.execution_log[1] == "timer_effect");
    CHECK(instance.execution_log[2] == "entry_done");
  }

  SUBCASE("Zero Duration After Timer") {
    auto model = hsm::define(
        "ZeroAfterTimer", hsm::initial(hsm::target("start")),
        hsm::state("start",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("start", ctx, inst, event);
                   }),
                   hsm::transition(
                       hsm::after<std::chrono::milliseconds, TimerTestInstance>(
                           zero_duration),
                       hsm::target("../immediate"))),
        hsm::state("immediate",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("immediate", ctx, inst, event);
                   })));

    TimerTestInstance instance;
    hsm::start(instance, model);

    // Zero duration timer should not fire
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(instance.state() == "/ZeroAfterTimer/start");
  }

  SUBCASE("After Timer with State Exit Cancellation") {
    auto model = hsm::define(
        "AfterCancellation", hsm::initial(hsm::target("waiting")),
        hsm::state(
            "waiting",
            hsm::entry(
                [](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                  log_entry("waiting", ctx, inst, event);
                }),
            hsm::exit(
                [](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                  log_exit("waiting", ctx, inst, event);
                }),
            hsm::transition(hsm::on("CANCEL"), hsm::target("../cancelled")),
            hsm::transition(
                hsm::after<std::chrono::milliseconds, TimerTestInstance>(
                    medium_duration),
                hsm::target("../timeout"), hsm::effect(timer_effect))),
        hsm::state("cancelled",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("cancelled", ctx, inst, event);
                   })),
        hsm::state("timeout",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("timeout", ctx, inst, event);
                   })));

    TimerTestInstance instance;
    hsm::start(instance, model);

    CHECK(instance.state() == "/AfterCancellation/waiting");

    // Cancel before timer fires
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    instance.dispatch(hsm::Event("CANCEL")).wait();

    CHECK(instance.state() == "/AfterCancellation/cancelled");
    CHECK_FALSE(instance.timer_fired.load());

    // Wait longer to ensure timer doesn't fire after cancellation
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CHECK_FALSE(instance.timer_fired.load());
    CHECK(instance.state() == "/AfterCancellation/cancelled");
  }
}

TEST_CASE("Timer Transitions - Every Functionality") {
  SUBCASE("Basic Every Timer") {
    auto model = hsm::define(
        "EveryTimer", hsm::initial(hsm::target("repeating")),
        hsm::state("repeating",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("repeating", ctx, inst, event);
                   }),
                   hsm::transition(hsm::on("STOP"), hsm::target("../stopped")),
                   hsm::transition(
                       hsm::every<std::chrono::milliseconds, TimerTestInstance>(
                           short_duration),
                       hsm::effect(count_timer))),
        hsm::state("stopped",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("stopped", ctx, inst, event);
                   })));

    TimerTestInstance instance;
    hsm::start(instance, model);

    CHECK(instance.state() == "/EveryTimer/repeating");
    CHECK(instance.timer_count.load() == 0);

    // Wait for multiple timer fires
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int count_after_wait = instance.timer_count.load();
    CHECK(count_after_wait >= 2);  // Should have fired at least twice
    CHECK(instance.state() == "/EveryTimer/repeating");

    // Stop the timer
    instance.dispatch(hsm::Event("STOP")).wait();
    CHECK(instance.state() == "/EveryTimer/stopped");

    // Wait and verify timer stops firing
    int count_before_final_wait = instance.timer_count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(instance.timer_count.load() == count_before_final_wait);
  }

  SUBCASE("Every Timer with Zero Duration") {
    auto model = hsm::define(
        "ZeroEveryTimer", hsm::initial(hsm::target("start")),
        hsm::state("start",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("start", ctx, inst, event);
                   }),
                   hsm::transition(
                       hsm::every<std::chrono::milliseconds, TimerTestInstance>(
                           zero_duration),
                       hsm::effect(count_timer))));

    TimerTestInstance instance;
    hsm::start(instance, model);

    // Zero duration timer should not fire
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(instance.timer_count.load() == 0);
  }
}

TEST_CASE("Timer Transitions - Multiple Timers") {
  SUBCASE("Multiple After Timers in Same State") {
    auto model = hsm::define(
        "MultipleAfterTimers", hsm::initial(hsm::target("waiting")),
        hsm::state("waiting",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("waiting", ctx, inst, event);
                   }),
                   hsm::transition(
                       hsm::after<std::chrono::milliseconds, TimerTestInstance>(
                           short_duration),
                       hsm::target("../fast")),
                   hsm::transition(
                       hsm::after<std::chrono::milliseconds, TimerTestInstance>(
                           medium_duration),
                       hsm::target("../slow"))),
        hsm::state("fast", hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                         hsm::Event& event) {
                     log_entry("fast", ctx, inst, event);
                   })),
        hsm::state("slow", hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                         hsm::Event& event) {
                     log_entry("slow", ctx, inst, event);
                   })));

    TimerTestInstance instance;
    hsm::start(instance, model);

    CHECK(instance.state() == "/MultipleAfterTimers/waiting");

    // Wait for the first timer to fire, but fail if the slower timer wins
    bool fast_seen = false;
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    while (std::chrono::steady_clock::now() < deadline) {
      auto current = instance.state();
      if (current == "/MultipleAfterTimers/fast") {
        fast_seen = true;
        break;
      }
      if (current == "/MultipleAfterTimers/slow") {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // First timer should have fired before the slower one
    CHECK(fast_seen);

    // Check that we got the fast transition, not the slow one
    REQUIRE(instance.execution_log.size() >= 2);
    CHECK(instance.execution_log[0] == "entry_waiting");
    CHECK(instance.execution_log[1] == "entry_fast");
  }

  SUBCASE("Mixed After and Every Timers") {
    auto model = hsm::define(
        "MixedTimers", hsm::initial(hsm::target("active")),
        hsm::state("active",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("active", ctx, inst, event);
                   }),
                   hsm::transition(hsm::on("STOP"), hsm::target("../inactive")),
                   hsm::transition(
                       hsm::after<std::chrono::milliseconds, TimerTestInstance>(
                           medium_duration),
                       hsm::target("../timeout")),
                   hsm::transition(
                       hsm::every<std::chrono::milliseconds, TimerTestInstance>(
                           short_duration),
                       hsm::effect(count_timer))),
        hsm::state("timeout",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("timeout", ctx, inst, event);
                   })),
        hsm::state("inactive",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("inactive", ctx, inst, event);
                   })));

    TimerTestInstance instance;
    hsm::start(instance, model);

    CHECK(instance.state() == "/MixedTimers/active");

    // Wait for some every timer fires, but not enough for after timer
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    CHECK(instance.timer_count.load() >= 1);
    CHECK(instance.state() == "/MixedTimers/active");

    // Wait for after timer to fire
    bool timeout_reached = false;
    auto timeout_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < timeout_deadline) {
      if (instance.state() == "/MixedTimers/timeout") {
        timeout_reached = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(timeout_reached);
  }
}

TEST_CASE("Timer Transitions - Error Conditions") {
  SUBCASE("Timer in Final State") {
    // Final states should not have timers, but this tests the system's
    // robustness
    auto model = hsm::define(
        "TimerInFinal", hsm::initial(hsm::target("start")),
        hsm::state("start",
                   hsm::transition(hsm::on("FINISH"), hsm::target("../end"))),
        hsm::final("end"));

    TimerTestInstance instance;
    hsm::start(instance, model);

    CHECK(instance.state() == "/TimerInFinal/start");

    instance.dispatch(hsm::Event("FINISH")).wait();
    CHECK(instance.state() == "/TimerInFinal/end");

    // No timers should fire from final state
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(instance.state() == "/TimerInFinal/end");
  }

  SUBCASE("Rapid State Changes with Timers") {
    auto model = hsm::define(
        "RapidChanges", hsm::initial(hsm::target("state1")),
        hsm::state("state1",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("state1", ctx, inst, event);
                   }),
                   hsm::transition(hsm::on("NEXT"), hsm::target("../state2")),
                   hsm::transition(
                       hsm::after<std::chrono::milliseconds, TimerTestInstance>(
                           medium_duration),
                       hsm::target("../timeout1"))),
        hsm::state("state2",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("state2", ctx, inst, event);
                   }),
                   hsm::transition(hsm::on("NEXT"), hsm::target("../state3")),
                   hsm::transition(
                       hsm::after<std::chrono::milliseconds, TimerTestInstance>(
                           medium_duration),
                       hsm::target("../timeout2"))),
        hsm::state("state3",
                   hsm::entry([](hsm::Context& ctx, hsm::Instance& inst,
                                 hsm::Event& event) {
                     log_entry("state3", ctx, inst, event);
                   })),
        hsm::state("timeout1"), hsm::state("timeout2"));

    TimerTestInstance instance;
    hsm::start(instance, model);

    CHECK(instance.state() == "/RapidChanges/state1");

    // Rapidly change states before timers can fire
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    instance.dispatch(hsm::Event("NEXT")).wait();
    CHECK(instance.state() == "/RapidChanges/state2");

    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    instance.dispatch(hsm::Event("NEXT")).wait();
    CHECK(instance.state() == "/RapidChanges/state3");

    // Wait to ensure no timeout transitions fire
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    CHECK(instance.state() == "/RapidChanges/state3");
  }
}