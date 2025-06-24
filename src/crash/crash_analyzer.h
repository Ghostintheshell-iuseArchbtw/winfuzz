#pragma once

#include "winuzzf.h"
#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

namespace winuzzf {

class CrashAnalyzer {
public:
    CrashAnalyzer();
    ~CrashAnalyzer();
    
    CrashInfo AnalyzeCrash(const std::vector<uint8_t>& input_data);
    void SetTargetProcess(HANDLE process_handle);
    std::string GenerateCrashHash(const CrashInfo& crash_info);
    bool IsExploitable(const CrashInfo& crash_info);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Exception handler for in-process crashes
class ExceptionHandler {
public:
    ExceptionHandler();
    ~ExceptionHandler();
    
    void Install();
    void Uninstall();
    
    static LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS exception_pointers);
    static CrashInfo* GetLastCrashInfo();
    
private:
    static PVOID handler_handle_;
    static CrashInfo last_crash_info_;
    static std::mutex crash_mutex_;
};

// Crash dump analyzer
class CrashDumpAnalyzer {
public:
    static CrashInfo AnalyzeDumpFile(const std::string& dump_path);
    static void CreateMiniDump(DWORD process_id, const std::string& dump_path);
    static std::vector<uint64_t> GetCallStack(HANDLE process, HANDLE thread, CONTEXT* context);
    static std::string GetModuleNameFromAddress(HANDLE process, uint64_t address);
    static std::string GetFunctionNameFromAddress(HANDLE process, uint64_t address);
};

} // namespace winuzzf
