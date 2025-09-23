#include <iostream>

#include "../include/cthsm.hpp"

// Example of compile-time HSM usage
int main() {
  // Define the model with exact same syntax as runtime HSM
  auto model = cthsm::define(
      "light", cthsm::initial(cthsm::target("off")),
      cthsm::state(
          "off", cthsm::transition(cthsm::on("turn_on"), cthsm::target("on"))),
      cthsm::state("on", cthsm::transition(cthsm::on("turn_off"),
                                           cthsm::target("off"))));

  // Compile it
  cthsm::compile<decltype(model)> sm;

  // Use it
  std::cout << "Initial state: " << sm.state() << "\n";

  sm.dispatch("turn_on");
  std::cout << "After turn_on: " << sm.state() << "\n";

  sm.dispatch("turn_off");
  std::cout << "After turn_off: " << sm.state() << "\n";

  return 0;
}