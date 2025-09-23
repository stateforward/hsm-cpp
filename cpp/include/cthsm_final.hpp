#pragma once

#include "cthsm.hpp"
#include <string_view>

namespace cthsm {

// The stable CTHSM implementation is in cthsm.hpp
// This header provides clean state comparison functionality

// Helper class for state comparison
class StateComparator {
private:
    uint32_t hash_value_;
    uint32_t model_hash_;
    
public:
    StateComparator(std::string_view state_str, uint32_t model_hash = 0) 
        : model_hash_(model_hash) {
        // Parse hash from state string
        hash_value_ = 0;
        for (char c : state_str) {
            if (c >= '0' && c <= '9') {
                hash_value_ = hash_value_ * 10 + (c - '0');
            }
        }
    }
    
    bool matches(std::string_view state_name) const {
        // Direct hash comparison
        if (hash_value_ == hash(state_name)) {
            return true;
        }
        
        // Try with model prefix
        if (model_hash_ != 0) {
            uint32_t full_hash = combine_hashes(model_hash_, hash(state_name));
            if (hash_value_ == full_hash) {
                return true;
            }
        }
        
        return false;
    }
};

// Helper function for clean state comparison
// Note: We need the model name to properly match states
template<typename SM>
bool check_state(const SM& sm, std::string_view state_name, std::string_view model_name) {
    StateComparator comp(sm.state(), hash(model_name));
    return comp.matches(state_name);
}

// Overload that tries to extract model name from the state machine type
template<typename Model, typename T>
bool check_state(const StateMachine<Model, T>& sm, std::string_view state_name) {
    // Extract model name from the Model type if available
    if constexpr (requires { Model{}.name; }) {
        return check_state(sm, state_name, Model{}.name);
    } else {
        // Fallback - just check without model prefix
        StateComparator comp(sm.state(), 0);
        return comp.matches(state_name);
    }
}

// Macro for even cleaner syntax (optional)
#define IS_STATE(sm, name) check_state(sm, name)

} // namespace cthsm