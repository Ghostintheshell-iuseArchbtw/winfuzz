#pragma once

#include "winuzzf.h"
#include <windows.h>
#include <memory>
#include <string>

namespace winuzzf {

class Sandbox {
public:
    Sandbox();
    ~Sandbox();
    
    bool Initialize();
    void Cleanup();
    
    // Process isolation
    HANDLE CreateSandboxedProcess(const std::string& command_line);
    bool TerminateProcess(HANDLE process_handle, DWORD timeout_ms = 5000);
    bool IsProcessAlive(HANDLE process_handle);
    
    // Memory protection
    void EnableDEP(HANDLE process_handle);
    void EnableASLR(HANDLE process_handle);
    void SetHeapFlags(HANDLE process_handle, DWORD flags);
    
    // Resource limits
    void SetMemoryLimit(HANDLE process_handle, SIZE_T limit_bytes);
    void SetTimeLimit(HANDLE process_handle, DWORD limit_ms);
    void SetCpuLimit(HANDLE process_handle, DWORD percentage);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Job object wrapper for process sandboxing
class JobObjectSandbox {
public:
    JobObjectSandbox();
    ~JobObjectSandbox();
    
    bool Create(const std::string& job_name);
    bool AssignProcess(HANDLE process_handle);
    void SetLimits(SIZE_T memory_limit, DWORD time_limit);
    void Terminate();
    
private:
    HANDLE job_handle_;
    std::string job_name_;
};

} // namespace winuzzf
