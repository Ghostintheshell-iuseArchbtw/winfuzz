#include "winuzzf.h"
#include "cli_ui.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <csignal>
#include <thread>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <fstream>

using namespace winuzzf;
using Config = winuzzf::cli::Config;

// Global fuzzer instance for signal handling
std::unique_ptr<WinFuzzer> g_fuzzer = nullptr;
std::unique_ptr<cli::TerminalUI> g_ui = nullptr;
std::unique_ptr<cli::FuzzingStatsDisplay> g_stats = nullptr;

void SignalHandler(int signal) {
    if (g_ui) {
        g_ui->PrintLine("\nReceived interrupt signal, stopping fuzzer gracefully...", cli::Color::Bright_Yellow);
    }
    if (g_fuzzer) {
        g_fuzzer->Stop();
    }
    exit(0);
}

void PrintUsage(const char* program_name) {
    cli::HelpSystem::ShowFullHelp();
}

void PrintBanner() {
    if (g_ui) {
        g_ui->DisplayBanner();
    } else {
        cli::TerminalUI temp_ui;
        temp_ui.DisplayBanner();
    }
}

bool ParseArgs(int argc, char* argv[], Config& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            return false;
        } else if (arg == "--help-quick") {
            cli::HelpSystem::ShowQuickHelp();
            exit(0);
        } else if (arg == "--examples") {
            cli::HelpSystem::ShowExamples();
            exit(0);
        } else if (arg == "--help-advanced") {
            cli::HelpSystem::ShowAdvancedOptions();
            exit(0);
        } else if (arg == "--target-types") {
            cli::HelpSystem::ShowTargetTypes();
            exit(0);
        } else if (arg == "--mutation-strategies") {
            cli::HelpSystem::ShowMutationStrategies();
            exit(0);
        } else if (arg == "--coverage-types") {
            cli::HelpSystem::ShowCoverageTypes();
            exit(0);
        } else if (arg == "--target-api" && i + 2 < argc) {
            config.target_type = "api";
            config.target_param1 = argv[++i];
            config.target_param2 = argv[++i];
        } else if (arg == "--target-driver" && i + 1 < argc) {
            config.target_type = "driver";
            config.target_param1 = argv[++i];
        } else if (arg == "--target-exe" && i + 1 < argc) {
            config.target_type = "exe";
            config.target_param1 = argv[++i];
        } else if (arg == "--target-dll" && i + 2 < argc) {
            config.target_type = "dll";
            config.target_param1 = argv[++i];
            config.target_param2 = argv[++i];
        } else if (arg == "--target-network" && i + 1 < argc) {
            config.target_type = "network";
            config.target_param1 = argv[++i];
        } else if (arg == "--ioctl" && i + 1 < argc) {
            config.ioctl_code = std::strtoul(argv[++i], nullptr, 16);
        } else if (arg == "--corpus" && i + 1 < argc) {
            config.corpus_dir = argv[++i];
        } else if (arg == "--crashes" && i + 1 < argc) {
            config.crashes_dir = argv[++i];
        } else if (arg == "--logs" && i + 1 < argc) {
            config.logs_dir = argv[++i];
        } else if (arg == "--iterations" && i + 1 < argc) {
            config.max_iterations = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--timeout" && i + 1 < argc) {
            config.timeout_ms = std::strtoul(argv[++i], nullptr, 10);
        } else if (arg == "--threads" && i + 1 < argc) {
            config.threads = std::strtoul(argv[++i], nullptr, 10);
        } else if (arg == "--max-input-size" && i + 1 < argc) {
            config.max_input_size = std::strtoul(argv[++i], nullptr, 10);
        } else if (arg == "--coverage" && i + 1 < argc) {
            config.coverage_type = argv[++i];
        } else if (arg == "--mutation" && i + 1 < argc) {
            config.mutation_strategy = argv[++i];
        } else if (arg == "--dict" && i + 1 < argc) {
            config.dict_file = argv[++i];
        } else if (arg == "--seed" && i + 1 < argc) {
            config.seed_files.push_back(argv[++i]);
        } else if (arg == "--config" && i + 1 < argc) {
            config.config_file = argv[++i];
        } else if (arg == "--dry-run") {
            config.dry_run = true;
        } else if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
        } else if (arg == "--no-interactive") {
            config.interactive = false;
        } else if (arg == "--minimize") {
            config.minimize_corpus = true;
        } else if (arg == "--no-minimize") {
            config.minimize_corpus = false;
        } else if (arg == "--dedupe") {
            config.dedupe_crashes = true;
        } else if (arg == "--no-dedupe") {
            config.dedupe_crashes = false;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            return false;
        }
    }
    
    return !config.target_type.empty();
}

CoverageType ParseCoverageType(const std::string& type) {
    if (type == "etw") return CoverageType::ETW_USER;
    if (type == "etw-kernel") return CoverageType::ETW_KERNEL;
    if (type == "intel-pt") return CoverageType::Hardware_IntelPT;
    return CoverageType::None;
}

MutationStrategy ParseMutationStrategy(const std::string& strategy) {
    if (strategy == "random") return MutationStrategy::Random;
    if (strategy == "deterministic") return MutationStrategy::Deterministic;
    if (strategy == "dict") return MutationStrategy::Dictionary;
    if (strategy == "havoc") return MutationStrategy::Havoc;
    if (strategy == "splice") return MutationStrategy::Splice;
    return MutationStrategy::Random;
}

int main(int argc, char* argv[]) {
    // Initialize UI first
    g_ui = std::make_unique<cli::TerminalUI>();
    
    // Show banner
    PrintBanner();
    
    Config config;
    if (!ParseArgs(argc, argv, config)) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    // Validate configuration
    auto validation = cli::ConfigValidator::ValidateConfig(config);
    if (!validation.valid) {
        g_ui->PrintError("Configuration validation failed:");
        for (const auto& error : validation.errors) {
            g_ui->PrintError("  " + error);
        }
        return 1;
    }
    
    // Show warnings
    for (const auto& warning : validation.warnings) {
        g_ui->PrintWarning(warning);
    }
    
    // Dry run mode
    if (config.dry_run) {
        g_ui->PrintSuccess("Configuration is valid!");
        g_ui->PrintInfo("Target: " + config.target_type + " " + config.target_param1);
        if (!config.target_param2.empty()) {
            g_ui->PrintInfo("Function: " + config.target_param2);
        }
        g_ui->PrintInfo("Corpus: " + config.corpus_dir);
        g_ui->PrintInfo("Crashes: " + config.crashes_dir);
        g_ui->PrintInfo("Threads: " + std::to_string(config.threads));
        g_ui->PrintInfo("Coverage: " + config.coverage_type);
        return 0;
    }
    
    // Interactive confirmation for destructive operations
    if (config.interactive && !config.corpus_dir.empty()) {
        if (std::filesystem::exists(config.corpus_dir) && 
            !std::filesystem::is_empty(config.corpus_dir)) {
            if (!g_ui->ConfirmAction("Corpus directory exists and is not empty. Continue?")) {
                g_ui->PrintInfo("Operation cancelled by user");
                return 0;
            }
        }
    }
    
    // Install signal handlers
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    try {
        // Create and initialize stats display
        g_stats = std::make_unique<cli::FuzzingStatsDisplay>(g_ui.get());
        
        g_ui->PrintInfo("Initializing fuzzer...");
        
        // Create fuzzer instance
        g_fuzzer = WinFuzzer::Create();
        
        // Configure fuzzer
        FuzzConfig fuzz_config;
        fuzz_config.max_iterations = config.max_iterations;
        fuzz_config.timeout_ms = config.timeout_ms;
        fuzz_config.worker_threads = config.threads;
        fuzz_config.max_input_size = config.max_input_size;
        fuzz_config.corpus_dir = config.corpus_dir;
        fuzz_config.crashes_dir = config.crashes_dir;
        fuzz_config.logs_dir = config.logs_dir;
        fuzz_config.minimize_corpus = config.minimize_corpus;
        fuzz_config.deduplicate_crashes = config.dedupe_crashes;
        fuzz_config.coverage_type = ParseCoverageType(config.coverage_type);
        
        g_fuzzer->SetConfig(fuzz_config);
        
        // Create target
        std::shared_ptr<Target> target;
        
        if (config.target_type == "api") {
            g_ui->PrintInfo("Creating API target: " + config.target_param1 + "::" + config.target_param2);
            auto api_target = std::make_shared<APITarget>(config.target_param1, config.target_param2);
            
            // Set up common API parameter templates
            if (config.target_param2 == "CreateFileW") {
                api_target->SetParameterTemplate({
                    sizeof(LPCWSTR),    // lpFileName
                    sizeof(DWORD),      // dwDesiredAccess
                    sizeof(DWORD),      // dwShareMode
                    sizeof(LPSECURITY_ATTRIBUTES), // lpSecurityAttributes
                    sizeof(DWORD),      // dwCreationDisposition
                    sizeof(DWORD),      // dwFlagsAndAttributes
                    sizeof(HANDLE)      // hTemplateFile
                });
            }
            
            target = api_target;
        } else if (config.target_type == "driver") {
            g_ui->PrintInfo("Creating driver target: " + config.target_param1);
            if (config.ioctl_code != 0) {
                g_ui->PrintInfo("IOCTL code: 0x" + std::to_string(config.ioctl_code));
            }
            
            auto driver_target = std::make_shared<DriverTarget>(config.target_param1);
            if (config.ioctl_code != 0) {
                driver_target->SetIoctlCode(config.ioctl_code);
            }
            target = driver_target;
        } else if (config.target_type == "exe") {
            g_ui->PrintInfo("Creating executable target: " + config.target_param1);
            target = std::make_shared<ExecutableTarget>(config.target_param1);
        } else if (config.target_type == "dll") {
            g_ui->PrintInfo("Creating DLL target: " + config.target_param1 + "::" + config.target_param2);
            target = std::make_shared<DLLTarget>(config.target_param1, config.target_param2);
        } else if (config.target_type == "network") {
            g_ui->PrintInfo("Creating network target: " + config.target_param1);
            target = std::make_shared<NetworkTarget>(config.target_param1);
        } else {
            g_ui->PrintError("Unknown target type: " + config.target_type);
            return 1;
        }
        
        g_fuzzer->SetTarget(target);
        
        // Enable coverage if requested
        if (config.coverage_type != "none") {
            g_ui->PrintInfo("Enabling coverage: " + config.coverage_type);
            g_fuzzer->EnableCoverage(ParseCoverageType(config.coverage_type));
        }
        
        // Add mutation strategy
        g_ui->PrintInfo("Using mutation strategy: " + config.mutation_strategy);
        g_fuzzer->AddMutationStrategy(ParseMutationStrategy(config.mutation_strategy));
        
        // Load dictionary if provided
        if (!config.dict_file.empty()) {
            g_ui->PrintInfo("Loading dictionary: " + config.dict_file);
            try {
                auto dict_data = utils::ReadFile(config.dict_file);
                std::string dict_str(dict_data.begin(), dict_data.end());
                
                // Simple dictionary parsing (one entry per line)
                std::vector<std::string> dictionary;
                std::istringstream iss(dict_str);
                std::string line;
                while (std::getline(iss, line)) {
                    if (!line.empty() && line[0] != '#') { // Skip comments
                        dictionary.push_back(line);
                    }
                }
                
                g_fuzzer->SetDictionary(dictionary);
                g_ui->PrintSuccess("Loaded " + std::to_string(dictionary.size()) + " dictionary entries");
            } catch (const std::exception& e) {
                g_ui->PrintWarning("Failed to load dictionary: " + std::string(e.what()));
            }
        }
        
        // Load seed files
        for (const auto& seed_file : config.seed_files) {
            g_ui->PrintInfo("Loading seed: " + seed_file);
            try {
                auto seed_data = utils::ReadFile(seed_file);
                g_fuzzer->AddSeedInput(seed_data);
                g_ui->PrintSuccess("Loaded seed file (" + std::to_string(seed_data.size()) + " bytes)");
            } catch (const std::exception& e) {
                g_ui->PrintWarning("Failed to load seed file " + seed_file + ": " + e.what());
            }
        }
        
        // Load existing corpus
        g_ui->PrintInfo("Loading corpus from: " + config.corpus_dir);
        g_fuzzer->LoadCorpusFromDirectory(config.corpus_dir);
        auto corpus_count = g_fuzzer->GetCorpusSize();
        if (corpus_count > 0) {
            g_ui->PrintSuccess("Loaded " + std::to_string(corpus_count) + " corpus files");
        }
        
        // Set up callbacks for real-time updates
        g_fuzzer->SetCrashCallback([](const CrashInfo& crash) {
            if (g_ui) {
                g_ui->PrintLine("");
                g_ui->PrintError("!!! CRASH DETECTED !!!");
                g_ui->PrintError("Exception: 0x" + std::to_string(crash.exception_code));
                g_ui->PrintError("Address: 0x" + std::to_string(crash.exception_address));
                g_ui->PrintError("Module: " + crash.module_name);
                g_ui->PrintError("Function: " + crash.function_name);
                g_ui->Print("Exploitable: ", cli::Color::White);
                g_ui->PrintLine(crash.exploitable ? "YES" : "NO", 
                    crash.exploitable ? cli::Color::Bright_Red : cli::Color::Yellow);
                g_ui->PrintError("Hash: " + crash.crash_hash);
                g_ui->PrintLine("");
            }
        });
        
        g_fuzzer->SetProgressCallback([](uint64_t iterations, uint64_t crashes) {
            if (g_stats) {
                g_stats->UpdateIterations(iterations);
                g_stats->UpdateCrashes(crashes);
                g_stats->Refresh();
            }
        });
        
        // Display configuration summary
        g_ui->PrintLine("");
        g_ui->PrintLine("=== FUZZING CONFIGURATION ===", cli::Color::Bright_Cyan);
        g_ui->PrintInfo("Target: " + config.target_type + " " + config.target_param1);
        if (!config.target_param2.empty()) {
            g_ui->PrintInfo("Function: " + config.target_param2);
        }
        g_ui->PrintInfo("Max iterations: " + std::to_string(config.max_iterations));
        g_ui->PrintInfo("Timeout: " + std::to_string(config.timeout_ms) + "ms");
        g_ui->PrintInfo("Threads: " + std::to_string(config.threads));
        g_ui->PrintInfo("Corpus: " + config.corpus_dir);
        g_ui->PrintInfo("Crashes: " + config.crashes_dir);
        g_ui->PrintInfo("Coverage: " + config.coverage_type);
        g_ui->PrintInfo("Mutation: " + config.mutation_strategy);
        g_ui->PrintLine("");
        
        if (config.interactive) {
            g_ui->Print("Press Enter to start fuzzing...", cli::Color::Bright_Yellow);
            std::cin.get();
        }
        
        // Start fuzzing
        g_ui->PrintSuccess("Starting fuzzer...");
        g_ui->PrintInfo("Press Ctrl+C to stop gracefully");
        g_ui->PrintLine("");
        
        auto start_time = std::chrono::steady_clock::now();
        g_stats->UpdateStartTime(start_time);
        
        g_fuzzer->Start();
        
        // Main fuzzing loop with real-time updates
        while (g_fuzzer->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Update statistics
            if (g_stats) {
                g_stats->UpdateIterations(g_fuzzer->GetIterationCount());
                g_stats->UpdateCrashes(g_fuzzer->GetCrashCount());
                g_stats->UpdateHangs(g_fuzzer->GetHangCount());
                g_stats->UpdateExecPerSec(g_fuzzer->GetExecutionsPerSecond());
                
                auto coverage_info = g_fuzzer->GetCoverageInfo();
                g_stats->UpdateCoverage(coverage_info.coverage_percentage, coverage_info.basic_blocks_hit);
                g_stats->UpdateCorpusSize(g_fuzzer->GetCorpusSize());
                
                g_stats->Refresh();
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        // Clear stats display and show final results
        if (g_stats) {
            g_stats->Clear();
        }
        
        g_ui->PrintLine("");
        g_ui->PrintLine("=== FUZZING COMPLETE ===", cli::Color::Bright_Green);
        g_ui->PrintSuccess("Total iterations: " + std::to_string(g_fuzzer->GetIterationCount()));
        g_ui->PrintLine("Total crashes: " + std::to_string(g_fuzzer->GetCrashCount()), 
                        g_fuzzer->GetCrashCount() > 0 ? cli::Color::Bright_Red : cli::Color::Green);
        g_ui->PrintLine("Total hangs: " + std::to_string(g_fuzzer->GetHangCount()),
                        g_fuzzer->GetHangCount() > 0 ? cli::Color::Yellow : cli::Color::Green);
        
        auto hours = duration.count() / 3600;
        auto minutes = (duration.count() % 3600) / 60;
        auto seconds = duration.count() % 60;
        g_ui->PrintInfo("Duration: " + std::to_string(hours) + "h " + 
                        std::to_string(minutes) + "m " + std::to_string(seconds) + "s");
        g_ui->PrintInfo("Avg exec/sec: " + std::to_string(static_cast<int>(g_fuzzer->GetExecutionsPerSecond())));
        
        auto coverage_info = g_fuzzer->GetCoverageInfo();
        if (coverage_info.basic_blocks_hit > 0) {
            g_ui->PrintInfo("Basic blocks hit: " + std::to_string(coverage_info.basic_blocks_hit));
            g_ui->PrintInfo("Coverage: " + std::to_string(static_cast<int>(coverage_info.coverage_percentage)) + "%");
        }
        
        // Save final report
        std::string report_path = config.logs_dir + "/final_report.txt";
        std::ofstream report(report_path);
        if (report.is_open()) {
            report << "WinFuzz Final Report\n";
            report << "===================\n\n";
            report << "Target: " << config.target_type << " " << config.target_param1 << "\n";
            report << "Duration: " << duration.count() << " seconds\n";
            report << "Iterations: " << g_fuzzer->GetIterationCount() << "\n";
            report << "Crashes: " << g_fuzzer->GetCrashCount() << "\n";
            report << "Hangs: " << g_fuzzer->GetHangCount() << "\n";
            report << "Exec/sec: " << g_fuzzer->GetExecutionsPerSecond() << "\n";
            if (coverage_info.basic_blocks_hit > 0) {
                report << "Coverage: " << coverage_info.coverage_percentage << "%\n";
                report << "Basic blocks: " << coverage_info.basic_blocks_hit << "\n";
            }
            report.close();
            g_ui->PrintSuccess("Final report saved to: " + report_path);
        }
        
    } catch (const std::exception& e) {
        g_ui->PrintError("Fatal error: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}
