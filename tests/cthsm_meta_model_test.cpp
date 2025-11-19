#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "cthsm/cthsm.hpp"

using namespace cthsm;

TEST_CASE("Normalization - State Hierarchy") {
    constexpr auto model = define("root",
        state("s1",
            state("s1_1"),
            state("s1_2")
        ),
        state("s2")
    );
    
    constexpr auto data = cthsm::detail::normalize<model>();
    
    CHECK(data.state_count == 5); // root, s1, s1_1, s1_2, s2
    
    CHECK(data.get_state_name(0) == "/root");
    CHECK(data.get_state_name(1) == "/root/s1");
    CHECK(data.get_state_name(2) == "/root/s1/s1_1");
    CHECK(data.get_state_name(3) == "/root/s1/s1_2");
    CHECK(data.get_state_name(4) == "/root/s2");
    
    // Check parents
    CHECK(data.states[0].parent_id == cthsm::detail::invalid_index);
    CHECK(data.states[1].parent_id == 0);
    CHECK(data.states[2].parent_id == 1);
    CHECK(data.states[3].parent_id == 1);
    CHECK(data.states[4].parent_id == 0);
}

TEST_CASE("Normalization - Transition Resolution") {
    constexpr auto model = define("root",
        state("A",
            transition(target("B")),      // Sibling resolution
            transition(target("/root/B")) // Absolute resolution
        ),
        state("B")
    );
    
    constexpr auto data = cthsm::detail::normalize<model>();
    
    CHECK(data.state_count == 3);
    CHECK(data.transition_count == 2);
    
    std::size_t id_A = 1;
    std::size_t id_B = 2;
    
    CHECK(data.get_state_name(id_A) == "/root/A");
    CHECK(data.get_state_name(id_B) == "/root/B");
    
    // Transition 0: Sibling
    CHECK(data.transitions[0].source_id == id_A);
    CHECK(data.transitions[0].target_id == id_B);
    
    // Transition 1: Absolute
    CHECK(data.transitions[1].source_id == id_A);
    CHECK(data.transitions[1].target_id == id_B);
}

TEST_CASE("Normalization - Events and Deferral") {
    constexpr auto model = define("root",
        state("s1",
            transition(on("E1"), target("s2")),
            defer("E2", "E3")
        ),
        state("s2",
            transition(on("E2"))
        )
    );
    
    constexpr auto data = cthsm::detail::normalize<model>();
    
    // Count events: E1 (in trans), E2 (in defer), E3 (in defer), E2 (in trans)
    // Order: Defer (Pass 1) -> Transitions (Pass 2)
    CHECK(data.event_count == 4);
    
    // Defer events from s1
    CHECK(data.get_event_name(0) == "E2");
    CHECK(data.get_event_name(1) == "E3");
    
    // Transition event from s1
    CHECK(data.get_event_name(2) == "E1");
    
    // Transition event from s2
    CHECK(data.get_event_name(3) == "E2");
    
    // Verify transitions link to events
    // T0: s1 -> s2 on E1 (ID 2)
    CHECK(data.transitions[0].event_id == 2);
    
    // T1: s2 -> internal? on E2 (ID 3)
    CHECK(data.transitions[1].event_id == 3);
}
