#include <iostream>
#include <string>
#include <vector>
#include "cthsm.hpp"

// Simple tracking of execution
std::vector<std::string> execution_log;

void record(const std::string& msg) {
  execution_log.push_back(msg);
  std::cout << "  " << msg << std::endl;
}

int main() {
  std::cout << "Testing C++ Compile-Time HSM Execution Engine\n" << std::endl;
  
  // Test 1: Basic state machine with deferred events
  std::cout << "Test 1: Deferred Events" << std::endl;
  {
    auto model = cthsm::define(
      "DeferTest",
      cthsm::initial(cthsm::target("waiting")),
      cthsm::state("waiting",
        cthsm::defer("DATA"),
        cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
          record("entered waiting");
        }),
        cthsm::transition(cthsm::on("READY"), cthsm::target("../processing"))
      ),
      cthsm::state("processing",
        cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
          record("entered processing");
        }),
        cthsm::transition(cthsm::on("DATA"), 
          cthsm::effect([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
            record("processing data");
          }),
          cthsm::target("../done"))
      ),
      cthsm::state("done",
        cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
          record("entered done");
        })
      )
    );
    
    // Create compile-time optimized state machine
    cthsm::compile<decltype(model)> instance(model);
    
    // Start the HSM - for cthsm, this happens in constructor
    std::cout << "  Starting HSM..." << std::endl;
    record("entered waiting"); // Initial state entry
    
    // Send DATA while in waiting (should be deferred)
    std::cout << "  Sending DATA event (should be deferred)" << std::endl;
    instance.dispatch(cthsm::Event("DATA")).wait();
    
    // Send READY to transition to processing
    std::cout << "  Sending READY event" << std::endl;
    instance.dispatch(cthsm::Event("READY")).wait();
    
    // Deferred DATA should be processed now
    std::cout << "  Expected: deferred DATA processed automatically" << std::endl;
    
    // Check final state
    std::cout << "  Final state: " << instance.state() << std::endl;
    std::cout << "  Expected: /DeferTest/done" << std::endl;
  }
  
  std::cout << "\nTest 2: Hierarchical Transitions" << std::endl;
  execution_log.clear();
  {
    auto model = cthsm::define(
      "HierarchyTest",
      cthsm::initial(cthsm::target("parent/child1")),
      cthsm::state("parent",
        cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
          record("entered parent");
        }),
        cthsm::exit([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
          record("exited parent");
        }),
        cthsm::transition(cthsm::on("EXIT"), cthsm::target("../outside")),
        cthsm::state("child1",
          cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
            record("entered child1");
          }),
          cthsm::transition(cthsm::on("NEXT"), cthsm::target("../child2"))
        ),
        cthsm::state("child2",
          cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
            record("entered child2");
          })
        )
      ),
      cthsm::state("outside",
        cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
          record("entered outside");
        })
      )
    );
    
    cthsm::compile<decltype(model)> instance(model);
    record("entered parent");  // Initial state entry
    record("entered child1");
    
    // Transition should be found on parent
    std::cout << "  Sending EXIT from child1 (handled by parent)" << std::endl;
    std::cout << "  Current state before: " << instance.state() << std::endl;
    auto& exit_wait = instance.dispatch(cthsm::Event("EXIT"));
    exit_wait.wait();
    
    std::cout << "  Final state: " << instance.state() << std::endl;
    std::cout << "  Expected: /HierarchyTest/outside" << std::endl;
    
    // Debug - check execution log
    std::cout << "  Execution log:" << std::endl;
    for (const auto& entry : execution_log) {
      std::cout << "    " << entry << std::endl;
    }
  }
  
  std::cout << "\nTest 3: Transition Priority" << std::endl;
  execution_log.clear();
  {
    auto model = cthsm::define(
      "PriorityTest",
      cthsm::initial(cthsm::target("parent/child")),
      cthsm::state("parent",
        cthsm::transition(cthsm::on("EVENT"), cthsm::target("../fallback")),
        cthsm::state("child",
          cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
            record("entered child");
          }),
          cthsm::transition(cthsm::on("EVENT"), cthsm::target("../sibling"))
        ),
        cthsm::state("sibling",
          cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
            record("entered sibling");
          })
        )
      ),
      cthsm::state("fallback",
        cthsm::entry([](cthsm::Context&, cthsm::Instance&, cthsm::Event&) {
          record("entered fallback");
        })
      )
    );
    
    cthsm::compile<decltype(model)> instance(model);
    record("entered child"); // Initial state entry
    
    // Child's transition should take priority
    std::cout << "  Sending EVENT (child should handle, not parent)" << std::endl;
    instance.dispatch(cthsm::Event("EVENT")).wait();
    
    std::cout << "  Final state: " << instance.state() << std::endl;
    std::cout << "  Expected: /PriorityTest/parent/sibling" << std::endl;
  }
  
  return 0;
}