#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "cthsm/cthsm.hpp"

using namespace cthsm;

TEST_CASE("Configurable Defer Queue Size") {
  constexpr auto model = define(
      "DeferLimitMachine",
      initial(target("Idle")),
      state("Idle",
            defer("E1", "E2", "E3"),
            transition(on("NEXT"), target("Process"))),
      state("Process",
            transition(on("E1"), target("Process")),
            transition(on("E2"), target("Process")),
            transition(on("E3"), target("Process")))
  );

  // Compile with a tiny queue size of 2
  // cthsm::compile<Model, Instance, TaskProvider, Clock, MaxDeferred>
  using MachineType = compile<model, Instance, SequentialTaskProvider, Clock, 2>;
  
  MachineType sm;
  Instance inst;
  sm.start(inst);

  CHECK(sm.state() == "/DeferLimitMachine/Idle");

  // Queue E1 -> OK (count=1)
  sm.dispatch(inst, cthsm::EventBase{"E1"});
  
  // Queue E2 -> OK (count=2)
  sm.dispatch(inst, cthsm::EventBase{"E2"});
  
  // Queue E3 -> Overflow! Should be dropped (count=2)
  sm.dispatch(inst, cthsm::EventBase{"E3"});

  // Transition to Process to drain queue
  sm.dispatch(inst, cthsm::EventBase{"NEXT"});
  CHECK(sm.state() == "/DeferLimitMachine/Process");
  
  // We can't easily check internal queue state, but we can infer it.
  // E1 and E2 should be re-dispatched. E3 was dropped.
  // Since Process self-transitions on E1/E2, we can't detect much unless we add side effects.
  // Let's rebuild with side effects.
}

struct CounterInstance : public Instance {
    int e1_count = 0;
    int e2_count = 0;
    int e3_count = 0;
};

void countE1(Context&, CounterInstance& i, const EventBase&) { i.e1_count++; }
void countE2(Context&, CounterInstance& i, const EventBase&) { i.e2_count++; }
void countE3(Context&, CounterInstance& i, const EventBase&) { i.e3_count++; }

TEST_CASE("Configurable Defer Queue Size - Functional") {
  constexpr auto model = define(
      "DeferLimitFunc",
      initial(target("Idle")),
      state("Idle",
            defer("E1", "E2", "E3"),
            transition(on("NEXT"), target("Process"))),
      state("Process",
            transition(on("E1"), target("Process"), effect(countE1)),
            transition(on("E2"), target("Process"), effect(countE2)),
            transition(on("E3"), target("Process"), effect(countE3)))
  );

  // MaxDeferred = 2
  using MachineType = compile<model, CounterInstance, SequentialTaskProvider, Clock, 2>;
  
  MachineType sm;
  CounterInstance inst;
  sm.start(inst);

  // Defer 3 events. Queue size is 2.
  // 1st -> Queued
  sm.dispatch(inst, cthsm::EventBase{"E1"});
  // 2nd -> Queued
  sm.dispatch(inst, cthsm::EventBase{"E2"});
  // 3rd -> Dropped
  sm.dispatch(inst, cthsm::EventBase{"E3"});

  // Transition -> Process queue
  sm.dispatch(inst, cthsm::EventBase{"NEXT"});
  
  // Verify counts
  CHECK(inst.e1_count == 1);
  CHECK(inst.e2_count == 1);
  CHECK(inst.e3_count == 0); // Dropped!
}

TEST_CASE("Configurable Defer Queue Size - Large") {
    // Test that we can increase the size
    constexpr auto model = define("LargeQueue",
        initial(target("Idle")),
        state("Idle", defer("E"), transition(on("GO"), target("Done"))),
        state("Done", transition(on("E"), target("Done")))
    );
    
    // MaxDeferred = 100
    using MachineType = compile<model, Instance, SequentialTaskProvider, Clock, 100>;
    MachineType sm;
    Instance inst;
    sm.start(inst);
    
    // We won't actually fill 100, but just ensure it compiles and runs
    for(int i=0; i<50; ++i) {
        sm.dispatch(inst, cthsm::EventBase{"E"});
    }
    
    sm.dispatch(inst, cthsm::EventBase{"GO"});
    CHECK(sm.state() == "/LargeQueue/Done");
}
