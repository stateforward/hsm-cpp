#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "cthsm/cthsm.hpp"

using namespace cthsm;

namespace {

struct StartEvent : cthsm::Event<StartEvent> {};
struct PayloadEvent : cthsm::Event<PayloadEvent> {
  int value{};
};

struct Device : cthsm::Instance {
  int runtime_entries = 0;
  int typed_entries = 0;
  int guard_checks = 0;
  int effect_calls = 0;
  int payload_sum = 0;
};

constexpr auto typed_model = define(
    "device", initial(target("idle")),
    state("idle", entry([](cthsm::Context&, Device& d, const cthsm::AnyEvent&) {
            ++d.runtime_entries;
          }),
          entry([](cthsm::Context&, Device& d, const StartEvent&) {
            ++d.typed_entries;
          }),
          transition(
              on<StartEvent>(), guard([](Device& d, const StartEvent&) {
                ++d.guard_checks;
                return true;
              }),
              effect([](Device& d, const StartEvent&) { ++d.effect_calls; }),
              target("idle"))),
    state("active"));

constexpr auto payload_model = define(
    "payload_device", initial(target("idle")),
    state("idle", transition(on<PayloadEvent>(),
                             effect([](Device& d, const PayloadEvent& evt) {
                               d.payload_sum += evt.value;
                             }),
                             target("idle"))));

}  // namespace

TEST_CASE("Typed events dispatch and behavior resolution") {
  compile<typed_model> sm;
  Device dev;

  sm.start(dev);
  CHECK(dev.runtime_entries == 1);  // initial entry via init event
  CHECK(dev.typed_entries == 0);

  sm.dispatch<StartEvent>(dev);
  CHECK(dev.typed_entries == 1);
  CHECK(dev.guard_checks == 1);
  CHECK(dev.effect_calls == 1);
  CHECK(dev.runtime_entries == 2);  // entry invoked again for typed event
  CHECK(sm.state() == "/device/idle");
}

TEST_CASE("Typed dispatch forwards event payloads") {
  compile<payload_model> sm;
  Device dev;
  sm.start(dev);

  PayloadEvent payload{.value = 5};
  sm.dispatch(dev, payload);
  CHECK(dev.payload_sum == 5);

  sm.dispatch(dev, PayloadEvent{.value = 7});
  CHECK(dev.payload_sum == 12);
}
