# cthsm: Compile-Time Hierarchical State Machine

`cthsm` is a C++20 library for creating hierarchical state machines (HSMs) where the structure, transitions, and dispatch logic are resolved at compile time. This results in zero-allocation definitions, high performance, and strong type safety.

## Overview

- **Header-only**: `cthsm` is fully template-based.
- **Declarative**: Define your state machine structure using a clean, nested syntax.
- **Hierarchical**: Supports nested states, composite states, and orthogonal regions (via separate instances).
- **Feature-rich**: Includes guards, effects, entry/exit actions, activities, history, and timers.
- **Type-safe**: Events can be strongly typed, and invalid paths or references are often caught at compile time.

## Getting Started

### Minimal Example

```cpp path=examples/minimal.cpp start=null
#include "cthsm/cthsm.hpp"
#include <iostream>

using namespace cthsm;

// Define your instance data
struct MyInstance : public Instance {
    int counter = 0;
};

// Define behaviors
void on_entry(Context&, MyInstance& i, const EventBase&) {
    std::cout << "Entered State A\n";
    i.counter++;
}

// Define the model
constexpr auto model = define("Machine",
    initial(target("StateA")),
    state("StateA",
        entry(on_entry),
        transition(on("next"), target("StateB"))
    ),
    state("StateB",
        transition(on("reset"), target("StateA"))
    )
);

int main() {
    // Compile the machine with your model and instance type
    compile<model, MyInstance> sm;
    MyInstance instance;

    // Start the machine (enters initial state)
    sm.start(instance);
    // Output: "Entered State A"

    // Dispatch events
    sm.dispatch(instance, "next");
    
    std::cout << "Current State: " << sm.state() << "\n"; 
    // Output: "/Machine/StateB"
}
```

## Defining Models

Use `cthsm::define` to create a state machine model. The model is a `constexpr` object that describes the entire structure.

```cpp path=null start=null
constexpr auto model = define("MachineName",
    // Top-level elements
    initial(target("State1")),
    
    state("State1", ...),
    state("State2", ...),
    
    final("Done")
);
```

### States and Hierarchy

States can be nested to create a hierarchy. A composite state (parent) can have its own initial transition and child states.

```cpp path=null start=null
state("Parent",
    initial(target("Child1")), // Default entry for Parent
    
    entry(parent_entry_action),
    
    state("Child1",
        transition(on("event"), target("Child2"))
    ),
    state("Child2",
        transition(on("back"), target("Child1"))
    )
)
```

**Note**: Paths are absolute. The state `Child1` inside `Parent` in `Machine` has the path `/Machine/Parent/Child1`.

### Transitions

Transitions connect states and are triggered by events.

```cpp path=null start=null
transition(
    on("EventName"),        // Trigger
    guard(check_condition), // Optional guard
    target("TargetState"),  // Destination
    effect(do_action)       // Side effect
)
```

- **`on(Name)`**: Specifies the triggering event. Can be a string or a type (see Events).
- **`target(Path)`**: Specifies the destination state.
- **`guard(Fn)`**: A predicate that must return `true` for the transition to be taken.
- **`effect(Fn)`**: An action to execute during the transition.

## Behaviors

Behaviors are C++ functions attached to states or transitions. They must match specific signatures.

### Signatures

```cpp path=null start=null
// Action (Entry, Exit, Effect, Activity)
void action_name(cthsm::Context& ctx, InstanceType& instance, const cthsm::EventBase& event);

// Guard
bool guard_name(cthsm::Context& ctx, InstanceType& instance, const cthsm::EventBase& event);
```

### Types of Behaviors

- **`entry(Actions...)`**: Executed when entering a state.
- **`exit(Actions...)`**: Executed when leaving a state.
- **`effect(Actions...)`**: Executed when a transition is taken (after source exit, before target entry).
- **`activity(Actions...)`**: Long-running tasks started upon entry and cancelled upon exit. Activities run on a separate thread/task via the `TaskProvider`.

## Events

### String Events

The simplest events are string-based.

```cpp path=null start=null
sm.dispatch(instance, "my_event");
```

Match them in the model using `on("my_event")`.

### Typed Events

You can define custom event structs inheriting from `cthsm::Event<T>` or `cthsm::EventBase`.

```cpp path=null start=null
struct DataEvent : cthsm::Event<DataEvent> {
    int payload;
    explicit DataEvent(int p) : payload(p) {}
};

// Usage in model
transition(on<DataEvent>(), effect(handle_data))

// Handler
void handle_data(Context&, MyInstance&, const DataEvent& e) {
    std::cout << "Data: " << e.payload << "\n";
}

// Dispatch
sm.dispatch(instance, DataEvent{42});
```

`cthsm` automatically handles the casting if the handler signature accepts the specific event type.

### Wildcards

Use `cthsm::Any` to match any event not handled by other transitions in the state.

```cpp path=null start=null
transition(on<cthsm::Any>(), target("ErrorState"))
```

### Deferral

Events can be deferred to be processed later (e.g., when in a different state).

```cpp path=null start=null
state("Busy",
    defer("ImportantEvent"), // Will be re-queued when leaving Busy
    // ...
)
```

## Timers

`cthsm` supports time-based transitions.

- **`after(DurationFn)`**: Triggers after a delay.
- **`every(DurationFn)`**: Triggers periodically.
- **`when(PredicateFn)`**: Triggers when a condition becomes true (polled).
- **`at(TimePointFn)`**: Triggers at a specific time.

Timer functions take the standard `(Context&, Instance&, EventBase&)` arguments and return a `std::chrono::duration` or `bool` (for `when`).

```cpp path=null start=null
// Define a duration provider
auto wait_time(Context&, MyInstance&, const EventBase&) {
    return std::chrono::milliseconds(500);
}

// Usage
state("Waiting",
    transition(after(wait_time), target("NextState"))
)
```

## Pseudostates

### Initial

Defines the default child state to enter when a composite state is entered.

```cpp path=null start=null
initial(target("DefaultChild"), effect(init_action))
```

### Choice

Implements dynamic branching based on guards.

```cpp path=null start=null
state("Decide",
    transition(on("go"), target("Branch"))
),
choice("Branch",
    transition(guard(is_valid), target("Success")),
    transition(target("Failure")) // Else/Fallback
)
```

### Final

Represents the completion of a composite state. Reaching a final state triggers a completion event on the parent state.

```cpp path=null start=null
state("Processing",
    state("Step1", transition(on("next"), target("Step2"))),
    state("Step2", transition(on("done"), target("Finished"))),
    final("Finished"),
    
    // Transition on the parent triggers when "Finished" is reached
    transition(target("NextPhase")) 
)
```

### History

Restores the last active state configuration of a composite state.

- **`shallow_history("/Path/To/Composite")`**: Restores the direct child.
- **`deep_history("/Path/To/Composite")`**: Restores the full nested configuration.

```cpp path=null start=null
transition(on("resume"), target(shallow_history("/Machine/Process")))
```

## Runtime API

The `cthsm::compile` class template is the main runtime interface.

```cpp path=null start=null
template <auto Model, typename InstanceType, typename TaskProvider = SequentialTaskProvider>
struct compile { ... };
```

- **`start(instance)`**: Resets and starts the machine.
- **`dispatch(instance, event)`**: Processes an event.
- **`state()`**: Returns the current state path as a `std::string_view`.

### Thread Safety

`cthsm` is designed to be thread-safe when using an appropriate `TaskProvider`. The `Context` object handles synchronization for async activities and timers.
