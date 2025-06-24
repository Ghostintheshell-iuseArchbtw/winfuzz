#pragma once

#include "winuzzf.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <random>

namespace winuzzf {

class WinFuzzerImpl : public WinFuzzer {
public:
    WinFuzzerImpl();
    ~WinFuzzerImpl() override;

    // Configuration
    void SetConfig(const FuzzConfig& config) override;
    const FuzzConfig& GetConfig() const override;

    // Target management
    void SetTarget(std::shared_ptr<Target> target) override;
    std::shared_ptr<Target> GetTarget() const override;

    // Coverage
    void EnableCoverage(CoverageType type) override;
    void DisableCoverage() override;
    CoverageInfo GetCoverageInfo() const override;

    // Corpus management
    void AddSeedInput(const std::vector<uint8_t>& input) override;
    void LoadCorpusFromDirectory(const std::string& dir) override;
    void SaveCorpusToDirectory(const std::string& dir) override;

    // Mutation
    void AddMutationStrategy(MutationStrategy strategy) override;
    void SetDictionary(const std::vector<std::string>& dict) override;

    // Callbacks
    void SetCrashCallback(CrashCallback callback) override;
    void SetCoverageCallback(CoverageCallback callback) override;
    void SetProgressCallback(ProgressCallback callback) override;

    // Fuzzing control
    void Start() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    bool IsRunning() const override;

    // Statistics
    uint64_t GetIterationCount() const override;
    uint64_t GetCrashCount() const override;
    uint64_t GetHangCount() const override;
    uint64_t GetCorpusSize() const override;
    double GetExecutionsPerSecond() const override;

private:
    void WorkerThread(int worker_id);
    void MonitorThread();
    std::vector<uint8_t> MutateInput(const std::vector<uint8_t>& input);
    void HandleCrash(const CrashInfo& crash_info);
    void HandleNewCoverage(const CoverageInfo& coverage_info);
    void UpdateStatistics();

    // Configuration
    FuzzConfig config_;
    
    // Core components
    std::shared_ptr<Target> target_;
    std::shared_ptr<CoverageCollector> coverage_collector_;
    std::shared_ptr<Sandbox> sandbox_;
    std::shared_ptr<CrashAnalyzer> crash_analyzer_;
    std::shared_ptr<Logger> logger_;
    std::shared_ptr<CorpusManager> corpus_manager_;
    
    // Threading
    std::vector<std::thread> worker_threads_;
    std::thread monitor_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::mutex mutex_;
    std::condition_variable cv_;
    
    // Statistics
    std::atomic<uint64_t> iteration_count_;
    std::atomic<uint64_t> crash_count_;
    std::atomic<uint64_t> hang_count_;
    std::atomic<uint64_t> last_iteration_count_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_stats_time_;
    
    // Mutation
    std::vector<MutationStrategy> mutation_strategies_;
    std::vector<std::string> dictionary_;
    std::mt19937 rng_;
    
    // Callbacks
    CrashCallback crash_callback_;
    CoverageCallback coverage_callback_;
    ProgressCallback progress_callback_;
    
    // Corpus
    std::vector<std::vector<uint8_t>> seed_inputs_;
    std::queue<std::vector<uint8_t>> work_queue_;
    std::mutex queue_mutex_;
};

} // namespace winuzzf
