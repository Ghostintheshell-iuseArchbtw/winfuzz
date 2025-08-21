#pragma once

#include <string>
#include <chrono>
#include <memory>
#include <vector>
#include <windows.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>

namespace winuzzf {
namespace cli {

// Configuration structure
struct Config {
    std::string target_type;
    std::string target_param1;
    std::string target_param2;
    uint32_t ioctl_code = 0;
    std::string corpus_dir = "corpus";
    std::string crashes_dir = "crashes";
    std::string logs_dir = "logs";
    uint64_t max_iterations = 1000000;
    uint32_t timeout_ms = 5000;
    uint32_t threads = 8;
    uint32_t max_input_size = 65536;
    std::string coverage_type = "none";
    std::string mutation_strategy = "random";
    std::string dict_file;
    std::vector<std::string> seed_files;
    bool minimize_corpus = true;
    bool dedupe_crashes = true;
    bool dry_run = false;
    bool verbose = false;
    bool interactive = true;
    std::string config_file;
};

// Console colors
enum class Color {
    Reset = 0,
    Red = 4,
    Green = 2,
    Yellow = 6,
    Blue = 1,
    Magenta = 5,
    Cyan = 3,
    White = 7,
    Bright_Red = 12,
    Bright_Green = 10,
    Bright_Yellow = 14,
    Bright_Blue = 9,
    Bright_Magenta = 13,
    Bright_Cyan = 11,
    Bright_White = 15
};

// Terminal UI class for improved fuzzer interface
class TerminalUI {
public:
    TerminalUI();
    ~TerminalUI();

    // Terminal control
    void Clear();
    void HideCursor();
    void ShowCursor();
    void SetCursorPosition(int x, int y);
    void ClearLine(int y);
    void SetTitle(const std::string& title);
    void SetColor(Color color);
    void ResetColor();
    
    // Formatted output
    void Print(const std::string& text, Color color = Color::White);
    void PrintLine(const std::string& text, Color color = Color::White);
    void PrintError(const std::string& text);
    void PrintWarning(const std::string& text);
    void PrintSuccess(const std::string& text);
    void PrintInfo(const std::string& text);
    
    // Progress and status display
    // Draws a color-coded progress bar with percentage information
    void DrawProgressBar(const std::string& label, double percentage, int width = 50);
    void UpdateStatus(const std::string& status);
    void DisplayBanner();
    
    // Interactive elements
    bool ConfirmAction(const std::string& prompt);
    std::string GetInput(const std::string& prompt);
    
    // Size information
    int GetWidth() const { return console_width_; }
    int GetHeight() const { return console_height_; }

    // Utility methods
    std::string FormatTime(std::chrono::seconds duration);
    std::string FormatBytes(uint64_t bytes);
    std::string FormatNumber(uint64_t number);

private:
    HANDLE console_handle_;
    CONSOLE_SCREEN_BUFFER_INFO original_info_;
    int console_width_;
    int console_height_;
    int spinner_index_;
    static inline const char kSpinnerFrames[4] = {'|', '/', '-', '\'};

    void UpdateConsoleSize();
};

// Simple animated spinner for long running operations
class Spinner {
public:
    explicit Spinner(TerminalUI* ui);
    ~Spinner();

    void Start(const std::string& message);
    void Stop();

private:
    void Run(std::string message);
    TerminalUI* ui_;
    std::thread thread_;
    std::atomic<bool> running_;
};

// Real-time fuzzing stats display
class FuzzingStatsDisplay {
public:
    FuzzingStatsDisplay(TerminalUI* ui);
    
    // Update statistics
    void UpdateIterations(uint64_t iterations);
    void UpdateCrashes(uint64_t crashes);
    void UpdateHangs(uint64_t hangs);
    void UpdateExecPerSec(double exec_per_sec);
    void UpdateCoverage(double coverage_percentage, uint64_t basic_blocks);
    void UpdateCorpusSize(uint64_t size);
    void UpdateStartTime(std::chrono::steady_clock::time_point start);
    
    // Display the stats panel
    void Refresh();
    void Clear();

private:
    TerminalUI* ui_;
    uint64_t iterations_;
    uint64_t crashes_;
    uint64_t hangs_;
    double exec_per_sec_;
    double coverage_percentage_;
    uint64_t basic_blocks_hit_;
    uint64_t corpus_size_;
    std::chrono::steady_clock::time_point start_time_;
    
    void DrawBox(int x, int y, int width, int height, const std::string& title);
    void DrawStatLine(int y, const std::string& label, const std::string& value, Color color = Color::White);
};

// Enhanced help system
class HelpSystem {
public:
    static void ShowFullHelp();
    static void ShowQuickHelp();
    static void ShowExamples();
    static void ShowAdvancedOptions();
    static void ShowTargetTypes();
    static void ShowMutationStrategies();
    static void ShowCoverageTypes();
};

// Configuration validator
class ConfigValidator {
public:
    struct ValidationResult {
        bool valid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    static ValidationResult ValidateConfig(const Config& config);
    static bool ValidateDirectory(const std::string& path, bool create_if_missing = false);
    static bool ValidateFile(const std::string& path);
    static ValidationResult ValidateTarget(const std::string& target_type, const std::string& param1, const std::string& param2);
};

} // namespace cli
} // namespace winuzzf
