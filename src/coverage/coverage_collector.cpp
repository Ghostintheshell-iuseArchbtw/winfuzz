#include "coverage_collector.h"
#include <iostream>
#include <mutex>
#include <unordered_set>
#include <evntrace.h>
#include <evntcons.h>

#pragma comment(lib, "advapi32.lib")

namespace std {
    template<>
    struct hash<std::pair<uint64_t, uint64_t>> {
        size_t operator()(const std::pair<uint64_t, uint64_t>& p) const {
            return hash<uint64_t>{}(p.first) ^ (hash<uint64_t>{}(p.second) << 1);
        }
    };
}

namespace winuzzf {

class CoverageCollector::Impl {
public:
    Impl() : enabled_(false), type_(CoverageType::None), total_basic_blocks_(0), total_edges_(0) {}
    
    void Initialize(std::shared_ptr<Target> target) {
        target_ = target;
    }
    
    void Enable(CoverageType type) {
        type_ = type;
        enabled_ = true;
        
        switch (type) {
            case CoverageType::ETW_USER:
            case CoverageType::ETW_KERNEL:
                etw_collector_ = std::make_unique<ETWCoverageCollector>();
                break;
            case CoverageType::Hardware_IntelPT:
                intel_pt_collector_ = std::make_unique<IntelPTCoverageCollector>();
                break;
            default:
                // Fallback to simple breakpoint-based coverage
                bp_collector_ = std::make_unique<BreakpointCoverageCollector>();
                if (target_) {
                    bp_collector_->SetTarget(target_);
                }
                break;
        }
    }
    
    void Disable() {
        enabled_ = false;
        type_ = CoverageType::None;
        etw_collector_.reset();
        intel_pt_collector_.reset();
        bp_collector_.reset();
    }
    
    bool IsEnabled() const {
        return enabled_;
    }
    
    void StartCollection() {
        if (!enabled_) return;
        
        switch (type_) {
            case CoverageType::ETW_USER:
                if (etw_collector_) {
                    etw_collector_->StartSession("WinFuzzETWSession");
                }
                break;
            case CoverageType::Hardware_IntelPT:
                if (intel_pt_collector_) {
                    intel_pt_collector_->StartTracing(GetCurrentProcessId());
                }
                break;
            default:
                break;
        }
    }
    
    void StopCollection() {
        if (!enabled_) return;
        
        switch (type_) {
            case CoverageType::ETW_USER:
                if (etw_collector_) {
                    etw_collector_->StopSession();
                }
                break;
            case CoverageType::Hardware_IntelPT:
                if (intel_pt_collector_) {
                    intel_pt_collector_->StopTracing();
                }
                break;
            default:
                break;
        }
    }
    
    CoverageInfo GetCoverageInfo() const {
        if (!enabled_) {
            return CoverageInfo{};
        }
        
        switch (type_) {
            case CoverageType::ETW_USER:
            case CoverageType::ETW_KERNEL:
                if (etw_collector_) {
                    return etw_collector_->GetCoverageInfo();
                }
                break;
            case CoverageType::Hardware_IntelPT:
                if (intel_pt_collector_) {
                    return intel_pt_collector_->GetCoverageInfo();
                }
                break;
            default:
                if (bp_collector_) {
                    return bp_collector_->GetCoverageInfo();
                }
                break;
        }
        
        return CoverageInfo{};
    }
    
    void ResetCoverage() {
        std::lock_guard<std::mutex> lock(mutex_);
        hit_addresses_.clear();
        total_basic_blocks_ = 0;
        total_edges_ = 0;
    }

private:
    bool enabled_;
    CoverageType type_;
    std::shared_ptr<Target> target_;
    std::unique_ptr<ETWCoverageCollector> etw_collector_;
    std::unique_ptr<IntelPTCoverageCollector> intel_pt_collector_;
    std::unique_ptr<BreakpointCoverageCollector> bp_collector_;
    
    mutable std::mutex mutex_;
    std::unordered_set<uint64_t> hit_addresses_;
    uint64_t total_basic_blocks_;
    uint64_t total_edges_;
};

CoverageCollector::CoverageCollector() : pImpl(std::make_unique<Impl>()) {}
CoverageCollector::~CoverageCollector() = default;

void CoverageCollector::Initialize(std::shared_ptr<Target> target) {
    pImpl->Initialize(target);
}

void CoverageCollector::Enable(CoverageType type) {
    pImpl->Enable(type);
}

void CoverageCollector::Disable() {
    pImpl->Disable();
}

bool CoverageCollector::IsEnabled() const {
    return pImpl->IsEnabled();
}

void CoverageCollector::StartCollection() {
    pImpl->StartCollection();
}

void CoverageCollector::StopCollection() {
    pImpl->StopCollection();
}

CoverageInfo CoverageCollector::GetCoverageInfo() const {
    return pImpl->GetCoverageInfo();
}

void CoverageCollector::ResetCoverage() {
    pImpl->ResetCoverage();
}

// ETW Coverage Collector Implementation
class ETWCoverageCollector::Impl {
public:
    Impl() : session_handle_(0), enabled_(false) {}
    
    ~Impl() {
        StopSession();
    }
    
    bool StartSession(const std::string& session_name) {
        // ETW session setup
        session_name_ = session_name;
        
        // Calculate size needed for EVENT_TRACE_PROPERTIES + session name
        ULONG buffer_size = sizeof(EVENT_TRACE_PROPERTIES) + (session_name.length() + 1) * sizeof(WCHAR);
        
        std::vector<uint8_t> buffer(buffer_size);
        EVENT_TRACE_PROPERTIES* properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());
        
        ZeroMemory(properties, buffer_size);
        properties->Wnode.BufferSize = buffer_size;
        properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        properties->Wnode.ClientContext = 1; // Use QueryPerformanceCounter for timestamps
        properties->BufferSize = 1024; // 1MB buffers
        properties->MinimumBuffers = 4;
        properties->MaximumBuffers = 16;
        properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
        properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        
        // Convert session name to wide string
        std::wstring wide_session_name(session_name.begin(), session_name.end());
        wcscpy_s(reinterpret_cast<wchar_t*>(buffer.data() + properties->LoggerNameOffset), 
                session_name.length() + 1, wide_session_name.c_str());
        
        ULONG result = StartTraceW(&session_handle_, wide_session_name.c_str(), properties);
        if (result != ERROR_SUCCESS) {
            std::cerr << "Failed to start ETW session: " << result << std::endl;
            return false;
        }
        
        enabled_ = true;
        return true;
    }
    
    void StopSession() {
        if (enabled_ && session_handle_ != 0) {
            ControlTraceW(session_handle_, nullptr, nullptr, EVENT_TRACE_CONTROL_STOP);
            session_handle_ = 0;
            enabled_ = false;
        }
    }
    
    void EnableProvider(const GUID& provider_guid) {
        if (!enabled_) return;
        
        ULONG result = EnableTraceEx2(
            session_handle_,
            &provider_guid,
            EVENT_CONTROL_CODE_ENABLE_PROVIDER,
            TRACE_LEVEL_VERBOSE,
            0,
            0,
            0,
            nullptr
        );
        
        if (result != ERROR_SUCCESS) {
            std::cerr << "Failed to enable ETW provider: " << result << std::endl;
        }
    }
    
    CoverageInfo GetCoverageInfo() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        CoverageInfo info{};
        info.basic_blocks_hit = hit_addresses_.size();
        info.edges_hit = 0; // ETW doesn't directly provide edge coverage
        info.new_coverage = 0; // Would need to track previous state
        info.hit_addresses = std::vector<uint64_t>(hit_addresses_.begin(), hit_addresses_.end());
        info.coverage_percentage = 0.0; // Would need total BB count
        
        return info;
    }

private:
    std::string session_name_;
    TRACEHANDLE session_handle_;
    bool enabled_;
    mutable std::mutex mutex_;
    std::unordered_set<uint64_t> hit_addresses_;
};

ETWCoverageCollector::ETWCoverageCollector() : pImpl(std::make_unique<Impl>()) {}
ETWCoverageCollector::~ETWCoverageCollector() = default;

bool ETWCoverageCollector::StartSession(const std::string& session_name) {
    return pImpl->StartSession(session_name);
}

void ETWCoverageCollector::StopSession() {
    pImpl->StopSession();
}

void ETWCoverageCollector::EnableProvider(const GUID& provider_guid) {
    pImpl->EnableProvider(provider_guid);
}

CoverageInfo ETWCoverageCollector::GetCoverageInfo() const {
    return pImpl->GetCoverageInfo();
}

// Intel PT Coverage Collector Implementation
class IntelPTCoverageCollector::Impl {
public:
    Impl() : initialized_(false), tracing_(false) {}
    
    bool Initialize() {
        // Check if Intel PT is supported
        int cpuInfo[4];
        __cpuid(cpuInfo, 0x14);
        
        if (cpuInfo[0] == 0) {
            std::cerr << "Intel PT not supported on this processor" << std::endl;
            return false;
        }
        
        initialized_ = true;
        return true;
    }
    
    void StartTracing(DWORD process_id) {
        if (!initialized_) return;
        
        // This is a simplified implementation
        // Real Intel PT integration would require kernel driver or special APIs
        process_id_ = process_id;
        tracing_ = true;
    }
    
    void StopTracing() {
        tracing_ = false;
    }
    
    CoverageInfo GetCoverageInfo() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        CoverageInfo info{};
        info.basic_blocks_hit = hit_addresses_.size();
        info.edges_hit = edges_.size();
        info.new_coverage = 0;
        info.hit_addresses = std::vector<uint64_t>(hit_addresses_.begin(), hit_addresses_.end());
        info.coverage_percentage = 0.0;
        
        return info;
    }

private:
    bool initialized_;
    bool tracing_;
    DWORD process_id_;
    mutable std::mutex mutex_;
    std::unordered_set<uint64_t> hit_addresses_;
    std::unordered_set<uint64_t> edges_; // Simplified to avoid hash issues
};

IntelPTCoverageCollector::IntelPTCoverageCollector() : pImpl(std::make_unique<Impl>()) {}
IntelPTCoverageCollector::~IntelPTCoverageCollector() = default;

bool IntelPTCoverageCollector::Initialize() {
    return pImpl->Initialize();
}

void IntelPTCoverageCollector::StartTracing(DWORD process_id) {
    pImpl->StartTracing(process_id);
}

void IntelPTCoverageCollector::StopTracing() {
    pImpl->StopTracing();
}

CoverageInfo IntelPTCoverageCollector::GetCoverageInfo() const {
    return pImpl->GetCoverageInfo();
}

// Breakpoint Coverage Collector Implementation
BreakpointCoverageCollector::BreakpointCoverageCollector() {}
BreakpointCoverageCollector::~BreakpointCoverageCollector() = default;

void BreakpointCoverageCollector::SetTarget(std::shared_ptr<Target> target) {
    // In a real implementation, you would analyze the target binary
    // and set breakpoints on interesting locations (function entries, basic block starts, etc.)
}

void BreakpointCoverageCollector::AddBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(mutex_);
    breakpoints_.insert(address);
}

void BreakpointCoverageCollector::RemoveBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(mutex_);
    breakpoints_.erase(address);
}

CoverageInfo BreakpointCoverageCollector::GetCoverageInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    CoverageInfo info{};
    info.basic_blocks_hit = hit_addresses_.size();
    info.edges_hit = 0;
    info.new_coverage = 0;
    info.hit_addresses = std::vector<uint64_t>(hit_addresses_.begin(), hit_addresses_.end());
    
    if (!breakpoints_.empty()) {
        info.coverage_percentage = static_cast<double>(hit_addresses_.size()) / breakpoints_.size() * 100.0;
    }
    
    return info;
}

} // namespace winuzzf
