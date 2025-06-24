#!/bin/bash
# Enhanced build script for WinFuzz v2.0

echo "============================================"
echo "WinFuzz v2.0 - Advanced Windows Fuzzer"
echo "============================================"
echo ""

# Check for required tools
echo "Checking prerequisites..."

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found. Please install CMake 3.16+"
    exit 1
fi

# Check CMake version
CMAKE_VERSION=$(cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+')
if [ "$(printf '%s\n' "3.16" "$CMAKE_VERSION" | sort -V | head -n1)" != "3.16" ]; then
    echo "❌ CMake version too old. Please install CMake 3.16+"
    exit 1
fi

echo "✅ CMake found: $CMAKE_VERSION"

# Create build directory
echo "Setting up build environment..."
if [ -d "build" ]; then
    echo "🧹 Cleaning existing build directory..."
    rm -rf build
fi

mkdir build
cd build

echo "✅ Build directory created"

# Configure project
echo "Configuring project..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed"
    exit 1
fi

echo "✅ Project configured"

# Build project
echo "Building WinFuzz..."
cmake --build . --config Release -j$(nproc)

if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

echo "✅ Build completed successfully"

# Check if executable exists
if [ -f "bin/winuzzf.exe" ] || [ -f "bin/Release/winuzzf.exe" ]; then
    echo "✅ WinFuzz executable created"
else
    echo "⚠️  WinFuzz executable not found in expected location"
fi

echo ""
echo "============================================"
echo "🎉 WinFuzz v2.0 build complete!"
echo "============================================"
echo ""
echo "Usage examples:"
echo "  ./bin/winuzzf.exe --help"
echo "  ./bin/winuzzf.exe --examples"
echo "  ./bin/winuzzf.exe --target-api kernel32.dll CreateFileW --corpus corpus"
echo ""
echo "For more information, see README.md"
