#!/bin/bash

# Build script for WinFuzz (Linux/WSL)
echo "Building WinFuzz..."

# Check if required tools are available
if ! command -v cmake &> /dev/null; then
    echo "CMake not found. Please install CMake."
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo "Make not found. Please install build tools."
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release
if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

# Build the project
echo "Building project..."
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo ""
echo "Build completed successfully!"
echo ""
echo "Executables are in: build/bin/"
echo ""
echo "To run examples:"
echo "  cd build/bin"
echo "  ./api_fuzzing_example"
echo "  ./driver_fuzzing_example"
echo ""
echo "To run main fuzzer:"
echo "  ./winuzzf --help"
echo ""
