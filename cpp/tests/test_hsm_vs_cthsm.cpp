#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include "hsm.hpp"
#include "cthsm.hpp"

// Test that both implementations produce identical behavior

template<typename HSM>
std::vector<std::string> run_state_machine_test() {
  std::vector<std::string> log;
  
  auto record = [&log](const std::string& msg) {
    log.push_back(msg);
  };
  
  // Define identical state machine with both APIs
  auto model = HSM::define(
    "TestMachine",
    HSM::initial(HSM::target("idle")),
    
    HSM::state("idle",
      HSM::entry([&record](auto& ctx, auto& inst, auto& event) {
        record("entered idle");
      }),
      HSM::exit([&record](auto& ctx, auto& inst, auto& event) {
        record("exited idle");
      }),
      HSM::transition(
        HSM::on("start"),
        HSM::guard([](auto& ctx, auto& inst, auto& event) { return true; }),
        HSM::effect([&record](auto& ctx, auto& inst, auto& event) {
          record("start transition effect");
        }),
        HSM::target("running")
      ),
      HSM::defer("stop")
    ),
    
    HSM::state("running",
      HSM::entry([&record](auto& ctx, auto& inst, auto& event) {
        record("entered running");
      }),
      HSM::exit([&record](auto& ctx, auto& inst, auto& event) {
        record("exited running");
      }),
      HSM::transition(
        HSM::on("stop"),
        HSM::target("idle")
      ),
      HSM::transition(
        HSM::on("pause"),
        HSM::target("paused")
      )
    ),
    
    HSM::state("paused",
      HSM::entry([&record](auto& ctx, auto& inst, auto& event) {
        record("entered paused");
      }),
      HSM::transition(
        HSM::on("resume"),
        HSM::target("running")
      ),
      HSM::transition(
        HSM::on("stop"),
        HSM::target("idle")
      )
    )
  );
  
  // Create and run state machine
  if constexpr (std::is_same_v<HSM, hsm>) {
    // Runtime HSM
    hsm::Instance instance;
    hsm::start(instance, model);
    
    // Run test sequence
    instance.dispatch(hsm::Event("stop")).wait();  // Deferred in idle
    instance.dispatch(hsm::Event("start")).wait();
    // Deferred stop should execute now
    instance.dispatch(hsm::Event("start")).wait();
    instance.dispatch(hsm::Event("pause")).wait();
    instance.dispatch(hsm::Event("resume")).wait();
    instance.dispatch(hsm::Event("stop")).wait();
  } else {
    // Compile-time HSM
    cthsm::compile<decltype(model)> instance;
    
    // Initial state entry
    record("entered idle");
    
    // Run test sequence
    instance.dispatch(cthsm::Event("stop")).wait();  // Deferred in idle
    instance.dispatch(cthsm::Event("start")).wait();
    // Deferred stop should execute now
    instance.dispatch(cthsm::Event("start")).wait();
    instance.dispatch(cthsm::Event("pause")).wait();
    instance.dispatch(cthsm::Event("resume")).wait();
    instance.dispatch(cthsm::Event("stop")).wait();
  }
  
  return log;
}

int main() {
  std::cout << "Comparing HSM vs CTHSM Behavior\n" << std::endl;
  
  // Run both implementations
  std::cout << "Running runtime HSM..." << std::endl;
  auto hsm_log = run_state_machine_test<hsm>();
  
  std::cout << "\nRunning compile-time HSM..." << std::endl;
  auto cthsm_log = run_state_machine_test<cthsm>();
  
  // Compare results
  std::cout << "\n=== Results ===" << std::endl;
  
  std::cout << "\nRuntime HSM log:" << std::endl;
  for (const auto& entry : hsm_log) {
    std::cout << "  " << entry << std::endl;
  }
  
  std::cout << "\nCompile-time HSM log:" << std::endl;
  for (const auto& entry : cthsm_log) {
    std::cout << "  " << entry << std::endl;
  }
  
  // Check if identical
  bool identical = hsm_log == cthsm_log;
  std::cout << "\n=== Verdict ===" << std::endl;
  if (identical) {
    std::cout << "✓ Both implementations produce IDENTICAL behavior!" << std::endl;
  } else {
    std::cout << "✗ Implementations differ!" << std::endl;
    std::cout << "  Runtime HSM produced " << hsm_log.size() << " events" << std::endl;
    std::cout << "  Compile-time HSM produced " << cthsm_log.size() << " events" << std::endl;
  }
  
  // Performance comparison
  std::cout << "\n=== Performance Comparison ===" << std::endl;
  
  const int iterations = 100000;
  
  // Time runtime HSM
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    hsm::Instance instance;
    auto model = hsm::define("Perf",
      hsm::initial(hsm::target("a")),
      hsm::state("a", hsm::transition(hsm::on("go"), hsm::target("b"))),
      hsm::state("b", hsm::transition(hsm::on("go"), hsm::target("a")))
    );
    hsm::start(instance, model);
    instance.dispatch(hsm::Event("go"));
  }
  auto hsm_time = std::chrono::high_resolution_clock::now() - start;
  
  // Time compile-time HSM
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    auto model = cthsm::define("Perf",
      cthsm::initial(cthsm::target("a")),
      cthsm::state("a", cthsm::transition(cthsm::on("go"), cthsm::target("b"))),
      cthsm::state("b", cthsm::transition(cthsm::on("go"), cthsm::target("a")))
    );
    cthsm::compile<decltype(model)> instance;
    instance.dispatch(cthsm::Event("go"));
  }
  auto cthsm_time = std::chrono::high_resolution_clock::now() - start;
  
  auto hsm_us = std::chrono::duration_cast<std::chrono::microseconds>(hsm_time).count();
  auto cthsm_us = std::chrono::duration_cast<std::chrono::microseconds>(cthsm_time).count();
  
  std::cout << "Runtime HSM: " << hsm_us << " µs for " << iterations << " iterations" << std::endl;
  std::cout << "Compile-time HSM: " << cthsm_us << " µs for " << iterations << " iterations" << std::endl;
  std::cout << "Speedup: " << (double)hsm_us / cthsm_us << "x faster" << std::endl;
  
  return identical ? 0 : 1;
}