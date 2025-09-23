#!/bin/bash

# Build and test the compile-time HSM implementation

echo "Building C++ tests with cthsm..."

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the cthsm tests
echo "Building cthsm tests..."
make test_cthsm_execution
make cthsm_syntax_test

# Run the tests
echo -e "\n=== Running cthsm execution test ==="
./tests/test_cthsm_execution

echo -e "\n=== Running cthsm syntax compatibility test ==="
./tests/cthsm_syntax_test

echo -e "\nDone!"