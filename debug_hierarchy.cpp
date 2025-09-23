#include <iostream>
#include "hsm.hpp"

int main() {
  auto model = hsm::define(
    "HierarchyTest",
    hsm::initial(hsm::target("parent/child1")),
    hsm::state("parent",
      hsm::transition(hsm::on("EXIT"), hsm::target("../outside")),
      hsm::state("child1"),
      hsm::state("child2")
    ),
    hsm::state("outside")
  );
  
  // Build transition table
  hsm::buildTransitionTable(*model);
  
  std::cout << "Transition table for /HierarchyTest/parent/child1:" << std::endl;
  auto it = model->transition_map.find("/HierarchyTest/parent/child1");
  if (it != model->transition_map.end()) {
    std::cout << "  Events in table:" << std::endl;
    for (const auto& [event, transitions] : it->second) {
      std::cout << "    " << event << " -> " << transitions.size() << " transitions" << std::endl;
      for (const auto* trans : transitions) {
        std::cout << "      Source: " << trans->source << ", Target: " << trans->target << std::endl;
      }
    }
  } else {
    std::cout << "  Not found in transition map!" << std::endl;
  }
  
  // Also check parent
  std::cout << "\nTransition table for /HierarchyTest/parent:" << std::endl;
  it = model->transition_map.find("/HierarchyTest/parent");
  if (it != model->transition_map.end()) {
    std::cout << "  Events in table:" << std::endl;
    for (const auto& [event, transitions] : it->second) {
      std::cout << "    " << event << " -> " << transitions.size() << " transitions" << std::endl;
    }
  }
  
  return 0;
}