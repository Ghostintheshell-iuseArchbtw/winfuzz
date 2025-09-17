# WinFuzz - Advanced Windows Fuzzing Framework v2.0

A comprehensive, high-performance fuzzing framework specifically designed for Windows platforms. WinFuzz provides coverage-guided fuzzing with advanced instrumentation, intelligent mutation strategies, and a modern Terminal User Interface (TUI).

## 🚀 Key Features

- **Multi-Target Support**: Fuzz APIs, executables, drivers, DLLs, and network services
- **Advanced Coverage Collection**: ETW, Intel PT, and LBR support
- **Intelligent Mutation Strategies**: Random, dictionary-based, havoc, and splice mutations
- **Modern Terminal UI**: Real-time statistics, progress visualization, and colorized output
- **Crash Analysis**: Automatic exploitability assessment and deduplication
- **High Performance**: Multi-threaded execution with optimized corpus management
- **Comprehensive Reporting**: Detailed logs and final analysis reports
- **Starter Corpus & Dictionary**: Sample seeds and extensive dictionary for immediate fuzzing

## 🎯 Quick Start

### Basic API Fuzzing
```bash
winuzzf --target-api kernel32.dll CreateFileW --corpus corpus --crashes crashes
```

### Driver Fuzzing
```bash
winuzzf --target-driver \\.\MyDriver --ioctl 0x220000 --coverage intel-pt
```

### Executable Fuzzing with Dictionary
```bash
winuzzf --target-exe notepad.exe --dict dictionary.txt --seed sample.txt
```

## Architecture

```
winuzzf/
├── core/           # Core fuzzing engine
├── mutators/       # Input mutation strategies
├── coverage/       # Coverage collection and analysis
├── targets/        # Target abstraction layer
├── sandbox/        # Process isolation and monitoring
├── crash/          # Crash analysis and reporting
├── logging/        # Comprehensive logging system
├── corpus/         # Corpus management and minimization
├── examples/       # Example targets and configurations
├── tools/          # Utility tools and scripts
└── tests/          # Unit and integration tests
```

## Building

### Prerequisites
- Visual Studio 2019/2022 with C++ workload
- Windows SDK 10.0.19041.0 or later
- CMake 3.16 or later

### Build Steps
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### Building on Linux or WSL

While WinFuzz's advanced instrumentation is Windows-focused, the project now builds on Linux and WSL with portable stubs so you can explore the codebase, run unit tests, and use the corpus utilities without a Windows SDK.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

> **Note:** Non-Windows builds provide simplified implementations for Windows-specific functionality (sandboxing, ETW coverage, crash analysis, etc.). These stubs are ideal for development and CI but do not offer real instrumentation on Linux.

## Quick Start

### Basic API Fuzzing Example
```cpp
#include "winuzzf.h"

int main() {
    // Create fuzzer instance
    auto fuzzer = WinFuzzer::Create();
    
    // Configure target
    auto target = std::make_shared<APITarget>("kernel32.dll", "CreateFileW");
    fuzzer->SetTarget(target);
    
    // Set up coverage
    fuzzer->EnableCoverage(CoverageType::ETW_KERNEL);
    
    // Start fuzzing
    fuzzer->Start();
    
    return 0;
}
```

### Driver Fuzzing Example
```cpp
#include "winuzzf.h"

int main() {
    auto fuzzer = WinFuzzer::Create();
    
    // Configure driver target
    auto target = std::make_shared<DriverTarget>("\\Device\\MyDriver");
    target->SetIoctlCode(0x220000);
    fuzzer->SetTarget(target);
    
    // Enable kernel coverage
    fuzzer->EnableCoverage(CoverageType::ETW_KERNEL);
    
    // Start fuzzing
    fuzzer->Start();
    
    return 0;
}
```

## Configuration

Create a `config.json` file to customize fuzzing parameters:

```json
{
    "fuzzer": {
        "max_iterations": 1000000,
        "timeout_ms": 5000,
        "worker_threads": 8,
        "corpus_dir": "corpus",
        "crashes_dir": "crashes"
    },
    "coverage": {
        "type": "etw",
        "modules": ["kernel32.dll", "ntdll.dll"],
        "functions": ["NtCreateFile", "NtDeviceIoControlFile"]
    },
    "mutations": {
        "strategies": ["random", "deterministic", "grammar"],
        "max_size": 65536,
        "dict_file": "dictionary.txt"
    }
}
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.
