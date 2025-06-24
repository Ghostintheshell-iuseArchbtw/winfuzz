#include "crash_analyzer.h"
#include <dbghelp.h>
#include <psapi.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <mutex>

// Define EXCEPTION_HEAP_CORRUPTION if not available
#ifndef EXCEPTION_HEAP_CORRUPTION
#define EXCEPTION_HEAP_CORRUPTION 0xC0000374
#endif

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

namespace winuzzf {

class CrashAnalyzer::Impl {
public:
    Impl() : target_process_(nullptr) {
        // Initialize symbol handling
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    }
    
    ~Impl() {
        if (target_process_) {
            SymCleanup(target_process_);
        }
    }
    
    CrashInfo AnalyzeCrash(const std::vector<uint8_t>& input_data) {
        CrashInfo crash_info{};
        crash_info.input_data = input_data;
        
        // Get the last crash info from the exception handler
        CrashInfo* last_crash = ExceptionHandler::GetLastCrashInfo();
        if (last_crash) {
            crash_info = *last_crash;
            crash_info.input_data = input_data;
        }
        
        // Generate crash hash
        crash_info.crash_hash = GenerateCrashHash(crash_info);
        
        // Determine exploitability
        crash_info.exploitable = IsExploitable(crash_info);
        
        return crash_info;
    }
    
    void SetTargetProcess(HANDLE process_handle) {
        target_process_ = process_handle;
        
        if (process_handle) {
            // Initialize symbols for this process
            SymInitialize(process_handle, nullptr, TRUE);
        }
    }
    
    std::string GenerateCrashHash(const CrashInfo& crash_info) {
        // Create a simple hash based on exception code and address
        std::stringstream ss;
        ss << std::hex << crash_info.exception_code << "_" << crash_info.exception_address;
        
        // Add call stack information if available
        if (!crash_info.call_stack.empty()) {
            ss << "_";
            for (size_t i = 0; i < std::min(crash_info.call_stack.size(), static_cast<size_t>(3)); ++i) {
                ss << std::hex << crash_info.call_stack[i];
                if (i < 2) ss << "_";
            }
        }
        
        return ss.str();
    }
    
    bool IsExploitable(const CrashInfo& crash_info) {
        // Basic exploitability heuristics
        switch (crash_info.exception_code) {
            case EXCEPTION_ACCESS_VIOLATION:
                // Check if it's a write to a controlled address
                return crash_info.exception_address < 0x10000 || // NULL pointer dereference range
                       (crash_info.exception_address >= 0x41414141 && crash_info.exception_address <= 0x42424242); // Controlled values
                
            case EXCEPTION_STACK_OVERFLOW:
                return true; // Stack overflows are often exploitable
                
            case EXCEPTION_HEAP_CORRUPTION:
                return true; // Heap corruption is often exploitable
                
            case EXCEPTION_ILLEGAL_INSTRUCTION:
                // Could indicate ROP/JOP
                return true;
                
            default:
                return false;
        }
    }

private:
    HANDLE target_process_;
};

CrashAnalyzer::CrashAnalyzer() : pImpl(std::make_unique<Impl>()) {}
CrashAnalyzer::~CrashAnalyzer() = default;

CrashInfo CrashAnalyzer::AnalyzeCrash(const std::vector<uint8_t>& input_data) {
    return pImpl->AnalyzeCrash(input_data);
}

void CrashAnalyzer::SetTargetProcess(HANDLE process_handle) {
    pImpl->SetTargetProcess(process_handle);
}

std::string CrashAnalyzer::GenerateCrashHash(const CrashInfo& crash_info) {
    return pImpl->GenerateCrashHash(crash_info);
}

bool CrashAnalyzer::IsExploitable(const CrashInfo& crash_info) {
    return pImpl->IsExploitable(crash_info);
}

// Exception Handler Implementation
PVOID ExceptionHandler::handler_handle_ = nullptr;
CrashInfo ExceptionHandler::last_crash_info_{};
std::mutex ExceptionHandler::crash_mutex_;

ExceptionHandler::ExceptionHandler() {}
ExceptionHandler::~ExceptionHandler() {
    Uninstall();
}

void ExceptionHandler::Install() {
    if (handler_handle_ == nullptr) {
        handler_handle_ = AddVectoredExceptionHandler(1, VectoredExceptionHandler);
    }
}

void ExceptionHandler::Uninstall() {
    if (handler_handle_ != nullptr) {
        RemoveVectoredExceptionHandler(handler_handle_);
        handler_handle_ = nullptr;
    }
}

LONG WINAPI ExceptionHandler::VectoredExceptionHandler(PEXCEPTION_POINTERS exception_pointers) {
    if (exception_pointers && exception_pointers->ExceptionRecord) {
        std::lock_guard<std::mutex> lock(crash_mutex_);
        
        EXCEPTION_RECORD* record = exception_pointers->ExceptionRecord;
        CONTEXT* context = exception_pointers->ContextRecord;
        
        // Fill crash info
        last_crash_info_.exception_code = record->ExceptionCode;
        last_crash_info_.exception_address = reinterpret_cast<uint64_t>(record->ExceptionAddress);
        
#ifdef _WIN64
        last_crash_info_.instruction_pointer = context->Rip;
        last_crash_info_.stack_pointer = context->Rsp;
#else
        last_crash_info_.instruction_pointer = context->Eip;
        last_crash_info_.stack_pointer = context->Esp;
#endif
        
        // Get call stack
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        last_crash_info_.call_stack = CrashDumpAnalyzer::GetCallStack(process, thread, context);
        
        // Get module and function names
        last_crash_info_.module_name = CrashDumpAnalyzer::GetModuleNameFromAddress(process, last_crash_info_.exception_address);
        last_crash_info_.function_name = CrashDumpAnalyzer::GetFunctionNameFromAddress(process, last_crash_info_.exception_address);
    }
    
    // Continue search for other handlers
    return EXCEPTION_CONTINUE_SEARCH;
}

CrashInfo* ExceptionHandler::GetLastCrashInfo() {
    std::lock_guard<std::mutex> lock(crash_mutex_);
    return &last_crash_info_;
}

// Crash Dump Analyzer Implementation
CrashInfo CrashDumpAnalyzer::AnalyzeDumpFile(const std::string& dump_path) {
    CrashInfo crash_info{};
    
    // This would require implementing minidump parsing
    // For now, return empty crash info
    // Real implementation would parse the dump file and extract:
    // - Exception information
    // - Register state
    // - Call stack
    // - Module information
    
    return crash_info;
}

void CrashDumpAnalyzer::CreateMiniDump(DWORD process_id, const std::string& dump_path) {
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
    if (process == nullptr) {
        return;
    }
    
    HANDLE dump_file = CreateFileA(
        dump_path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (dump_file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dump_info{};
        dump_info.ThreadId = GetCurrentThreadId();
        dump_info.ExceptionPointers = nullptr; // Would need actual exception pointers
        dump_info.ClientPointers = FALSE;
        
        MiniDumpWriteDump(
            process,
            process_id,
            dump_file,
            MiniDumpNormal,
            &dump_info,
            nullptr,
            nullptr
        );
        
        CloseHandle(dump_file);
    }
    
    CloseHandle(process);
}

std::vector<uint64_t> CrashDumpAnalyzer::GetCallStack(HANDLE process, HANDLE thread, CONTEXT* context) {
    std::vector<uint64_t> call_stack;
    
    STACKFRAME64 stack_frame{};
    DWORD machine_type;
    
#ifdef _WIN64
    machine_type = IMAGE_FILE_MACHINE_AMD64;
    stack_frame.AddrPC.Offset = context->Rip;
    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Offset = context->Rbp;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Offset = context->Rsp;
    stack_frame.AddrStack.Mode = AddrModeFlat;
#else
    machine_type = IMAGE_FILE_MACHINE_I386;
    stack_frame.AddrPC.Offset = context->Eip;
    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Offset = context->Ebp;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Offset = context->Esp;
    stack_frame.AddrStack.Mode = AddrModeFlat;
#endif
    
    for (int i = 0; i < 64; ++i) {
        if (!StackWalk64(
            machine_type,
            process,
            thread,
            &stack_frame,
            context,
            nullptr,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            nullptr)) {
            break;
        }
        
        if (stack_frame.AddrPC.Offset == 0) {
            break;
        }
        
        call_stack.push_back(stack_frame.AddrPC.Offset);
    }
    
    return call_stack;
}

std::string CrashDumpAnalyzer::GetModuleNameFromAddress(HANDLE process, uint64_t address) {
    HMODULE modules[1024];
    DWORD bytes_needed;
    
    if (EnumProcessModules(process, modules, sizeof(modules), &bytes_needed)) {
        for (DWORD i = 0; i < (bytes_needed / sizeof(HMODULE)); ++i) {
            MODULEINFO module_info;
            if (GetModuleInformation(process, modules[i], &module_info, sizeof(module_info))) {
                uint64_t base = reinterpret_cast<uint64_t>(module_info.lpBaseOfDll);
                if (address >= base && address < (base + module_info.SizeOfImage)) {
                    char module_name[MAX_PATH];
                    if (GetModuleBaseNameA(process, modules[i], module_name, sizeof(module_name))) {
                        return std::string(module_name);
                    }
                }
            }
        }
    }
    
    return "unknown";
}

std::string CrashDumpAnalyzer::GetFunctionNameFromAddress(HANDLE process, uint64_t address) {
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    
    DWORD64 displacement = 0;
    if (SymFromAddr(process, address, &displacement, symbol)) {
        return std::string(symbol->Name);
    }
    
    return "unknown";
}

} // namespace winuzzf
