/*
 * Benchmark comparing:
 * 1. Vanilla C++ Switch-Case FSM (Baseline)
 * 2. Vanilla C++ Function-Pointer FSM
 * 3. cthsm (Compile-Time HSM) - String & Typed Events
 * 4. hsm (Runtime HSM)
 *
 * To compile:
 *   g++ -std=c++20 -O3 -Iinclude -Ibench_deps/sml/include -Ibench_deps/tinyfsm/include examples/comparison_benchmark.cpp -o comparison_benchmark
 *
 * To run:
 *   ./comparison_benchmark
 */

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Include library headers
#include "hsm.hpp"
#include "cthsm/cthsm.hpp"

// Include SML (Single Header is usually in include/boost/sml.hpp)
// We will add -Ibench_deps/sml/include to compile command
#include "boost/sml.hpp"

// Include TinyFSM
// We will add -Ibench_deps/tinyfsm/include to compile command
#include "tinyfsm.hpp"

// ==========================================
// Infrastructure
// ==========================================

// Global side effect to prevent optimization
volatile size_t g_counter = 0;
void tick() { g_counter++; }

// Wrappers for HSM/CTHSM effects
void hsmTick(hsm::Context&, hsm::Instance&, hsm::Event&) { tick(); }
void cthsmTick(cthsm::Context&, cthsm::Instance&, const cthsm::EventBase&) { tick(); }

struct BenchmarkResult {
    std::string name;
    double transitionsPerSecond;
    double relativeSpeed; // 1.0 = baseline
};

// Simple event structure for vanilla implementations
struct VanillaEvent {
    std::string name;
};

// ==========================================
// 1. Vanilla Switch-Case FSM
// ==========================================
class SwitchFSM {
public:
    enum class State {
        Ping,
        Pong
    };

    State currentState = State::Ping;

    void dispatch(const VanillaEvent& event) {
        switch (currentState) {
            case State::Ping:
                if (event.name == "next") {
                    tick();
                    currentState = State::Pong;
                }
                break;
            case State::Pong:
                if (event.name == "next") {
                    tick();
                    currentState = State::Ping;
                }
                break;
        }
    }
};

// ==========================================
// 2. Vanilla Function-Pointer FSM
// ==========================================
class FuncPtrFSM {
public:
    using StateFunc = void (FuncPtrFSM::*)(const VanillaEvent&);
    StateFunc currentState;

    FuncPtrFSM() {
        currentState = &FuncPtrFSM::statePing;
    }

    void dispatch(const VanillaEvent& event) {
        (this->*currentState)(event);
    }

private:
    void statePing(const VanillaEvent& event) {
        if (event.name == "next") {
            tick();
            currentState = &FuncPtrFSM::statePong;
        }
    }

    void statePong(const VanillaEvent& event) {
        if (event.name == "next") {
            tick();
            currentState = &FuncPtrFSM::statePing;
        }
    }
};

// ==========================================
// 3. HSM (Runtime) Implementation
// ==========================================
struct HsmInstance : public hsm::Instance {
    HsmInstance() : hsm::Instance() {}
};

auto hsmModel = hsm::define("PingPong",
    hsm::state("Ping",
        hsm::transition(hsm::on("next"), hsm::target("/PingPong/Pong"), hsm::effect(hsmTick))
    ),
    hsm::state("Pong",
        hsm::transition(hsm::on("next"), hsm::target("/PingPong/Ping"), hsm::effect(hsmTick))
    ),
    hsm::initial(hsm::target("/PingPong/Ping"))
);

// ==========================================
// 4. CTHSM (Compile-Time) Implementation
// ==========================================
struct CthsmInstance : public cthsm::Instance {};

// Define CTHSM model
constexpr auto cthsmModel = cthsm::define("PingPong",
    cthsm::state("Ping",
        cthsm::transition(cthsm::on("next"), cthsm::target("/PingPong/Pong"), cthsm::effect(cthsmTick))
    ),
    cthsm::state("Pong",
        cthsm::transition(cthsm::on("next"), cthsm::target("/PingPong/Ping"), cthsm::effect(cthsmTick))
    ),
    cthsm::initial(cthsm::target("/PingPong/Ping"))
);

using CompiledCthsm = cthsm::compile<cthsmModel, CthsmInstance>;

// Define CTHSM model typed (moved up)
struct NextEvent : cthsm::Event<NextEvent> {};

constexpr auto cthsmModelTyped = cthsm::define("PingPongTyped",
    cthsm::state("Ping",
        cthsm::transition(cthsm::on<NextEvent>(), cthsm::target("/PingPongTyped/Pong"), cthsm::effect(cthsmTick))
    ),
    cthsm::state("Pong",
        cthsm::transition(cthsm::on<NextEvent>(), cthsm::target("/PingPongTyped/Ping"), cthsm::effect(cthsmTick))
    ),
    cthsm::initial(cthsm::target("/PingPongTyped/Ping"))
);

using CompiledCthsmTyped = cthsm::compile<cthsmModelTyped, CthsmInstance>;

// ==========================================
// 6. Boost.SML Implementation
// ==========================================
namespace sml = boost::sml;

struct SmlEvent {
    // SML events can be empty structs
};

struct SmlPingPong {
    auto operator()() const {
        using namespace sml;
        auto action = []{ tick(); };
        return make_transition_table(
           *"Ping"_s + event<SmlEvent> / action = "Pong"_s,
            "Pong"_s + event<SmlEvent> / action = "Ping"_s
        );
    }
};

// ==========================================
// 7. TinyFSM Implementation
// ==========================================
// TinyFSM requires events to be structs inheriting from tinyfsm::Event if using dispatch?
// Actually TinyFSM usually uses dispatch<Event>()
struct TinyEvent : tinyfsm::Event {};

struct TinyFsmList; // Forward declaration

struct TinyFsmBase : tinyfsm::Fsm<TinyFsmBase> {
    virtual void react(TinyEvent const&) {}
    virtual void entry() {}
    virtual void exit() {}
};

struct TinyPong; // Forward

struct TinyPing : TinyFsmBase {
    void react(TinyEvent const&) override;
};

struct TinyPong : TinyFsmBase {
    void react(TinyEvent const&) override;
};

void TinyPing::react(TinyEvent const&) { tick(); transit<TinyPong>(); }
void TinyPong::react(TinyEvent const&) { tick(); transit<TinyPing>(); }

FSM_INITIAL_STATE(TinyFsmBase, TinyPing)

// ==========================================
// Benchmark Runner
// ==========================================
template<typename Func>
BenchmarkResult runScenario(std::string name, Func&& func, int iterations) {
    // Warmup
    g_counter = 0;
    for(int i=0; i<iterations/10; ++i) {
        func();
    }
    
    g_counter = 0;

    auto start = std::chrono::high_resolution_clock::now();
    
    for(int i=0; i<iterations; ++i) {
        func();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double seconds = duration / 1e9;
    // Each iteration usually does 2 transitions (Ping->Pong, Pong->Ping) 
    // BUT here we will just call the func once per iteration in the loop.
    // To make it fair, the func passed in should do ONE round trip (2 transitions) or we adjust math.
    // Let's assume the func does 2 transitions.
    
    double transitions = (double)iterations * 2.0;
    
    return BenchmarkResult{
        name,
        transitions / seconds,
        0.0 // Calculated later
    };
}

int main() {
    const int ITERATIONS = 1000000;
    std::vector<BenchmarkResult> results;

    std::cout << "Benchmarking HSM implementations (" << ITERATIONS << " iterations per scenario)..." << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    VanillaEvent ev{"next"};

    // 1. Switch FSM
    {
        SwitchFSM fsm;
        results.push_back(runScenario("Vanilla C++ (Switch)", [&]() {
            fsm.dispatch(ev); // Ping -> Pong
            fsm.dispatch(ev); // Pong -> Ping
        }, ITERATIONS));
    }

    // 2. FuncPtr FSM
    {
        FuncPtrFSM fsm;
        results.push_back(runScenario("Vanilla C++ (FuncPtr)", [&]() {
            fsm.dispatch(ev);
            fsm.dispatch(ev);
        }, ITERATIONS));
    }

    // 3. CTHSM
    {
        CthsmInstance instance;
        CompiledCthsm sm;
        sm.start(instance);
        
        cthsm::EventBase evBase{"next"};

        results.push_back(runScenario("cthsm (String Events)", [&]() {
            sm.dispatch(instance, evBase);
            sm.dispatch(instance, evBase);
        }, ITERATIONS));
    }

    // 3b. CTHSM Typed
    {
        CthsmInstance instance;
        CompiledCthsmTyped sm;
        sm.start(instance);
        
        NextEvent evTyped;

        results.push_back(runScenario("cthsm (Typed Events)", [&]() {
            sm.dispatch(instance, evTyped);
            sm.dispatch(instance, evTyped);
        }, ITERATIONS));
    }

    // 5. Boost.SML
    {
        sml::sm<SmlPingPong> sm;
        SmlEvent ev;
        
        results.push_back(runScenario("Boost.SML", [&]() {
            sm.process_event(ev);
            sm.process_event(ev);
        }, ITERATIONS));
    }

    // 6. TinyFSM
    {
        TinyFsmBase::start();
        TinyEvent ev;
        
        results.push_back(runScenario("TinyFSM", [&]() {
            TinyFsmBase::dispatch(ev);
            TinyFsmBase::dispatch(ev);
        }, ITERATIONS));
    }

    // 4. HSM
    {
        HsmInstance instance;
        hsm::start(instance, hsmModel);
        
        hsm::Event event;
        event.name = "next";

        results.push_back(runScenario("hsm (Runtime)", [&]() {
            // hsm dispatch returns a future/observable, need to wait?
            // Looking at benchmark.cpp: instance.dispatch(event).wait();
            // But simpler usage might be possible?
            // The example uses .wait(). This adds overhead for async/future handling.
            // If hsm is async-by-default, that explains performance diffs.
            instance.dispatch(event).wait();
            instance.dispatch(event).wait();
        }, ITERATIONS));
    }

    // Calculate relatives
    double baseline = results[0].transitionsPerSecond; // Switch is usually fastest
    for(auto& res : results) {
        res.relativeSpeed = res.transitionsPerSecond / baseline;
    }

    // Print Table
    std::cout << std::left << std::setw(30) << "Implementation" 
              << std::right << std::setw(20) << "Transitions/Sec" 
              << std::setw(15) << "Relative" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    for(const auto& res : results) {
        std::cout << std::left << std::setw(30) << res.name 
                  << std::right << std::setw(20) << std::fixed << std::setprecision(0) << res.transitionsPerSecond
                  << std::setw(15) << std::setprecision(2) << res.relativeSpeed << "x" << std::endl;
    }

    return 0;
}
