#pragma once

#include "winuzzf.h"
#include <memory>
#include <mutex>
#include <unordered_set>
#ifdef _WIN32
#    include <windows.h>
#else
struct GUID {
    unsigned long Data1 = 0;
    unsigned short Data2 = 0;
    unsigned short Data3 = 0;
    unsigned char Data4[8] = {0};
};
#endif

namespace winuzzf {

class CoverageCollector {
public:
    CoverageCollector();
    ~CoverageCollector();
    
    void Initialize(std::shared_ptr<Target> target);
    void Enable(CoverageType type);
    void Disable();
    bool IsEnabled() const;
    
    void StartCollection();
    void StopCollection();
    CoverageInfo GetCoverageInfo() const;
    void ResetCoverage();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// ETW-based coverage collector
class ETWCoverageCollector {
public:
    ETWCoverageCollector();
    ~ETWCoverageCollector();
    
    bool StartSession(const std::string& session_name);
    void StopSession();
    void EnableProvider(const GUID& provider_guid);
    CoverageInfo GetCoverageInfo() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Hardware-based coverage collector using Intel PT
class IntelPTCoverageCollector {
public:
    IntelPTCoverageCollector();
    ~IntelPTCoverageCollector();
    
    bool Initialize();
    void StartTracing(DWORD process_id);
    void StopTracing();
    CoverageInfo GetCoverageInfo() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Simple breakpoint-based coverage
class BreakpointCoverageCollector {
public:
    BreakpointCoverageCollector();
    ~BreakpointCoverageCollector();
    
    void SetTarget(std::shared_ptr<Target> target);
    void AddBreakpoint(uint64_t address);
    void RemoveBreakpoint(uint64_t address);
    CoverageInfo GetCoverageInfo() const;
    
private:
    std::unordered_set<uint64_t> breakpoints_;
    std::unordered_set<uint64_t> hit_addresses_;
    mutable std::mutex mutex_;
};

} // namespace winuzzf
