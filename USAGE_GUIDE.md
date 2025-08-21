# WinFuzz - Complete Build and Usage Guide

## Overview

WinFuzz is a comprehensive Windows fuzzing framework designed for discovering zero-day vulnerabilities in Windows internals, native binaries, and drivers. It provides a modular, extensible architecture with support for multiple fuzzing techniques and coverage collection methods.

## Prerequisites

### Required Software
1. **Visual Studio 2019/2022** with C++ workload
   - Desktop development with C++
   - Windows 10/11 SDK (latest version)
   - CMake tools for C++

2. **CMake 3.16 or later**
   - Download from https://cmake.org/download/
   - Add to system PATH

3. **Git** (optional, for version control)

### System Requirements
- Windows 10/11 (x64)
- Administrator privileges (for driver fuzzing and ETW)
- Virtual machine recommended for safety

## Building the Project

### Option 1: Using Visual Studio IDE

1. Open Visual Studio 2019/2022
2. Select "Open a local folder" and choose the fuzzing directory
3. Visual Studio will automatically detect CMakeLists.txt
4. Build -> Build All

### Option 2: Using Command Line

1. Open **Developer Command Prompt for VS 2019/2022**
2. Navigate to the project directory:
   ```cmd
   cd c:\Users\range\Desktop\fuzzing
   ```
3. Run the build script:
   ```cmd
   build.bat
   ```

### Option 3: Manual CMake Build

```cmd
cd c:\Users\range\Desktop\fuzzing
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

## Project Structure

```
winuzzf/
├── include/winuzzf.h           # Main public API header
├── src/
│   ├── core/                   # Core fuzzing engine
│   ├── mutators/               # Input mutation strategies  
│   ├── coverage/               # Coverage collection (ETW, Intel PT)
│   ├── targets/                # Target abstraction (API, driver, exe)
│   ├── sandbox/                # Process isolation and monitoring
│   ├── crash/                  # Crash analysis and reporting
│   ├── logging/                # Comprehensive logging system
│   ├── corpus/                 # Corpus management and minimization
│   ├── main.cpp                # Main executable
│   └── utils.cpp               # Utility functions
├── examples/                   # Example programs
├── tools/                      # Utility tools
├── tests/                      # Unit tests
├── config.json                 # Configuration file
├── dictionary.txt              # Fuzzing dictionary (auto-loaded if present)
└── README.md                   # Main documentation
```

## Usage Examples

### 1. API Fuzzing - CreateFileW

Fuzz the Windows CreateFileW API function:

```cmd
winuzzf.exe --target-api kernel32.dll CreateFileW --corpus corpus --crashes crashes --iterations 100000
```

Example programmatic usage:
```cpp
#include "winuzzf.h"

int main() {
    auto fuzzer = WinFuzzer::Create();
    
    // Configure fuzzer
    FuzzConfig config;
    config.max_iterations = 100000;
    config.timeout_ms = 5000;
    config.worker_threads = 8;
    fuzzer->SetConfig(config);
    
    // Create API target
    auto target = std::make_shared<APITarget>("kernel32.dll", "CreateFileW");
    target->SetParameterTemplate({
        sizeof(LPCWSTR),    // lpFileName
        sizeof(DWORD),      // dwDesiredAccess
        sizeof(DWORD),      // dwShareMode
        sizeof(LPSECURITY_ATTRIBUTES), // lpSecurityAttributes
        sizeof(DWORD),      // dwCreationDisposition
        sizeof(DWORD),      // dwFlagsAndAttributes
        sizeof(HANDLE)      // hTemplateFile
    });
    
    fuzzer->SetTarget(target);
    fuzzer->EnableCoverage(CoverageType::ETW_USER);
    fuzzer->Start();
    
    return 0;
}
```

### 2. Driver Fuzzing

Fuzz a Windows driver via IOCTL:

```cmd
winuzzf.exe --target-driver "\\Device\\MyDriver" --ioctl 0x220000 --coverage etw-kernel --threads 4
```

Example code:
```cpp
auto fuzzer = WinFuzzer::Create();
auto target = std::make_shared<DriverTarget>("\\Device\\MyDriver");
target->SetIoctlCode(0x220000);
target->SetInputMethod(true);
target->SetOutputBuffer(4096);

fuzzer->SetTarget(target);
fuzzer->EnableCoverage(CoverageType::ETW_KERNEL);
fuzzer->Start();
```

### 3. Executable Fuzzing

Fuzz a standalone executable:

```cmd
winuzzf.exe --target-exe "C:\Program Files\MyApp\app.exe" --corpus corpus --seed input.dat
```

Example code:
```cpp
auto fuzzer = WinFuzzer::Create();
auto target = std::make_shared<ExecutableTarget>("C:\\test\\app.exe");
target->SetInputMethod("file");
target->SetCommandLineTemplate("%INPUT%");

fuzzer->SetTarget(target);
fuzzer->Start();
```

## Configuration

### Basic Configuration (config.json)

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
        "modules": ["kernel32.dll", "ntdll.dll"]
    },
    "mutations": {
        "strategies": ["random", "dictionary", "havoc"],
        "max_size": 65536,
        "dict_file": "dictionary.txt"
    }
}
```
If `dict_file` is omitted, WinFuzz automatically attempts to load `dictionary.txt` from the current directory.

### Coverage Types

- **ETW_USER**: Event Tracing for Windows (user-mode)
- **ETW_KERNEL**: Event Tracing for Windows (kernel-mode) 
- **Hardware_IntelPT**: Intel Processor Trace (requires supported CPU)
- **None**: No coverage collection

### Mutation Strategies

- **Random**: Random byte-level mutations
- **Deterministic**: Systematic bit flips and arithmetic
- **Dictionary**: Dictionary-based mutations
- **Grammar**: Grammar-based generation (for structured formats)
- **Havoc**: Multiple stacked mutations
- **Splice**: Combine inputs from corpus

## Advanced Features

### Crash Analysis

WinFuzz automatically analyzes crashes and provides:
- Exception code and address
- Call stack and register state
- Module and function information
- Exploitability assessment
- Crash deduplication via hash

### Corpus Management

- Automatic corpus minimization
- Coverage-guided input selection
- Testcase deduplication
- Corpus statistics and analysis

### Process Sandboxing

- Job object isolation
- Memory and time limits
- Crash containment
- Automated cleanup

### Parallel Execution

- Multi-threaded fuzzing
- Lock-free corpus sharing
- Scalable to many CPU cores
- NUMA awareness

## Safety and Best Practices

### Running Safely

1. **Use Virtual Machines**: Always run driver fuzzing in VMs
2. **Take Snapshots**: Create VM snapshots before fuzzing
3. **Monitor Resources**: Watch for memory/disk usage
4. **Regular Backups**: Backup important crash findings

### Performance Tuning

1. **Thread Count**: Usually CPU cores + 2
2. **Timeout Values**: Balance thoroughness vs speed
3. **Corpus Size**: Keep manageable for faster iteration
4. **Coverage Type**: ETW has overhead, consider disabling for speed

### Troubleshooting

#### Common Issues

1. **"Access Denied" errors**
   - Run as Administrator
   - Check Windows Defender exclusions
   - Verify target permissions

2. **ETW not working**
   - Requires Administrator privileges
   - Check Windows version compatibility
   - Verify ETW service is running

3. **Driver crashes system**
   - Use VM with good snapshot support
   - Start with small iteration counts
   - Test IOCTL codes individually

4. **Low execution speed**
   - Reduce timeout values
   - Disable coverage collection temporarily
   - Check for antivirus interference

## Security Considerations

### Responsible Disclosure

If you discover vulnerabilities:
1. Do not publicly disclose immediately
2. Contact the vendor through proper channels
3. Follow coordinated disclosure timelines
4. Document the vulnerability thoroughly

### Legal Compliance

- Only test software you own or have permission to test
- Respect intellectual property rights
- Follow local laws and regulations
- Use for educational and defensive purposes

## Example Workflows

### Basic API Fuzzing Workflow

1. **Identify Target**: Choose Windows API function
2. **Setup Environment**: VM with symbols installed
3. **Create Configuration**: Set parameters and limits
4. **Start Fuzzing**: Begin with small iteration count
5. **Monitor Progress**: Watch for crashes and hangs
6. **Analyze Results**: Examine crash dumps and logs
7. **Scale Up**: Increase iterations if stable

### Driver Fuzzing Workflow

1. **Research Target**: Understand driver functionality
2. **Identify IOCTLs**: Use tools like WinObj, DeviceTree
3. **Setup VM**: Clean snapshot with target driver
4. **Start Conservative**: Low iteration count, short timeout
5. **Monitor System**: Watch for BSODs and hangs
6. **Collect Crashes**: Analyze minidumps
7. **Iterate**: Refine inputs based on results

## Development and Customization

### Adding New Targets

1. Inherit from `Target` base class
2. Implement required virtual methods
3. Handle target-specific setup/cleanup
4. Add to target factory if needed

### Custom Mutation Strategies

1. Add new `MutationStrategy` enum value
2. Implement in `Mutator` class
3. Update mutation selection logic
4. Test with known inputs

### Extending Coverage Collection

1. Inherit from coverage collector base
2. Implement platform-specific hooks
3. Handle coverage data collection
4. Integrate with main fuzzing loop

## Contribution Guidelines

### Code Style
- Follow existing naming conventions
- Use RAII and modern C++ features
- Include comprehensive error handling
- Add unit tests for new functionality

### Testing
- Test on multiple Windows versions
- Verify both x86 and x64 builds
- Include stress testing
- Test edge cases and error conditions

## License

This project is licensed under the MIT License. See LICENSE file for details.

## Disclaimer

This software is provided for educational and research purposes only. Users are responsible for ensuring compliance with applicable laws and regulations. The authors assume no liability for misuse or damage caused by this software.

## Support and Resources

### Documentation
- API Reference: See include/winuzzf.h
- Examples: See examples/ directory
- Configuration: See config.json

### Community
- Report bugs via GitHub issues
- Contribute improvements via pull requests
- Share interesting findings responsibly

### Additional Resources
- Windows Internals books by Russinovich/Solomon
- Windows Driver Kit (WDK) documentation
- ETW documentation on Microsoft Docs
- Intel PT programming guide

---

**Happy Fuzzing! Stay safe and fuzz responsibly!** 🐛🔍
