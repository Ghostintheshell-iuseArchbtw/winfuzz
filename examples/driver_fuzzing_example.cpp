#include "winuzzf.h"
#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>

using namespace winuzzf;

int main() {
    std::cout << "WinFuzz Driver Fuzzing Example" << std::endl;
    
    try {
        // Create fuzzer instance
        auto fuzzer = WinFuzzer::Create();
        
        // Configure fuzzer
        FuzzConfig config;
        config.max_iterations = 100000;
        config.timeout_ms = 2000;
        config.worker_threads = 2; // Use fewer threads for kernel fuzzing
        config.corpus_dir = "corpus_driver";
        config.crashes_dir = "crashes_driver";
        
        fuzzer->SetConfig(config);
        
        // Create driver target
        // Note: Replace with actual driver device name
        auto target = std::make_shared<DriverTarget>("\\\\.\\MyTestDriver");
        
        // Set IOCTL code to fuzz
        // This is a hypothetical IOCTL code - replace with real one
        target->SetIoctlCode(0x220000); // CTL_CODE(0x22, 0, METHOD_BUFFERED, FILE_ANY_ACCESS)
        
        // Configure to use input buffer
        target->SetInputMethod(true);
        
        // Set output buffer size
        target->SetOutputBuffer(4096);
        
        fuzzer->SetTarget(target);
        
        // Enable kernel coverage (requires special setup)
        fuzzer->EnableCoverage(CoverageType::ETW_KERNEL);
        
        // Add mutation strategies suitable for driver fuzzing
        fuzzer->AddMutationStrategy(MutationStrategy::Random);
        fuzzer->AddMutationStrategy(MutationStrategy::Havoc);
        
        // Driver-specific dictionary
        std::vector<std::string> dictionary = {
            // Common sizes and alignments
            "\x00\x00\x00\x00",  // 0
            "\x01\x00\x00\x00",  // 1
            "\x00\x01\x00\x00",  // 256
            "\x00\x10\x00\x00",  // 4096
            "\xFF\xFF\xFF\xFF",  // -1 / MAX_UINT
            "\x00\x00\x00\x80",  // 0x80000000
            
            // Common pointers/handles
            "\x00\x00\x00\x00\x00\x00\x00\x00", // NULL
            "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", // INVALID_HANDLE_VALUE
            
            // Device-specific magic values (examples)
            "MAGIC",
            "\xDE\xAD\xBE\xEF",
            "\xCA\xFE\xBA\xBE"
        };
        
        fuzzer->SetDictionary(dictionary);
        
        // Add seed inputs for common IOCTL patterns
        
        // Seed 1: Simple 4-byte input
        std::vector<uint8_t> seed1 = {0x41, 0x42, 0x43, 0x44};
        fuzzer->AddSeedInput(seed1);
        
        // Seed 2: Structure-like input
        std::vector<uint8_t> seed2 = {
            0x10, 0x00, 0x00, 0x00,  // Size field
            0x01, 0x00, 0x00, 0x00,  // Type field
            0x00, 0x00, 0x00, 0x00,  // Reserved
            0x00, 0x00, 0x00, 0x00,  // Data pointer
            // Followed by data
            0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00 // "Hello"
        };
        fuzzer->AddSeedInput(seed2);
        
        // Seed 3: Large buffer
        std::vector<uint8_t> seed3(1024, 0x41); // 1KB of 'A'
        fuzzer->AddSeedInput(seed3);
        
        // Set up crash callback
        fuzzer->SetCrashCallback([](const CrashInfo& crash) {
            std::cout << "\n*** DRIVER CRASH DETECTED! ***" << std::endl;
            std::cout << "Exception Code: 0x" << std::hex << crash.exception_code << std::endl;
            std::cout << "Exception Address: 0x" << std::hex << crash.exception_address << std::endl;
            std::cout << "Module: " << crash.module_name << std::endl;
            std::cout << "Hash: " << crash.crash_hash << std::endl;
            std::cout << "Exploitable: " << (crash.exploitable ? "YES" : "NO") << std::endl;
            
            // Driver crashes are often more serious
            if (crash.exploitable) {
                std::cout << "WARNING: This may be a kernel privilege escalation vulnerability!" << std::endl;
            }
        });
        
        // Set up progress callback
        fuzzer->SetProgressCallback([](uint64_t iterations, uint64_t crashes) {
            if (iterations % 5000 == 0) {
                std::cout << "Driver fuzzing - Iterations: " << iterations 
                         << ", Crashes: " << crashes << std::endl;
            }
        });
        
        std::cout << "Starting driver IOCTL fuzzing..." << std::endl;
        std::cout << "Target: " << target->GetName() << std::endl;
        std::cout << "WARNING: Driver fuzzing may cause system instability!" << std::endl;
        std::cout << "Make sure you're running in a VM or test environment" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Start fuzzing
        fuzzer->Start();
        
        // Wait for completion
        while (fuzzer->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Print final statistics
        std::cout << "\nDriver fuzzing completed!" << std::endl;
        std::cout << "Total iterations: " << fuzzer->GetIterationCount() << std::endl;
        std::cout << "Total crashes: " << fuzzer->GetCrashCount() << std::endl;
        std::cout << "Total hangs: " << fuzzer->GetHangCount() << std::endl;
        std::cout << "Execution rate: " << fuzzer->GetExecutionsPerSecond() << " exec/sec" << std::endl;
        
        auto coverage_info = fuzzer->GetCoverageInfo();
        if (coverage_info.basic_blocks_hit > 0) {
            std::cout << "Code coverage: " << coverage_info.basic_blocks_hit << " basic blocks" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Note: Driver fuzzing requires the target driver to be installed and accessible" << std::endl;
        return 1;
    }
    
    return 0;
}
