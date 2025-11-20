#include <iostream>

#include "cthsm/cthsm.hpp"

int main() {
  constexpr auto model = cthsm::define("example");
  cthsm::compile<model> machine;
  cthsm::Instance inst;
  machine.start(inst);

  const auto state = machine.state();
  std::cout << "cthsm example current state: "
            << (state.empty() ? "<none>" : std::string(state)) << '\n';

  machine.dispatch(inst, cthsm::AnyEvent{"noop"});
  return 0;
}
