#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <functional>
#include <memory>
#include <queue>

#include "cthsm/cthsm.hpp"

using namespace cthsm;

// --- Test Utilities ---

struct SharedState {
  std::queue<std::function<void()>> tasks;
};

struct WrapperProvider {
  std::shared_ptr<SharedState> state;

  struct TaskHandle {
    void join() {}
    bool joinable() const { return false; }
  };

  WrapperProvider(std::shared_ptr<SharedState> s) : state(std::move(s)) {}
  // Default constructor for compilation when not explicitly initialized
  WrapperProvider() : state(std::make_shared<SharedState>()) {}

  template <typename F>
  TaskHandle create_task(F&& f, const char* = nullptr, size_t = 0, int = 0) {
    if (state) {
      state->tasks.push(std::forward<F>(f));
    }
    return TaskHandle{};
  }
  void sleep_for(std::chrono::milliseconds, Context*) {}
};

struct TestInstance : public Instance {
  bool flag = false;
  int counter = 0;
};

// --- Tests ---

TEST_CASE("Completion - Simple Immediate") {
  constexpr auto model =
      define("SimpleMachine", initial(target("start")),
             state("start", transition(target("end"))),  // Immediate completion
             state("end"));

  compile<model> sm;
  Instance inst;
  sm.start(inst);

  CHECK(sm.state() == "/SimpleMachine/end");
}

TEST_CASE("Completion - Waiting for Activity") {
  static int activity_run_count = 0;
  activity_run_count = 0;

  struct MyActivity {
    void operator()(TestInstance&) const { activity_run_count++; }
  };

  constexpr auto model = define(
      "ActivityMachine", initial(target("working")),
      state("working", activity(MyActivity{}), transition(target("done"))),
      state("done"));

  auto shared = std::make_shared<SharedState>();
  WrapperProvider provider{shared};
  compile<model, TestInstance, WrapperProvider> sm(provider);
  TestInstance inst;

  sm.start(inst);

  // Should wait for activity
  CHECK(sm.state() == "/ActivityMachine/working");

  // Run activity
  REQUIRE(!shared->tasks.empty());
  auto f = shared->tasks.front();
  shared->tasks.pop();
  f();

  // Should trigger completion
  CHECK(sm.state() == "/ActivityMachine/done");
  CHECK(activity_run_count == 1);
}

TEST_CASE("Completion - Guarded with Trigger") {
  struct GuardedInstance : public Instance {
    bool ready = false;
  };

  struct IsReady {
    bool operator()(GuardedInstance& i, const EventBase&) const {
      return i.ready;
    }
  };

  struct MakeReady {
    void operator()(GuardedInstance& i, const EventBase&) const {
      i.ready = true;
    }
  };

  constexpr auto model = define(
      "TriggerMachine", initial(target("wait")),
      state("wait",
            transition(
                on("KICK"), target("wait"),
                effect(MakeReady{})),  // Self-transition to trigger re-eval
            transition(guard(IsReady{}), target("finished"))),
      state("finished"));

  compile<model, GuardedInstance> sm;
  GuardedInstance inst;
  sm.start(inst);

  CHECK(sm.state() == "/TriggerMachine/wait");

  // Dispatch KICK.
  // 1. KICK matches transition 1.
  // 2. Effect runs (ready=true).
  // 3. Transition executes (target=wait, self-transition).
  // 4. enter_from_lca...
  // 5. resolve_completion called.
  // 6. guard IsReady() is now true.
  // 7. transition to finished.

  sm.dispatch(inst, EventBase{"KICK"});
  CHECK(sm.state() == "/TriggerMachine/finished");
}

TEST_CASE("Completion - Hierarchical") {
  constexpr auto model =
      define("HierMachine", initial(target("composite")),
             state("composite",
                   transition(target("final_dest")),  // Completion on composite
                   initial(target("step1")),
                   state("step1", transition(on("NEXT"), target("step2"))),
                   state("step2", transition(on("DONE"), target("sub_final"))),
                   final("sub_final")),
             state("final_dest"));

  compile<model> sm;
  Instance inst;
  sm.start(inst);

  CHECK(sm.state() == "/HierMachine/composite/step1");

  sm.dispatch(inst, EventBase{"NEXT"});
  CHECK(sm.state() == "/HierMachine/composite/step2");

  sm.dispatch(inst, EventBase{"DONE"});
  // step2 -> sub_final.
  // sub_final is Final.
  // resolve_completion sees current is Final.
  // Bubbles up to composite.
  // Checks completion transitions on composite.
  // Found transition to final_dest.
  CHECK(sm.state() == "/HierMachine/final_dest");
}

TEST_CASE("Completion - Multiple Choices (Priority)") {
  struct Ctx : public Instance {
    int val = 0;
  };

  struct ValIs1 {
    bool operator()(Ctx& c, const EventBase&) const { return c.val == 1; }
  };
  struct ValIs2 {
    bool operator()(Ctx& c, const EventBase&) const { return c.val == 2; }
  };

  constexpr auto model =
      define("ChoiceMachine", initial(target("decide")),
             state("decide", transition(guard(ValIs1{}), target("path1")),
                   transition(guard(ValIs2{}), target("path2")),
                   transition(target("default"))),
             state("path1"), state("path2"), state("default"));

  SUBCASE("Path 1") {
    compile<model, Ctx> sm;
    Ctx inst;
    inst.val = 1;
    sm.start(inst);
    CHECK(sm.state() == "/ChoiceMachine/path1");
  }

  SUBCASE("Path 2") {
    compile<model, Ctx> sm;
    Ctx inst;
    inst.val = 2;
    sm.start(inst);
    CHECK(sm.state() == "/ChoiceMachine/path2");
  }

  SUBCASE("Default Path") {
    compile<model, Ctx> sm;
    Ctx inst;
    inst.val = 99;
    sm.start(inst);
    CHECK(sm.state() == "/ChoiceMachine/default");
  }
}

TEST_CASE("Completion - Activity and Hierarchy Mixed") {
  // Composite state has activity.
  // Substates run.
  // Composite should not complete until BOTH substate reaches final AND
  // activity finishes.

  static int activity_done = 0;
  activity_done = 0;
  struct MyActivity {
    void operator()(TestInstance&) const { activity_done++; }
  };

  constexpr auto model =
      define("MixedMachine", initial(target("composite")),
             state("composite", activity(MyActivity{}),
                   transition(target("finished")), initial(target("sub1")),
                   state("sub1", transition(on("NEXT"), target("sub_final"))),
                   final("sub_final")),
             state("finished"));

  auto shared = std::make_shared<SharedState>();
  WrapperProvider provider{shared};
  compile<model, TestInstance, WrapperProvider> sm(provider);
  TestInstance inst;

  sm.start(inst);
  CHECK(sm.state() == "/MixedMachine/composite/sub1");

  // 1. Finish substate workflow first
  sm.dispatch(inst, EventBase{"NEXT"});
  // Now at sub_final. Composite is technically "complete" regarding regions.
  // BUT activity is still running.
  CHECK(sm.state() == "/MixedMachine/composite/sub_final");

    // 2. Finish activity
    REQUIRE(!shared->tasks.empty());
    auto f = shared->tasks.front();
    shared->tasks.pop();
    f(); 
    CHECK(activity_done == 1);

    // Now should complete
    CHECK(sm.state() == "/MixedMachine/finished");
}
