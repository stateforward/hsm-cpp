#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "cthsm/cthsm.hpp"

using namespace cthsm;

TEST_CASE("Deferral - Basic") {
    constexpr auto model = define("machine",
        initial(target("idle")),
        state("idle",
            defer("event_A"),
            transition(on("event_B"), target("processing"))
        ),
        state("processing",
            transition(on("event_A"), target("done"))
        ),
        state("done")
    );
    
    compile<model, Instance> sm;
    Instance inst;
    sm.start(inst);
    
    // Dispatch A in idle -> deferred
    sm.dispatch(inst, "event_A");
    CHECK(sm.state() == "/machine/idle");
    
    // Dispatch B -> switch to processing, then A re-dispatched -> switch to done
    sm.dispatch(inst, "event_B");
    CHECK(sm.state() == "/machine/done");
}

TEST_CASE("Deferral - Hierarchy") {
    // Deferral should work inherited?
    // SCXML: <state id="p"><onentry>...</onentry><transition event="e" .../></state>
    // If child doesn't handle e, parent handles it.
    // If parent defers e, child inherits deferral?
    // cthsm implementation checks parent deferral list.
    // My `is_deferred` implementation iterates hierarchy.
    
    constexpr auto model = define("machine",
        initial(target("p/c")),
        state("p",
            defer("event_A"),
            state("c",
                transition(on("event_B"), target("/machine/other"))
            )
        ),
        state("other",
            transition(on("event_A"), target("done"))
        ),
        state("done")
    );
    
    compile<model, Instance> sm;
    Instance inst;
    sm.start(inst);
    
    sm.dispatch(inst, "event_A"); // Should be deferred by parent p
    CHECK(sm.state() == "/machine/p/c");
    
    sm.dispatch(inst, "event_B"); // Transition to other
    // A re-dispatched -> done
    CHECK(sm.state() == "/machine/done");
}
