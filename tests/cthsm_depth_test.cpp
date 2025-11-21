#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "cthsm/cthsm.hpp"

using namespace cthsm;

// A test to verify that hierarchy depth > 16 is supported.
// We will construct a model with 20 nested states.

// Helper macros to generate nested states
// We'll build: Root -> L1 -> L2 ... -> L20

TEST_CASE("Deep Hierarchy Support (>16 levels)") {
  constexpr auto model = define(
      "DeepMachine",
      initial(target("L1")),
      state("L1",
        state("L2",
          state("L3",
            state("L4",
              state("L5",
                state("L6",
                  state("L7",
                    state("L8",
                      state("L9",
                        state("L10",
                          state("L11",
                            state("L12",
                              state("L13",
                                state("L14",
                                  state("L15",
                                    state("L16",
                                      state("L17",
                                        state("L18",
                                          state("L19",
                                            state("L20",
                                                transition(on("RESET"), target("/DeepMachine/L1"))
                                            )
                                          )
                                        )
                                      )
                                    )
                                  )
                                )
                              )
                            )
                          )
                        )
                      )
                    )
                  )
                )
              )
            )
          )
        )
      )
  );

  // If the hardcoded limit of 16 was still in place, this might compile (if array bounds check is runtime)
  // or fail compile (if static assert) or fail runtime logic (LCA truncation).
  // The LCA logic in tables.hpp used to break at 16, meaning it would fail to find common ancestor correctly
  // for very deep transitions.

  compile<model> sm;
  Instance inst;
  sm.start(inst);

  // Should start at L1, but initial is not deep.
  // Wait, L1 has no initial. So it stays at L1?
  // The model says initial(target("L1")).
  // So it enters L1. L1 has substates but no initial.
  // So active state is L1.
  CHECK(sm.state() == "/DeepMachine/L1");
  
  // Let's manually drill down? No, we need a transition to L20 or initial chain.
  // Let's add initial chain to make it go deep automatically?
  // Or just dispatch to target L20?
  // Let's refine the model to auto-drill.
}

TEST_CASE("Deep Hierarchy Auto-Drill") {
    constexpr auto model = define(
      "DrillMachine",
      initial(target("L1")),
      state("L1", initial(target("L2")),
        state("L2", initial(target("L3")),
          state("L3", initial(target("L4")),
            state("L4", initial(target("L5")),
              state("L5", initial(target("L6")),
                state("L6", initial(target("L7")),
                  state("L7", initial(target("L8")),
                    state("L8", initial(target("L9")),
                      state("L9", initial(target("L10")),
                        state("L10", initial(target("L11")),
                          state("L11", initial(target("L12")),
                            state("L12", initial(target("L13")),
                              state("L13", initial(target("L14")),
                                state("L14", initial(target("L15")),
                                  state("L15", initial(target("L16")),
                                    state("L16", initial(target("L17")),
                                      state("L17", initial(target("L18")),
                                        state("L18", initial(target("L19")),
                                          state("L19", initial(target("L20")),
                                            state("L20",
                                                transition(on("POP"), target("/DrillMachine/L1"))
                                            )
                                          )
                                        )
                                      )
                                    )
                                  )
                                )
                              )
                            )
                          )
                        )
                      )
                    )
                  )
                )
              )
            )
          )
        )
      )
  );

  compile<model> sm;
  Instance inst;
  sm.start(inst);

  // It should have drilled all the way down to L20
  CHECK(sm.state() == "/DrillMachine/L1/L2/L3/L4/L5/L6/L7/L8/L9/L10/L11/L12/L13/L14/L15/L16/L17/L18/L19/L20");
  
  // Test LCA calculation for a long jump up
  sm.dispatch(inst, cthsm::EventBase{"POP"});
  
  // Should be back at L1 -> L2 ... -> L20 (because of initial transitions re-entering)
  // If POP targets L1, it exits L20...L1, enters L1, then initial chain L2...L20.
  // So state should be L20 again.
  CHECK(sm.state() == "/DrillMachine/L1/L2/L3/L4/L5/L6/L7/L8/L9/L10/L11/L12/L13/L14/L15/L16/L17/L18/L19/L20");
}
