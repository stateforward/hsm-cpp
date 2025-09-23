#include <chrono>
#include <iostream>
#include <iomanip>
#include <sys/resource.h>
#include "hsm.hpp"

// Simple light HSM instance
class LightHSM : public hsm::Instance {
public:
    explicit LightHSM() : hsm::Instance() {}
};

// Get current memory usage in bytes
size_t getCurrentMemoryUsage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return static_cast<size_t>(usage.ru_maxrss) * 1024; // Convert KB to bytes on Linux
}

// Create simple on/off light state machine model
std::unique_ptr<hsm::Model> createLightModel() {
    return hsm::define(
        "LightHSM",
        hsm::state("off"),
        hsm::state("on"),
        hsm::transition(hsm::on("on"), hsm::source("off"), hsm::target("on")),
        hsm::transition(hsm::on("off"), hsm::source("on"), hsm::target("off")),
        hsm::initial(hsm::target("off"))
    );
}

int main() {
    std::cout << "Light State Machine Benchmark (C++)" << std::endl;
    std::cout << "====================================" << std::endl;

    const int iterations = 100000;
    const int warmup_iterations = 1000;

    // Create model and HSM instance
    auto model = createLightModel();
    LightHSM instance;
    hsm::HSM hsm_instance(instance, model);

    // Create events
    hsm::Event on_event;
    on_event.name = "on";
    hsm::Event off_event;
    off_event.name = "off";

    // Record memory before benchmark
    size_t memBefore = getCurrentMemoryUsage();

    // Warmup
    for (int i = 0; i < warmup_iterations; i++) {
        instance.dispatch(on_event).wait();
        instance.dispatch(off_event).wait();
    }

    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        instance.dispatch(on_event).wait();
        instance.dispatch(off_event).wait();
    }

    auto end = std::chrono::high_resolution_clock::now();

    // Record memory after benchmark
    size_t memAfter = getCurrentMemoryUsage();

    // Calculate results
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double totalTransitions = static_cast<double>(iterations) * 2.0; // Two transitions per iteration
    double transitionsPerSecond = (totalTransitions / static_cast<double>(duration)) * 1000000.0;
    double timePerTransitionNs = (static_cast<double>(duration) * 1000.0) / totalTransitions;
    size_t memoryUsed = memAfter - memBefore;
    double bytesPerOp = static_cast<double>(memoryUsed) / totalTransitions;

    // Print results
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Total transitions: " << static_cast<int>(totalTransitions) << std::endl;
    std::cout << "Transitions per second: " << std::fixed << std::setprecision(0) 
              << transitionsPerSecond << std::endl;
    std::cout << "Memory bytes per operation: " << std::fixed << std::setprecision(1) 
              << bytesPerOp << std::endl;
    std::cout << "Time per transition: " << std::fixed << std::setprecision(1) 
              << timePerTransitionNs << " ns" << std::endl;

    return 0;
}