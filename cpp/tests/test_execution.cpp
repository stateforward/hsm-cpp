#include <iostream>
#include <string>
#include <vector>
#include "hsm.hpp"

// Simple tracking of execution
std::vector<std::string> execution_log;

void record(const std::string& msg) {
  execution_log.push_back(msg);
  std::cout << "  " << msg << std::endl;
}

int main() {
  std::cout << "Testing C++ HSM Execution Engine\n" << std::endl;
  
  // Test 1: Basic state machine with deferred events
  std::cout << "Test 1: Deferred Events" << std::endl;
  {
    auto model = hsm::define(
      "DeferTest",
      hsm::initial(hsm::target("waiting")),
      hsm::state("waiting",
        hsm::defer("DATA"),
        hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
          record("entered waiting");
        }),
        hsm::transition(hsm::on("READY"), hsm::target("../processing"))
      ),
      hsm::state("processing",
        hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
          record("entered processing");
        }),
        hsm::transition(hsm::on("DATA"), 
          hsm::effect([](hsm::Context&, hsm::Instance&, hsm::Event&) {
            record("processing data");
          }),
          hsm::target("../done"))
      ),
      hsm::state("done",
        hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
          record("entered done");
        })
      )
    );
    
    hsm::Instance instance;
    
    // Start the HSM
    std::cout << "  Starting HSM..." << std::endl;
    hsm::start(instance, model);
    
    // Send DATA while in waiting (should be deferred)
    std::cout << "  Sending DATA event (should be deferred)" << std::endl;
    instance.dispatch(hsm::Event("DATA")).wait();
    
    // Send READY to transition to processing
    std::cout << "  Sending READY event" << std::endl;
    instance.dispatch(hsm::Event("READY")).wait();
    
    // Deferred DATA should be processed now
    std::cout << "  Expected: deferred DATA processed automatically" << std::endl;
    
    // Check final state
    std::cout << "  Final state: " << instance.state() << std::endl;
    std::cout << "  Expected: /DeferTest/done" << std::endl;
  }
  
  std::cout << "\nTest 2: Hierarchical Transitions" << std::endl;
  execution_log.clear();
  {
    auto model = hsm::define(
      "HierarchyTest",
      hsm::initial(hsm::target("parent/child1")),
      hsm::state("parent",
        hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
          record("entered parent");
        }),
        hsm::exit([](hsm::Context&, hsm::Instance&, hsm::Event&) {
          record("exited parent");
        }),
        hsm::transition(hsm::on("EXIT"), hsm::target("../outside")),
        hsm::state("child1",
          hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
            record("entered child1");
          }),
          hsm::transition(hsm::on("NEXT"), hsm::target("../child2"))
        ),
        hsm::state("child2",
          hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
            record("entered child2");
          })
        )
      ),
      hsm::state("outside",
        hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
          record("entered outside");
        })
      )
    );
    
    hsm::Instance instance;
    hsm::start(instance, model);
    
    // Transition should be found on parent
    std::cout << "  Sending EXIT from child1 (handled by parent)" << std::endl;
    std::cout << "  Current state before: " << instance.state() << std::endl;
    auto& exit_wait = instance.dispatch(hsm::Event("EXIT"));
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
    auto model = hsm::define(
      "PriorityTest",
      hsm::initial(hsm::target("parent/child")),
      hsm::state("parent",
        hsm::transition(hsm::on("EVENT"), hsm::target("../fallback")),
        hsm::state("child",
          hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
            record("entered child");
          }),
          hsm::transition(hsm::on("EVENT"), hsm::target("../sibling"))
        ),
        hsm::state("sibling",
          hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
            record("entered sibling");
          })
        )
      ),
      hsm::state("fallback",
        hsm::entry([](hsm::Context&, hsm::Instance&, hsm::Event&) {
          record("entered fallback");
        })
      )
    );
    
    hsm::Instance instance;
    hsm::start(instance, model);
    
    // Child's transition should take priority
    std::cout << "  Sending EVENT (child should handle, not parent)" << std::endl;
    instance.dispatch(hsm::Event("EVENT")).wait();
    
    std::cout << "  Final state: " << instance.state() << std::endl;
    std::cout << "  Expected: /PriorityTest/parent/sibling" << std::endl;
  }
  
  return 0;
}