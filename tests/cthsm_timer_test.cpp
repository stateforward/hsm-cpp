#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>

#include "cthsm/cthsm.hpp"

using namespace cthsm;

struct TimerInstance : public Instance {
  struct Timer {
    std::size_t id;
    std::size_t duration;

    bool operator==(const Timer& other) const = default;
  };
  std::vector<Timer> active_timers;

  void schedule(std::size_t id, std::size_t duration) {
    // Replace if exists
    cancel_timer(id);
    active_timers.push_back({id, duration});
  }

  void cancel_timer(std::size_t id) {
    std::erase_if(active_timers, [&](const auto& t) { return t.id == id; });
  }

  bool has_timer(std::size_t id) const {
    for (const auto& t : active_timers)
      if (t.id == id) return true;
    return false;
  }
};

std::size_t timer_100ms(Instance&) { return 100; }
std::size_t timer_200ms(Instance&) { return 200; }

TEST_CASE("Timers - After") {
  constexpr auto model =
      define("machine", initial(target("idle")),
             state("idle", transition(after(timer_100ms), target("timeout"))),
             state("timeout"));

  compile<model, TimerInstance> sm;
  TimerInstance inst;
  sm.start(inst);

  // Check timer scheduled
  CHECK(inst.active_timers.size() == 1);
  CHECK(inst.active_timers[0].duration == 100);

  // Fire timer
  sm.handle_timer(inst, 0);  // Timer index 0 should be the after timer
  CHECK(sm.state() == "/machine/timeout");

  // Timer should be cancelled on exit (if single shot? implementation detail of
  // cthsm doesn't auto cancel unless 'every'?) 'after' is transition trigger.
  // Transition taken -> exit state. Exit state logic cancels timers of that
  // state. So timer should be cancelled.
  CHECK(inst.active_timers.empty());
}

TEST_CASE("Timers - Cancel on External Transition") {
  constexpr auto model =
      define("machine", initial(target("idle")),
             state("idle", transition(after(timer_200ms), target("timeout")),
                   transition(on("stop"), target("stopped"))),
             state("timeout"), state("stopped"));

  compile<model, TimerInstance> sm;
  TimerInstance inst;
  sm.start(inst);

  CHECK(inst.active_timers.size() == 1);

  sm.dispatch(inst, cthsm::AnyEvent{"stop"});
  CHECK(sm.state() == "/machine/stopped");

  // Timer should be cancelled
  CHECK(inst.active_timers.empty());
}

TEST_CASE("Timers - Every") {
  // Placeholder
  CHECK(true);
}
