#include "winuzzf.h"
#include <chrono>
#include <iostream>
#include <thread>
#ifdef _WIN32
#    include <windows.h>
#endif

using namespace winuzzf;

int main() {
#ifndef _WIN32
    std::cout << "WinFuzz API example is only available on Windows." << std::endl;
    return 0;
#else
    std::cout << "WinFuzz API Fuzzing Example - CreateFileW" << std::endl;

    try {
        // Create fuzzer instance
        auto fuzzer = WinFuzzer::Create();
        
        // Configure fuzzer
        FuzzConfig config;
        config.max_iterations = 10000;
        config.timeout_ms = 1000;
        config.worker_threads = 4;
        config.corpus_dir = "corpus_createfile";
        config.crashes_dir = "crashes_createfile";
        
        fuzzer->SetConfig(config);
        
        // Create API target for CreateFileW
        auto target = std::make_shared<APITarget>("kernel32.dll", "CreateFileW");
        
        // Set parameter template for CreateFileW
        target->SetParameterTemplate({
            sizeof(LPCWSTR),    // lpFileName
            sizeof(DWORD),      // dwDesiredAccess  
            sizeof(DWORD),      // dwShareMode
            sizeof(LPSECURITY_ATTRIBUTES), // lpSecurityAttributes
            sizeof(DWORD),      // dwCreationDisposition
            sizeof(DWORD),      // dwFlagsAndAttributes
            sizeof(HANDLE)      // hTemplateFile
        });
        
        // Set return value checker (optional)
        target->SetReturnValueCheck([](DWORD retval) {
            // Consider INVALID_HANDLE_VALUE as normal failure, not crash
            return retval != (DWORD)INVALID_HANDLE_VALUE;
        });
        
        fuzzer->SetTarget(target);
        
        // Enable coverage collection
        fuzzer->EnableCoverage(CoverageType::ETW_USER);
        
        // Add mutation strategies
        fuzzer->AddMutationStrategy(MutationStrategy::Random);
        fuzzer->AddMutationStrategy(MutationStrategy::Dictionary);
        
        // Set up dictionary with common file-related strings
        std::vector<std::string> dictionary = {
            "C:\\",
            "CON",
            "PRN", 
            "AUX",
            "NUL",
            "COM1",
            "LPT1",
            "\\\\?\\",
            "\\\\?\\C:\\",
            "\\\\?\\UNC\\",
            "\\\\localhost\\",
            "file.txt",
            "test.dat",
            "..",
            "..\\",
            "\\",
            "/",
            "*",
            "?",
            "<",
            ">",
            "|",
            "\"",
            "\x00",
            "\xFF"
        };
        
        fuzzer->SetDictionary(dictionary);
        
        // Add some seed inputs
        std::vector<uint8_t> seed1 = {
            'C', 0, ':', 0, '\\', 0, 't', 0, 'e', 0, 's', 0, 't', 0, '.', 0, 't', 0, 'x', 0, 't', 0, 0, 0,  // "C:\test.txt"
            0x80, 0x00, 0x00, 0x00,  // GENERIC_READ
            0x01, 0x00, 0x00, 0x00,  // FILE_SHARE_READ
            0x00, 0x00, 0x00, 0x00,  // NULL security attributes
            0x03, 0x00, 0x00, 0x00,  // OPEN_EXISTING
            0x80, 0x00, 0x00, 0x00,  // FILE_ATTRIBUTE_NORMAL
            0x00, 0x00, 0x00, 0x00   // NULL template file
        };
        
        fuzzer->AddSeedInput(seed1);
        
        // Set up crash callback
        fuzzer->SetCrashCallback([](const CrashInfo& crash) {
            std::cout << "\n*** CRASH FOUND! ***" << std::endl;
            std::cout << "Exception Code: 0x" << std::hex << crash.exception_code << std::endl;
            std::cout << "Exception Address: 0x" << std::hex << crash.exception_address << std::endl;
            std::cout << "Module: " << crash.module_name << std::endl;
            std::cout << "Exploitable: " << (crash.exploitable ? "YES" : "NO") << std::endl;
        });
        
        // Set up progress callback
        fuzzer->SetProgressCallback([](uint64_t iterations, uint64_t crashes) {
            if (iterations % 1000 == 0) {
                std::cout << "Iterations: " << iterations << ", Crashes: " << crashes << std::endl;
            }
        });
        
        std::cout << "Starting CreateFileW fuzzing..." << std::endl;
        std::cout << "This will fuzz the CreateFileW API with various filename inputs" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Start fuzzing
        fuzzer->Start();
        
        // Wait for completion
        while (fuzzer->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Print final statistics
        std::cout << "\nFuzzing completed!" << std::endl;
        std::cout << "Total iterations: " << fuzzer->GetIterationCount() << std::endl;
        std::cout << "Total crashes: " << fuzzer->GetCrashCount() << std::endl;
        std::cout << "Execution rate: " << fuzzer->GetExecutionsPerSecond() << " exec/sec" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
#endif
}
