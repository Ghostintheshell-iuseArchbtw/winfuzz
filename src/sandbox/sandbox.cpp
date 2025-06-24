#include "sandbox.h"
#include <iostream>

namespace winuzzf {

class Sandbox::Impl {
public:
    Impl() : initialized_(false) {}
    
    bool Initialize() {
        if (initialized_) return true;
        
        // Create default job object for sandboxing
        job_sandbox_ = std::make_unique<JobObjectSandbox>();
        if (!job_sandbox_->Create("WinFuzzSandbox")) {
            return false;
        }
        
        initialized_ = true;
        return true;
    }
    
    void Cleanup() {
        if (job_sandbox_) {
            job_sandbox_->Terminate();
            job_sandbox_.reset();
        }
        initialized_ = false;
    }
    
    HANDLE CreateSandboxedProcess(const std::string& command_line) {
        if (!initialized_) return nullptr;
        
        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        
        // Create suspended process
        BOOL result = CreateProcessA(
            nullptr,
            const_cast<char*>(command_line.c_str()),
            nullptr,
            nullptr,
            FALSE,
            CREATE_SUSPENDED | CREATE_NEW_CONSOLE,
            nullptr,
            nullptr,
            &si,
            &pi
        );
        
        if (!result) {
            return nullptr;
        }
        
        // Assign to job object
        if (!job_sandbox_->AssignProcess(pi.hProcess)) {
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return nullptr;
        }
        
        // Enable security features
        EnableDEP(pi.hProcess);
        EnableASLR(pi.hProcess);
        
        // Resume main thread
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        
        return pi.hProcess;
    }
    
    bool TerminateProcess(HANDLE process_handle, DWORD timeout_ms) {
        if (!process_handle || process_handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        // Try graceful termination first
        if (::TerminateProcess(process_handle, 1)) {
            // Wait for process to exit
            DWORD wait_result = WaitForSingleObject(process_handle, timeout_ms);
            return wait_result == WAIT_OBJECT_0;
        }
        
        return false;
    }
    
    bool IsProcessAlive(HANDLE process_handle) {
        if (!process_handle || process_handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        DWORD exit_code;
        if (GetExitCodeProcess(process_handle, &exit_code)) {
            return exit_code == STILL_ACTIVE;
        }
        
        return false;
    }
    
    void EnableDEP(HANDLE process_handle) {
        // DEP is usually enabled by default on modern Windows
        // This is a placeholder for explicit DEP configuration
        
        typedef BOOL (WINAPI *SetProcessDEPPolicyProc)(DWORD dwFlags);
        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
        if (kernel32) {
            SetProcessDEPPolicyProc SetProcessDEPPolicy = 
                reinterpret_cast<SetProcessDEPPolicyProc>(GetProcAddress(kernel32, "SetProcessDEPPolicy"));
            if (SetProcessDEPPolicy) {
                SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
            }
        }
    }
    
    void EnableASLR(HANDLE process_handle) {
        // ASLR is enabled by linker flags and system settings
        // This is a placeholder for runtime ASLR configuration
    }
    
    void SetHeapFlags(HANDLE process_handle, DWORD flags) {
        // Set heap flags for the target process
        // This would require more advanced process manipulation
    }
    
    void SetMemoryLimit(HANDLE process_handle, SIZE_T limit_bytes) {
        if (job_sandbox_) {
            job_sandbox_->SetLimits(limit_bytes, 0);
        }
    }
    
    void SetTimeLimit(HANDLE process_handle, DWORD limit_ms) {
        if (job_sandbox_) {
            job_sandbox_->SetLimits(0, limit_ms);
        }
    }
    
    void SetCpuLimit(HANDLE process_handle, DWORD percentage) {
        // CPU limiting would be implemented through job objects
        // or process priority manipulation
    }

private:
    bool initialized_;
    std::unique_ptr<JobObjectSandbox> job_sandbox_;
};

Sandbox::Sandbox() : pImpl(std::make_unique<Impl>()) {}
Sandbox::~Sandbox() = default;

bool Sandbox::Initialize() {
    return pImpl->Initialize();
}

void Sandbox::Cleanup() {
    pImpl->Cleanup();
}

HANDLE Sandbox::CreateSandboxedProcess(const std::string& command_line) {
    return pImpl->CreateSandboxedProcess(command_line);
}

bool Sandbox::TerminateProcess(HANDLE process_handle, DWORD timeout_ms) {
    return pImpl->TerminateProcess(process_handle, timeout_ms);
}

bool Sandbox::IsProcessAlive(HANDLE process_handle) {
    return pImpl->IsProcessAlive(process_handle);
}

void Sandbox::EnableDEP(HANDLE process_handle) {
    pImpl->EnableDEP(process_handle);
}

void Sandbox::EnableASLR(HANDLE process_handle) {
    pImpl->EnableASLR(process_handle);
}

void Sandbox::SetHeapFlags(HANDLE process_handle, DWORD flags) {
    pImpl->SetHeapFlags(process_handle, flags);
}

void Sandbox::SetMemoryLimit(HANDLE process_handle, SIZE_T limit_bytes) {
    pImpl->SetMemoryLimit(process_handle, limit_bytes);
}

void Sandbox::SetTimeLimit(HANDLE process_handle, DWORD limit_ms) {
    pImpl->SetTimeLimit(process_handle, limit_ms);
}

void Sandbox::SetCpuLimit(HANDLE process_handle, DWORD percentage) {
    pImpl->SetCpuLimit(process_handle, percentage);
}

// Job Object Sandbox Implementation
JobObjectSandbox::JobObjectSandbox() : job_handle_(nullptr) {}

JobObjectSandbox::~JobObjectSandbox() {
    Terminate();
}

bool JobObjectSandbox::Create(const std::string& job_name) {
    job_name_ = job_name;
    
    job_handle_ = CreateJobObjectA(nullptr, job_name.c_str());
    if (!job_handle_) {
        std::cerr << "Failed to create job object: " << GetLastError() << std::endl;
        return false;
    }
    
    // Set basic limits
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits = {};
    job_limits.BasicLimitInformation.LimitFlags = 
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
        JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    
    if (!SetInformationJobObject(job_handle_, JobObjectExtendedLimitInformation, 
                                &job_limits, sizeof(job_limits))) {
        std::cerr << "Failed to set job object limits: " << GetLastError() << std::endl;
        return false;
    }
    
    return true;
}

bool JobObjectSandbox::AssignProcess(HANDLE process_handle) {
    if (!job_handle_ || !process_handle) {
        return false;
    }
    
    return AssignProcessToJobObject(job_handle_, process_handle) != FALSE;
}

void JobObjectSandbox::SetLimits(SIZE_T memory_limit, DWORD time_limit) {
    if (!job_handle_) return;
    
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits = {};
    DWORD limit_flags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | 
                       JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    
    if (memory_limit > 0) {
        job_limits.ProcessMemoryLimit = memory_limit;
        limit_flags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    }
    
    if (time_limit > 0) {
        job_limits.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = 
            static_cast<LONGLONG>(time_limit) * 10000; // Convert ms to 100ns units
        limit_flags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
    }
    
    job_limits.BasicLimitInformation.LimitFlags = limit_flags;
    
    SetInformationJobObject(job_handle_, JobObjectExtendedLimitInformation, 
                           &job_limits, sizeof(job_limits));
}

void JobObjectSandbox::Terminate() {
    if (job_handle_) {
        TerminateJobObject(job_handle_, 1);
        CloseHandle(job_handle_);
        job_handle_ = nullptr;
    }
}

} // namespace winuzzf
