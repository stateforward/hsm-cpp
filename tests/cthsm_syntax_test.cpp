// Test demonstrating identical syntax between hsm.hpp and cthsm.hpp
// The same state machine definition works with both implementations

#include <iostream>
#include <chrono>

// Toggle between implementations by changing this
#define USE_CTHSM 1

#if USE_CTHSM
  #include "cthsm.hpp"
  namespace hsm_impl = cthsm;
  #define COMPILE_TIME_OPTIMIZED
#else
  #include "hsm.hpp"  
  namespace hsm_impl = hsm;
#endif

// Custom instance type
struct LightInstance : hsm_impl::Instance {
  int brightness = 0;
  bool motion_detected = false;
};

// Actions - exact same syntax for both implementations
void on_enter(hsm_impl::Context& ctx, LightInstance& inst, hsm_impl::Event& event) {
  inst.brightness = 100;
  std::cout << "Light turned on\n";
}

void on_exit(hsm_impl::Context& ctx, LightInstance& inst, hsm_impl::Event& event) {
  inst.brightness = 0;
  std::cout << "Light turned off\n";
}

void dim_light(hsm_impl::Context& ctx, LightInstance& inst, hsm_impl::Event& event) {
  inst.brightness = 50;
  std::cout << "Light dimmed to 50%\n";
}

bool is_motion_detected(hsm_impl::Context& ctx, LightInstance& inst, hsm_impl::Event& event) {
  return inst.motion_detected;
}

auto timeout_duration(hsm_impl::Context& ctx, LightInstance& inst, hsm_impl::Event& event) {
  return std::chrono::seconds(5);
}

int main() {
  // Define state machine - EXACT same syntax for both hsm and cthsm
  auto model = hsm_impl::define("LightController",
    hsm_impl::initial(hsm_impl::target("off")),
    
    hsm_impl::state("off",
      hsm_impl::transition(
        hsm_impl::on("turn_on"),
        hsm_impl::target("on")
      ),
      hsm_impl::transition(
        hsm_impl::on("motion"),
        hsm_impl::guard(is_motion_detected),
        hsm_impl::target("on")
      )
    ),
    
    hsm_impl::state("on",
      hsm_impl::entry(on_enter),
      hsm_impl::exit(on_exit),
      
      hsm_impl::transition(
        hsm_impl::on("turn_off"),
        hsm_impl::target("off")
      ),
      
      hsm_impl::transition(
        hsm_impl::on("dim"),
        hsm_impl::effect(dim_light),
        hsm_impl::target("dimmed")
      ),
      
      hsm_impl::transition(
        hsm_impl::after<std::chrono::seconds>(timeout_duration),
        hsm_impl::target("off")
      ),
      
      hsm_impl::defer("emergency_stop")
    ),
    
    hsm_impl::state("dimmed",
      hsm_impl::transition(
        hsm_impl::on("brighten"),
        hsm_impl::target("on")
      ),
      hsm_impl::transition(
        hsm_impl::on("turn_off"),
        hsm_impl::target("off")
      )
    ),
    
    hsm_impl::choice("power_save_check",
      hsm_impl::transition(
        hsm_impl::guard([](auto& ctx, LightInstance& inst, auto& event) {
          return inst.brightness > 75;
        }),
        hsm_impl::target("dimmed")
      ),
      hsm_impl::transition(hsm_impl::target("on"))  // Default
    ),
    
    hsm_impl::final("shutdown")
  );

#ifdef COMPILE_TIME_OPTIMIZED
  // For cthsm, create compile-time optimized state machine
  hsm_impl::compile<decltype(model), LightInstance> sm;
  std::cout << "Using compile-time optimized HSM (cthsm)\n";
#else
  // For hsm, create runtime state machine
  LightInstance instance;
  hsm_impl::start(instance, model);
  auto& sm = instance;
  std::cout << "Using runtime HSM (hsm)\n";
#endif

  // Usage is identical regardless of implementation
  std::cout << "Initial state: " << sm.state() << "\n";
  
  // Test transitions
  sm.dispatch(hsm_impl::Event("turn_on"));
  std::cout << "State after turn_on: " << sm.state() << "\n";
  
  sm.dispatch(hsm_impl::Event("dim"));
  std::cout << "State after dim: " << sm.state() << "\n";
  
  sm.dispatch(hsm_impl::Event("turn_off"));
  std::cout << "Final state: " << sm.state() << "\n";

  return 0;
}

/*
Key points demonstrated:
1. State machine definition syntax is IDENTICAL between hsm and cthsm
2. All DSL functions work the same: state, transition, entry, exit, effect, guard, etc.
3. Time-based transitions (after/every) have the same syntax
4. Lambda functions, function pointers, and custom instance types work the same
5. The only difference is the final instantiation:
   - hsm: Uses runtime model with start()
   - cthsm: Uses compile<> template for compile-time optimization
   
By maintaining exact API compatibility, users can easily switch between 
runtime flexibility (hsm) and compile-time performance (cthsm) by just
changing the namespace.
*/