#include "fuzzer_impl.h"
#include "../mutators/mutator.h"
#include "../coverage/coverage_collector.h"
#include "../sandbox/sandbox.h"
#include "../crash/crash_analyzer.h"
#include "../logging/logger.h"
#include "../corpus/corpus_manager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>

namespace winuzzf {

WinFuzzerImpl::WinFuzzerImpl() 
    : running_(false)
    , paused_(false)
    , iteration_count_(0)
    , crash_count_(0)
    , hang_count_(0)
    , last_iteration_count_(0)
    , rng_(std::random_device{}())
{
    // Initialize default configuration
    config_ = FuzzConfig{};
    
    // Create core components
    coverage_collector_ = std::make_shared<CoverageCollector>();
    sandbox_ = std::make_shared<Sandbox>();
    crash_analyzer_ = std::make_shared<CrashAnalyzer>();
    logger_ = std::make_shared<Logger>();
    corpus_manager_ = std::make_shared<CorpusManager>();
}

WinFuzzerImpl::~WinFuzzerImpl() {
    Stop();
}

std::unique_ptr<WinFuzzer> WinFuzzer::Create() {
    return std::make_unique<WinFuzzerImpl>();
}

void WinFuzzerImpl::SetConfig(const FuzzConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    
    // Update component configurations
    if (logger_) {
        logger_->SetLogDirectory(config_.logs_dir);
    }
    if (corpus_manager_) {
        corpus_manager_->SetCorpusDirectory(config_.corpus_dir);
        corpus_manager_->SetMinimizationEnabled(config_.minimize_corpus);
    }
}

const FuzzConfig& WinFuzzerImpl::GetConfig() const {
    return config_;
}

void WinFuzzerImpl::SetTarget(std::shared_ptr<Target> target) {
    std::lock_guard<std::mutex> lock(mutex_);
    target_ = target;
}

std::shared_ptr<Target> WinFuzzerImpl::GetTarget() const {
    return target_;
}

void WinFuzzerImpl::EnableCoverage(CoverageType type) {
    if (coverage_collector_) {
        coverage_collector_->Enable(type);
    }
}

void WinFuzzerImpl::DisableCoverage() {
    if (coverage_collector_) {
        coverage_collector_->Disable();
    }
}

CoverageInfo WinFuzzerImpl::GetCoverageInfo() const {
    if (coverage_collector_) {
        return coverage_collector_->GetCoverageInfo();
    }
    return CoverageInfo{};
}

void WinFuzzerImpl::AddSeedInput(const std::vector<uint8_t>& input) {
    std::lock_guard<std::mutex> lock(mutex_);
    seed_inputs_.push_back(input);
    
    if (corpus_manager_) {
        corpus_manager_->AddInput(input);
    }
}

void WinFuzzerImpl::LoadCorpusFromDirectory(const std::string& dir) {
    if (!std::filesystem::exists(dir)) {
        logger_->LogWarning("Corpus directory does not exist: " + dir);
        return;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            if (file) {
                std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
                AddSeedInput(data);
            }
        }
    }
    
    logger_->LogInfo("Loaded corpus from: " + dir);
}

void WinFuzzerImpl::SaveCorpusToDirectory(const std::string& dir) {
    if (corpus_manager_) {
        corpus_manager_->SaveToDirectory(dir);
    }
}

void WinFuzzerImpl::AddMutationStrategy(MutationStrategy strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    mutation_strategies_.push_back(strategy);
}

void WinFuzzerImpl::SetDictionary(const std::vector<std::string>& dict) {
    std::lock_guard<std::mutex> lock(mutex_);
    dictionary_ = dict;
}

void WinFuzzerImpl::SetCrashCallback(CrashCallback callback) {
    crash_callback_ = callback;
}

void WinFuzzerImpl::SetCoverageCallback(CoverageCallback callback) {
    coverage_callback_ = callback;
}

void WinFuzzerImpl::SetProgressCallback(ProgressCallback callback) {
    progress_callback_ = callback;
}

void WinFuzzerImpl::Start() {
    if (running_) {
        logger_->LogWarning("Fuzzer is already running");
        return;
    }
    
    if (!target_) {
        logger_->LogError("No target set");
        return;
    }
    
    logger_->LogInfo("Starting fuzzer with " + std::to_string(config_.worker_threads) + " threads");
    
    // Initialize directories
    std::filesystem::create_directories(config_.corpus_dir);
    std::filesystem::create_directories(config_.crashes_dir);
    std::filesystem::create_directories(config_.logs_dir);
    
    // Setup target
    target_->Setup();
    
    // Initialize coverage if enabled
    if (config_.collect_coverage && coverage_collector_) {
        coverage_collector_->Initialize(target_);
    }
    
    // Populate work queue with seed inputs
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        for (const auto& seed : seed_inputs_) {
            work_queue_.push(seed);
        }
        
        // If no seeds, add empty input
        if (work_queue_.empty()) {
            work_queue_.push(std::vector<uint8_t>{});
        }
    }
    
    // Reset statistics
    iteration_count_ = 0;
    crash_count_ = 0;
    hang_count_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    last_stats_time_ = start_time_;
    
    // Start threads
    running_ = true;
    paused_ = false;
    
    // Start worker threads
    worker_threads_.reserve(config_.worker_threads);
    for (uint32_t i = 0; i < config_.worker_threads; ++i) {
        worker_threads_.emplace_back(&WinFuzzerImpl::WorkerThread, this, i);
    }
    
    // Start monitor thread
    monitor_thread_ = std::thread(&WinFuzzerImpl::MonitorThread, this);
    
    logger_->LogInfo("Fuzzer started successfully");
}

void WinFuzzerImpl::Stop() {
    if (!running_) {
        return;
    }
    
    logger_->LogInfo("Stopping fuzzer...");
    
    running_ = false;
    cv_.notify_all();
    
    // Join worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Join monitor thread
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    // Cleanup target
    if (target_) {
        target_->Cleanup();
    }
    
    // Save final corpus
    if (corpus_manager_) {
        corpus_manager_->SaveToDirectory(config_.corpus_dir);
    }
    
    logger_->LogInfo("Fuzzer stopped");
}

void WinFuzzerImpl::Pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = true;
    logger_->LogInfo("Fuzzer paused");
}

void WinFuzzerImpl::Resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = false;
    cv_.notify_all();
    logger_->LogInfo("Fuzzer resumed");
}

bool WinFuzzerImpl::IsRunning() const {
    return running_;
}

uint64_t WinFuzzerImpl::GetIterationCount() const {
    return iteration_count_;
}

uint64_t WinFuzzerImpl::GetCrashCount() const {
    return crash_count_;
}

uint64_t WinFuzzerImpl::GetHangCount() const {
    return hang_count_;
}

double WinFuzzerImpl::GetExecutionsPerSecond() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    if (duration.count() == 0) return 0.0;
    
    return static_cast<double>(iteration_count_) / duration.count();
}

uint64_t WinFuzzerImpl::GetCorpusSize() const {
    if (corpus_manager_) {
        // If we have a corpus manager, get the size from it
        // For now, return the size of seed inputs as a placeholder
        return seed_inputs_.size();
    }
    return seed_inputs_.size();
}

void WinFuzzerImpl::WorkerThread(int worker_id) {
    logger_->LogInfo("Worker thread " + std::to_string(worker_id) + " started");
    
    while (running_) {
        // Check if paused
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !paused_ || !running_; });
            
            if (!running_) break;
        }
        
        // Get input to fuzz
        std::vector<uint8_t> input;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (!work_queue_.empty()) {
                input = work_queue_.front();
                work_queue_.pop();
            }
        }
        
        // If no input available, generate one
        if (input.empty() && !seed_inputs_.empty()) {
            std::uniform_int_distribution<size_t> dist(0, seed_inputs_.size() - 1);
            input = seed_inputs_[dist(rng_)];
        }
        
        // Mutate input
        auto mutated_input = MutateInput(input);
        
        // Check iteration limit
        if (iteration_count_ >= config_.max_iterations) {
            logger_->LogInfo("Reached maximum iterations");
            break;
        }
        
        // Execute target
        try {
            auto result = target_->Execute(mutated_input);
            iteration_count_++;
            
            switch (result) {
                case FuzzResult::Crash: {
                    // Analyze crash
                    auto crash_info = crash_analyzer_->AnalyzeCrash(mutated_input);
                    HandleCrash(crash_info);
                    break;
                }
                case FuzzResult::Hang:
                    hang_count_++;
                    logger_->LogWarning("Hang detected on iteration " + std::to_string(iteration_count_));
                    break;
                case FuzzResult::Success:
                    // Check for new coverage
                    if (coverage_collector_ && coverage_collector_->IsEnabled()) {
                        auto coverage_info = coverage_collector_->GetCoverageInfo();
                        if (coverage_info.new_coverage > 0) {
                            HandleNewCoverage(coverage_info);
                            // Add to corpus for future mutations
                            if (corpus_manager_) {
                                corpus_manager_->AddInput(mutated_input);
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        catch (const std::exception& e) {
            logger_->LogError("Exception in worker thread " + std::to_string(worker_id) + ": " + e.what());
        }
    }
    
    logger_->LogInfo("Worker thread " + std::to_string(worker_id) + " stopped");
}

void WinFuzzerImpl::MonitorThread() {
    logger_->LogInfo("Monitor thread started");
    
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        if (!running_) break;
        
        UpdateStatistics();
        
        // Call progress callback
        if (progress_callback_) {
            progress_callback_(iteration_count_, crash_count_);
        }
    }
    
    logger_->LogInfo("Monitor thread stopped");
}

std::vector<uint8_t> WinFuzzerImpl::MutateInput(const std::vector<uint8_t>& input) {
    if (mutation_strategies_.empty()) {
        // Default random mutation
        return Mutator::RandomMutate(input, rng_);
    }
    
    // Select random mutation strategy
    std::uniform_int_distribution<size_t> dist(0, mutation_strategies_.size() - 1);
    auto strategy = mutation_strategies_[dist(rng_)];
    
    switch (strategy) {
        case MutationStrategy::Random:
            return Mutator::RandomMutate(input, rng_);
        case MutationStrategy::Deterministic:
            return Mutator::DeterministicMutate(input, iteration_count_);
        case MutationStrategy::Dictionary:
            return Mutator::DictionaryMutate(input, dictionary_, rng_);
        case MutationStrategy::Havoc:
            return Mutator::HavocMutate(input, rng_);
        case MutationStrategy::Splice:
            if (corpus_manager_) {
                auto corpus_inputs = corpus_manager_->GetRandomInputs(2);
                if (corpus_inputs.size() >= 2) {
                    return Mutator::SpliceMutate(corpus_inputs[0], corpus_inputs[1], rng_);
                }
            }
            return Mutator::RandomMutate(input, rng_);
        default:
            return Mutator::RandomMutate(input, rng_);
    }
}

void WinFuzzerImpl::HandleCrash(const CrashInfo& crash_info) {
    crash_count_++;
    
    logger_->LogError("CRASH DETECTED! Exception: 0x" + 
                     std::to_string(crash_info.exception_code) + 
                     " at 0x" + std::to_string(crash_info.exception_address));
    
    // Save crash input
    std::string crash_filename = config_.crashes_dir + "/crash_" + 
                                std::to_string(crash_count_) + "_" + 
                                crash_info.crash_hash + ".bin";
    
    std::ofstream crash_file(crash_filename, std::ios::binary);
    if (crash_file) {
        crash_file.write(reinterpret_cast<const char*>(crash_info.input_data.data()),
                        crash_info.input_data.size());
    }
    
    // Call crash callback
    if (crash_callback_) {
        crash_callback_(crash_info);
    }
}

void WinFuzzerImpl::HandleNewCoverage(const CoverageInfo& coverage_info) {
    logger_->LogInfo("New coverage found! Total BBs: " + 
                    std::to_string(coverage_info.basic_blocks_hit) + 
                    ", New: " + std::to_string(coverage_info.new_coverage));
    
    // Call coverage callback
    if (coverage_callback_) {
        coverage_callback_(coverage_info);
    }
}

void WinFuzzerImpl::UpdateStatistics() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time_);
    
    if (duration.count() >= 10) { // Update every 10 seconds
        auto current_iterations = iteration_count_.load();
        auto iterations_delta = current_iterations - last_iteration_count_;
        auto exec_per_sec = static_cast<double>(iterations_delta) / duration.count();
        
        logger_->LogInfo("Stats - Iterations: " + std::to_string(current_iterations) + 
                        ", Crashes: " + std::to_string(crash_count_) + 
                        ", Hangs: " + std::to_string(hang_count_) + 
                        ", Exec/sec: " + std::to_string(exec_per_sec));
        
        last_iteration_count_ = current_iterations;
        last_stats_time_ = now;
    }
}

} // namespace winuzzf
