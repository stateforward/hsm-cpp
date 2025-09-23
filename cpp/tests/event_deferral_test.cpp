#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#include "hsm.hpp"

// Test instance for event deferral
class EventDeferralInstance : public hsm::Instance {
public:
    std::vector<std::string> execution_log;
    std::atomic<int> counter{0};
    std::atomic<bool> flag{false};
    
    void log(const std::string& message) {
        execution_log.push_back(message);
    }
    
    void clear() {
        execution_log.clear();
        counter = 0;
        flag = false;
    }
};

// Action functions
void log_entry(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<EventDeferralInstance&>(inst);
    test_inst.log("entry_" + name);
}

void log_exit(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<EventDeferralInstance&>(inst);
    test_inst.log("exit_" + name);
}

void log_effect(const std::string& name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<EventDeferralInstance&>(inst);
    test_inst.log("effect_" + name);
}

void process_event(const std::string& event_name, hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<EventDeferralInstance&>(inst);
    test_inst.log("processed_" + event_name);
}

void increment_counter(hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
    auto& test_inst = static_cast<EventDeferralInstance&>(inst);
    test_inst.counter++;
    test_inst.log("increment_counter");
}

TEST_CASE("Event Deferral - Basic Functionality") {
    SUBCASE("Simple Event Deferral") {
        auto model = hsm::define(
            "SimpleDeferral",
            hsm::initial(hsm::target("busy")),
            hsm::state("busy",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("busy", ctx, inst, event);
                }),
                hsm::defer("REQUEST"),
                hsm::transition(hsm::on("COMPLETE"), hsm::target("../ready"))
            ),
            hsm::state("ready",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("ready", ctx, inst, event);
                }),
                hsm::transition(hsm::on("REQUEST"), 
                    hsm::target("../processing"),
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("REQUEST", ctx, inst, event);
                    }))
            ),
            hsm::state("processing",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("processing", ctx, inst, event);
                })
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/SimpleDeferral/busy");
        
        // Send REQUEST while busy - should be deferred
        instance.dispatch(hsm::Event("REQUEST")).wait();
        CHECK(instance.state() == "/SimpleDeferral/busy");
        
        // No processing should have occurred
        bool found_processed = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_REQUEST") {
                found_processed = true;
                break;
            }
        }
        CHECK_FALSE(found_processed);
        
        // Complete and transition to ready
        instance.dispatch(hsm::Event("COMPLETE")).wait();
        CHECK(instance.state() == "/SimpleDeferral/processing");
        
        // Deferred REQUEST should have been processed automatically
        found_processed = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_REQUEST") {
                found_processed = true;
                break;
            }
        }
        CHECK(found_processed);
        
        // Check execution order
        auto& log = instance.execution_log;
        auto entry_busy = std::find(log.begin(), log.end(), "entry_busy");
        auto entry_ready = std::find(log.begin(), log.end(), "entry_ready");
        auto processed_req = std::find(log.begin(), log.end(), "processed_REQUEST");
        auto entry_processing = std::find(log.begin(), log.end(), "entry_processing");
        
        CHECK(entry_busy != log.end());
        CHECK(entry_ready != log.end());
        CHECK(processed_req != log.end());
        CHECK(entry_processing != log.end());
        CHECK(entry_busy < entry_ready);
        CHECK(entry_ready < processed_req);
        CHECK(processed_req < entry_processing);
    }

    SUBCASE("Multiple Deferred Events") {
        auto model = hsm::define(
            "MultipleDeferral",
            hsm::initial(hsm::target("busy")),
            hsm::state("busy",
                hsm::defer("REQUEST1", "REQUEST2", "REQUEST3"),
                hsm::transition(hsm::on("READY"), hsm::target("../processing"))
            ),
            hsm::state("processing",
                hsm::transition(hsm::on("REQUEST1"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("REQUEST1", ctx, inst, event);
                    })),
                hsm::transition(hsm::on("REQUEST2"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("REQUEST2", ctx, inst, event);
                    })),
                hsm::transition(hsm::on("REQUEST3"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("REQUEST3", ctx, inst, event);
                    }))
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        // Send multiple deferred events
        instance.dispatch(hsm::Event("REQUEST2")).wait();
        instance.dispatch(hsm::Event("REQUEST1")).wait();
        instance.dispatch(hsm::Event("REQUEST3")).wait();
        
        CHECK(instance.state() == "/MultipleDeferral/busy");
        
        // Transition to processing
        instance.dispatch(hsm::Event("READY")).wait();
        CHECK(instance.state() == "/MultipleDeferral/processing");
        
        // All deferred events should have been processed in FIFO order
        auto& log = instance.execution_log;
        auto req2 = std::find(log.begin(), log.end(), "processed_REQUEST2");
        auto req1 = std::find(log.begin(), log.end(), "processed_REQUEST1");
        auto req3 = std::find(log.begin(), log.end(), "processed_REQUEST3");
        
        CHECK(req2 != log.end());
        CHECK(req1 != log.end());
        CHECK(req3 != log.end());
        CHECK(req2 < req1);  // REQUEST2 was sent first
        CHECK(req1 < req3);  // REQUEST1 was sent second
    }
}

TEST_CASE("Event Deferral - Hierarchical Behavior") {
    SUBCASE("Inherited Deferred Events") {
        auto model = hsm::define(
            "HierarchicalDeferral",
            hsm::initial(hsm::target("parent")),
            hsm::state("parent",
                hsm::defer("PARENT_EVENT"),
                hsm::initial(hsm::target("child")),
                hsm::state("child",
                    hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_entry("child", ctx, inst, event);
                    }),
                    hsm::defer("CHILD_EVENT"),
                    hsm::transition(hsm::on("EXIT_CHILD"), hsm::target("../sibling"))
                ),
                hsm::state("sibling",
                    hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_entry("sibling", ctx, inst, event);
                    }),
                    hsm::transition(hsm::on("CHILD_EVENT"), 
                        hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                            process_event("CHILD_EVENT", ctx, inst, event);
                        }))
                ),
                hsm::transition(hsm::on("EXIT_PARENT"), hsm::target("../outside"))
            ),
            hsm::state("outside",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("outside", ctx, inst, event);
                }),
                hsm::transition(hsm::on("PARENT_EVENT"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("PARENT_EVENT", ctx, inst, event);
                    })),
                hsm::transition(hsm::on("CHILD_EVENT"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("CHILD_EVENT", ctx, inst, event);
                    }))
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        CHECK(instance.state() == "/HierarchicalDeferral/parent/child");
        
        // Send events that should be deferred at different levels
        instance.dispatch(hsm::Event("PARENT_EVENT")).wait();
        instance.dispatch(hsm::Event("CHILD_EVENT")).wait();
        
        // Both should be deferred
        CHECK(instance.state() == "/HierarchicalDeferral/parent/child");
        
        // Exit child - CHILD_EVENT should be processed but PARENT_EVENT still deferred
        instance.dispatch(hsm::Event("EXIT_CHILD")).wait();
        CHECK(instance.state() == "/HierarchicalDeferral/parent/sibling");
        
        bool found_child_processed = false;
        bool found_parent_processed = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_CHILD_EVENT") found_child_processed = true;
            if (log == "processed_PARENT_EVENT") found_parent_processed = true;
        }
        CHECK(found_child_processed);
        CHECK_FALSE(found_parent_processed);
        
        // Exit parent - PARENT_EVENT should now be processed
        instance.dispatch(hsm::Event("EXIT_PARENT")).wait();
        CHECK(instance.state() == "/HierarchicalDeferral/outside");
        
        found_parent_processed = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_PARENT_EVENT") found_parent_processed = true;
        }
        CHECK(found_parent_processed);
    }

    SUBCASE("Deferred Events Across State Boundaries") {
        auto model = hsm::define(
            "BoundaryDeferral",
            hsm::initial(hsm::target("container")),
            hsm::state("container",
                hsm::initial(hsm::target("inner")),
                hsm::state("inner",
                    hsm::defer("DEFERRED"),
                    hsm::transition(hsm::on("MOVE"), hsm::target("../../other"))
                )
            ),
            hsm::state("other",
                hsm::transition(hsm::on("DEFERRED"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("DEFERRED", ctx, inst, event);
                    }))
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        // Send deferred event
        instance.dispatch(hsm::Event("DEFERRED")).wait();
        CHECK(instance.state() == "/BoundaryDeferral/container/inner");
        
        // Move to completely different state tree
        instance.dispatch(hsm::Event("MOVE")).wait();
        CHECK(instance.state() == "/BoundaryDeferral/other");
        
        // Deferred event should be processed
        bool found_processed = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_DEFERRED") {
                found_processed = true;
                break;
            }
        }
        CHECK(found_processed);
    }
}

TEST_CASE("Event Deferral - Priority and Conflicts") {
    SUBCASE("Deferral Takes Priority Over Transition") {
        auto model = hsm::define(
            "DeferralPriority",
            hsm::initial(hsm::target("conflicted")),
            hsm::state("conflicted",
                hsm::defer("EVENT"),
                hsm::transition(hsm::on("EVENT"), 
                    hsm::target("../should_not_reach"),
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        log_effect("should_not_execute", ctx, inst, event);
                    })),
                hsm::transition(hsm::on("RESOLVE"), hsm::target("../resolved"))
            ),
            hsm::state("should_not_reach",
                hsm::entry([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                    log_entry("should_not_reach", ctx, inst, event);
                })
            ),
            hsm::state("resolved",
                hsm::transition(hsm::on("EVENT"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("EVENT", ctx, inst, event);
                    }))
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        // Send EVENT - should be deferred despite having a transition
        instance.dispatch(hsm::Event("EVENT")).wait();
        CHECK(instance.state() == "/DeferralPriority/conflicted");
        
        // Check that transition effect was NOT executed
        bool found_bad_effect = false;
        bool found_bad_entry = false;
        for (const auto& log : instance.execution_log) {
            if (log == "effect_should_not_execute") found_bad_effect = true;
            if (log == "entry_should_not_reach") found_bad_entry = true;
        }
        CHECK_FALSE(found_bad_effect);
        CHECK_FALSE(found_bad_entry);
        
        // Resolve conflict
        instance.dispatch(hsm::Event("RESOLVE")).wait();
        CHECK(instance.state() == "/DeferralPriority/resolved");
        
        // Now EVENT should be processed
        bool found_processed = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_EVENT") {
                found_processed = true;
                break;
            }
        }
        CHECK(found_processed);
    }

    SUBCASE("Selective Deferral") {
        auto model = hsm::define(
            "SelectiveDeferral",
            hsm::initial(hsm::target("selective")),
            hsm::state("selective",
                hsm::defer("DEFER_ME"),
                hsm::transition(hsm::on("PROCESS_ME"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("PROCESS_ME", ctx, inst, event);
                    })),
                hsm::transition(hsm::on("DONE"), hsm::target("../done"))
            ),
            hsm::state("done",
                hsm::transition(hsm::on("DEFER_ME"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("DEFER_ME", ctx, inst, event);
                    }))
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        // Send both types of events
        instance.dispatch(hsm::Event("DEFER_ME")).wait();
        instance.dispatch(hsm::Event("PROCESS_ME")).wait();
        
        CHECK(instance.state() == "/SelectiveDeferral/selective");
        
        // Only PROCESS_ME should have been handled
        bool found_process_me = false;
        bool found_defer_me = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_PROCESS_ME") found_process_me = true;
            if (log == "processed_DEFER_ME") found_defer_me = true;
        }
        CHECK(found_process_me);
        CHECK_FALSE(found_defer_me);
        
        // Transition to done
        instance.dispatch(hsm::Event("DONE")).wait();
        CHECK(instance.state() == "/SelectiveDeferral/done");
        
        // Now DEFER_ME should be processed
        found_defer_me = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_DEFER_ME") found_defer_me = true;
        }
        CHECK(found_defer_me);
    }
}

TEST_CASE("Event Deferral - Edge Cases") {
    SUBCASE("Deferred Event Re-Deferral") {
        auto model = hsm::define(
            "ReDeferral",
            hsm::initial(hsm::target("state1")),
            hsm::state("state1",
                hsm::defer("BOUNCING"),
                hsm::transition(hsm::on("NEXT"), hsm::target("../state2"))
            ),
            hsm::state("state2",
                hsm::defer("BOUNCING"),
                hsm::transition(hsm::on("NEXT"), hsm::target("../state3"))
            ),
            hsm::state("state3",
                hsm::transition(hsm::on("BOUNCING"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("BOUNCING", ctx, inst, event);
                    }))
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        // Send event that will be deferred multiple times
        instance.dispatch(hsm::Event("BOUNCING")).wait();
        CHECK(instance.state() == "/ReDeferral/state1");
        
        // Move to state2 - event should still be deferred
        instance.dispatch(hsm::Event("NEXT")).wait();
        CHECK(instance.state() == "/ReDeferral/state2");
        
        // Event should not have been processed yet
        bool found_processed = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_BOUNCING") {
                found_processed = true;
                break;
            }
        }
        CHECK_FALSE(found_processed);
        
        // Move to state3 - event should finally be processed
        instance.dispatch(hsm::Event("NEXT")).wait();
        CHECK(instance.state() == "/ReDeferral/state3");
        
        found_processed = false;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_BOUNCING") {
                found_processed = true;
                break;
            }
        }
        CHECK(found_processed);
    }

    SUBCASE("Deferred Events in Final State") {
        auto model = hsm::define(
            "FinalStateDeferral",
            hsm::initial(hsm::target("active")),
            hsm::state("active",
                hsm::defer("DEFERRED"),
                hsm::transition(hsm::on("FINISH"), hsm::target("../done"))
            ),
            hsm::final("done")
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        // Send deferred event
        instance.dispatch(hsm::Event("DEFERRED")).wait();
        
        // Move to final state
        instance.dispatch(hsm::Event("FINISH")).wait();
        CHECK(instance.state() == "/FinalStateDeferral/done");
        
        // Deferred event should be discarded (final states don't process events)
        // Send another event to verify final state behavior
        instance.dispatch(hsm::Event("ANOTHER")).wait();
        CHECK(instance.state() == "/FinalStateDeferral/done");
    }

    SUBCASE("Large Number of Deferred Events") {
        auto model = hsm::define(
            "ManyDeferredEvents",
            hsm::initial(hsm::target("collecting")),
            hsm::state("collecting",
                hsm::defer("DATA"),
                hsm::transition(hsm::on("PROCESS_ALL"), hsm::target("../processing"))
            ),
            hsm::state("processing",
                hsm::transition(hsm::on("DATA"), 
                    hsm::effect([](hsm::Context& /*ctx*/, hsm::Instance& inst, hsm::Event& /*event*/) {
                        auto& test_inst = static_cast<EventDeferralInstance&>(inst);
                        test_inst.counter++;
                    }))
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        // Send many deferred events
        const int num_events = 20;
        for (int i = 0; i < num_events; ++i) {
            instance.dispatch(hsm::Event("DATA")).wait();
        }
        
        CHECK(instance.state() == "/ManyDeferredEvents/collecting");
        CHECK(instance.counter.load() == 0);
        
        // Process all deferred events
        instance.dispatch(hsm::Event("PROCESS_ALL")).wait();
        CHECK(instance.state() == "/ManyDeferredEvents/processing");
        
        // All events should have been processed
        CHECK(instance.counter.load() == num_events);
    }

    SUBCASE("Interleaved Deferred and Non-Deferred Events") {
        auto model = hsm::define(
            "InterleavedEvents",
            hsm::initial(hsm::target("mixed")),
            hsm::state("mixed",
                hsm::defer("DEFERRED"),
                hsm::transition(hsm::on("IMMEDIATE"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("IMMEDIATE", ctx, inst, event);
                    })),
                hsm::transition(hsm::on("DONE"), hsm::target("../final"))
            ),
            hsm::state("final",
                hsm::transition(hsm::on("DEFERRED"), 
                    hsm::effect([](hsm::Context& ctx, hsm::Instance& inst, hsm::Event& event) {
                        process_event("DEFERRED", ctx, inst, event);
                    }))
            )
        );

        EventDeferralInstance instance;
        hsm::start(instance, model);
        
        // Send interleaved events
        instance.dispatch(hsm::Event("DEFERRED")).wait();
        instance.dispatch(hsm::Event("IMMEDIATE")).wait();
        instance.dispatch(hsm::Event("DEFERRED")).wait();
        instance.dispatch(hsm::Event("IMMEDIATE")).wait();
        instance.dispatch(hsm::Event("DEFERRED")).wait();
        
        // Check that only IMMEDIATE events were processed
        int immediate_count = 0;
        int deferred_count = 0;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_IMMEDIATE") immediate_count++;
            if (log == "processed_DEFERRED") deferred_count++;
        }
        CHECK(immediate_count == 2);
        CHECK(deferred_count == 0);
        
        // Move to final state
        instance.dispatch(hsm::Event("DONE")).wait();
        CHECK(instance.state() == "/InterleavedEvents/final");
        
        // All deferred events should now be processed
        deferred_count = 0;
        for (const auto& log : instance.execution_log) {
            if (log == "processed_DEFERRED") deferred_count++;
        }
        CHECK(deferred_count == 3);
    }
}