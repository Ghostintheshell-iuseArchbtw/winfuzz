#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <windows.h>

namespace winuzzf {

// Forward declarations
class Target;
class Mutator;
class CoverageCollector;
class Sandbox;
class CrashAnalyzer;
class Logger;
class CorpusManager;

// Fuzzing result
enum class FuzzResult {
    Success,
    Crash,
    Hang,
    Error,
    NoNewCoverage
};

// Coverage types
enum class CoverageType {
    None,
    ETW_USER,
    ETW_KERNEL,
    Hardware_IntelPT,
    Hardware_LBR,
    DynamoRIO
};

// Mutation strategies
enum class MutationStrategy {
    Random,
    Deterministic,
    Grammar,
    Dictionary,
    Havoc,
    Splice
};

// Target types
enum class TargetType {
    API_Function,
    Executable,
    DLL_Export,
    Driver_IOCTL,
    Network_Socket,
    File_Parser,
    Registry_Key
};

// Architecture
enum class Architecture {
    x86,
    x64,
    ARM,
    ARM64
};

// Fuzzing configuration
struct FuzzConfig {
    uint64_t max_iterations = 1000000;
    uint32_t timeout_ms = 5000;
    uint32_t worker_threads = 8;
    uint32_t max_input_size = 65536;
    std::string corpus_dir = "corpus";
    std::string crashes_dir = "crashes";
    std::string logs_dir = "logs";
    bool minimize_corpus = true;
    bool deduplicate_crashes = true;
    bool collect_coverage = true;
    CoverageType coverage_type = CoverageType::ETW_USER;
};

// Crash information
struct CrashInfo {
    uint32_t exception_code;
    uint64_t exception_address;
    uint64_t instruction_pointer;
    uint64_t stack_pointer;
    std::vector<uint64_t> call_stack;
    std::string crash_hash;
    std::vector<uint8_t> input_data;
    std::string module_name;
    std::string function_name;
    bool exploitable;
};

// Coverage information
struct CoverageInfo {
    uint64_t basic_blocks_hit;
    uint64_t edges_hit;
    uint64_t new_coverage;
    std::vector<uint64_t> hit_addresses;
    double coverage_percentage;
};

// Callbacks
using CrashCallback = std::function<void(const CrashInfo&)>;
using CoverageCallback = std::function<void(const CoverageInfo&)>;
using ProgressCallback = std::function<void(uint64_t iterations, uint64_t crashes)>;

// Main fuzzer class
class WinFuzzer {
public:
    static std::unique_ptr<WinFuzzer> Create();
    virtual ~WinFuzzer() = default;

    // Configuration
    virtual void SetConfig(const FuzzConfig& config) = 0;
    virtual const FuzzConfig& GetConfig() const = 0;

    // Target management
    virtual void SetTarget(std::shared_ptr<Target> target) = 0;
    virtual std::shared_ptr<Target> GetTarget() const = 0;

    // Coverage
    virtual void EnableCoverage(CoverageType type) = 0;
    virtual void DisableCoverage() = 0;
    virtual CoverageInfo GetCoverageInfo() const = 0;

    // Corpus management
    virtual void AddSeedInput(const std::vector<uint8_t>& input) = 0;
    virtual void LoadCorpusFromDirectory(const std::string& dir) = 0;
    virtual void SaveCorpusToDirectory(const std::string& dir) = 0;

    // Mutation
    virtual void AddMutationStrategy(MutationStrategy strategy) = 0;
    virtual void SetDictionary(const std::vector<std::string>& dict) = 0;

    // Callbacks
    virtual void SetCrashCallback(CrashCallback callback) = 0;
    virtual void SetCoverageCallback(CoverageCallback callback) = 0;
    virtual void SetProgressCallback(ProgressCallback callback) = 0;

    // Fuzzing control
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual bool IsRunning() const = 0;

    // Statistics
    virtual uint64_t GetIterationCount() const = 0;
    virtual uint64_t GetCrashCount() const = 0;
    virtual uint64_t GetHangCount() const = 0;
    virtual uint64_t GetCorpusSize() const = 0;
    virtual double GetExecutionsPerSecond() const = 0;
};

// Base target class
class Target {
public:
    virtual ~Target() = default;
    virtual TargetType GetType() const = 0;
    virtual Architecture GetArchitecture() const = 0;
    virtual FuzzResult Execute(const std::vector<uint8_t>& input) = 0;
    virtual void Setup() = 0;
    virtual void Cleanup() = 0;
    virtual std::string GetName() const = 0;
};

// API function target
class APITarget : public Target {
public:    APITarget(const std::string& module, const std::string& function);
    virtual ~APITarget();
    
    TargetType GetType() const override { return TargetType::API_Function; }
    FuzzResult Execute(const std::vector<uint8_t>& input) override;
    void Setup() override;
    void Cleanup() override;
    std::string GetName() const override;
    Architecture GetArchitecture() const override;

    // API-specific methods
    void SetParameterTemplate(const std::vector<uint32_t>& param_sizes);
    void SetReturnValueCheck(std::function<bool(DWORD)> checker);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Driver IOCTL target
class DriverTarget : public Target {
public:    DriverTarget(const std::string& device_name);
    virtual ~DriverTarget();
    
    TargetType GetType() const override { return TargetType::Driver_IOCTL; }
    FuzzResult Execute(const std::vector<uint8_t>& input) override;
    void Setup() override;
    void Cleanup() override;
    std::string GetName() const override;
    Architecture GetArchitecture() const override;

    // Driver-specific methods
    void SetIoctlCode(uint32_t ioctl_code);
    void SetInputMethod(bool use_input_buffer);
    void SetOutputBuffer(size_t size);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Executable target
class ExecutableTarget : public Target {
public:    ExecutableTarget(const std::string& exe_path);
    virtual ~ExecutableTarget();
    
    TargetType GetType() const override { return TargetType::Executable; }
    FuzzResult Execute(const std::vector<uint8_t>& input) override;
    void Setup() override;
    void Cleanup() override;
    std::string GetName() const override;
    Architecture GetArchitecture() const override;

    // Executable-specific methods
    void SetCommandLineTemplate(const std::string& template_str);
    void SetInputMethod(const std::string& method); // "stdin", "file", "cmdline"
    void SetWorkingDirectory(const std::string& dir);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// DLL export target
class DLLTarget : public Target {
public:
    DLLTarget(const std::string& dll_path, const std::string& function_name);
    virtual ~DLLTarget();
    
    TargetType GetType() const override { return TargetType::DLL_Export; }
    FuzzResult Execute(const std::vector<uint8_t>& input) override;
    void Setup() override;
    void Cleanup() override;
    std::string GetName() const override;
    Architecture GetArchitecture() const override;

    // DLL-specific methods
    void SetParameterTemplate(const std::vector<uint32_t>& param_sizes);
    void SetCallingConvention(const std::string& convention);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Network target
class NetworkTarget : public Target {
public:
    NetworkTarget(const std::string& address_port);
    virtual ~NetworkTarget();
    
    TargetType GetType() const override { return TargetType::Network_Socket; }
    FuzzResult Execute(const std::vector<uint8_t>& input) override;
    void Setup() override;
    void Cleanup() override;
    std::string GetName() const override;
    Architecture GetArchitecture() const override;

    // Network-specific methods
    void SetProtocol(const std::string& protocol); // "tcp", "udp"
    void SetTimeout(uint32_t timeout_ms);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Utility functions
namespace utils {
    std::vector<uint8_t> ReadFile(const std::string& filename);
    void WriteFile(const std::string& filename, const std::vector<uint8_t>& data);
    std::string GetExecutablePath();
    std::string GetModulePath(HMODULE module);
    bool IsProcessRunning(DWORD pid);
    std::string GetLastErrorString();
    std::string BytesToHex(const std::vector<uint8_t>& data);
    std::vector<uint8_t> HexToBytes(const std::string& hex);
    uint64_t HashData(const std::vector<uint8_t>& data);
}

} // namespace winuzzf
