#include "cli_ui.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <atomic>

namespace winuzzf {
namespace cli {

TerminalUI::TerminalUI() : spinner_index_(0) {
    console_handle_ = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(console_handle_, &original_info_);
    UpdateConsoleSize();
    
    // Enable virtual terminal processing for color support
    DWORD mode = 0;
    GetConsoleMode(console_handle_, &mode);
    SetConsoleMode(console_handle_, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

TerminalUI::~TerminalUI() {
    ResetColor();
    ShowCursor();
}

void TerminalUI::Clear() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD count;
    DWORD cell_count;
    COORD home = {0, 0};

    if (!GetConsoleScreenBufferInfo(console_handle_, &csbi)) {
        system("cls");
        return;
    }

    cell_count = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(console_handle_, ' ', cell_count, home, &count);
    FillConsoleOutputAttribute(console_handle_, csbi.wAttributes, cell_count, home, &count);
    SetConsoleCursorPosition(console_handle_, home);
}

void TerminalUI::HideCursor() {
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(console_handle_, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(console_handle_, &cursorInfo);
}

void TerminalUI::ShowCursor() {
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(console_handle_, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(console_handle_, &cursorInfo);
}

void TerminalUI::SetCursorPosition(int x, int y) {
    COORD pos = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    SetConsoleCursorPosition(console_handle_, pos);
}

void TerminalUI::ClearLine(int y) {
    SetCursorPosition(0, y);
    Print(std::string(console_width_, ' '), Color::White);
    SetCursorPosition(0, y);
}

void TerminalUI::SetTitle(const std::string& title) {
    SetConsoleTitleA(title.c_str());
}

void TerminalUI::SetColor(Color color) {
    SetConsoleTextAttribute(console_handle_, static_cast<WORD>(color));
}

void TerminalUI::ResetColor() {
    SetConsoleTextAttribute(console_handle_, original_info_.wAttributes);
}

void TerminalUI::Print(const std::string& text, Color color) {
    SetColor(color);
    std::cout << text;
    ResetColor();
}

void TerminalUI::PrintLine(const std::string& text, Color color) {
    Print(text + "\n", color);
}

void TerminalUI::PrintError(const std::string& text) {
    Print("[ERROR] ", Color::Bright_Red);
    PrintLine(text, Color::Red);
}

void TerminalUI::PrintWarning(const std::string& text) {
    Print("[WARN]  ", Color::Bright_Yellow);
    PrintLine(text, Color::Yellow);
}

void TerminalUI::PrintSuccess(const std::string& text) {
    Print("[OK]    ", Color::Bright_Green);
    PrintLine(text, Color::Green);
}

void TerminalUI::PrintInfo(const std::string& text) {
    Print("[INFO]  ", Color::Bright_Cyan);
    PrintLine(text, Color::Cyan);
}

void TerminalUI::DrawProgressBar(const std::string& label, double percentage, int width) {
    std::cout << label << " [";
    
    int filled = static_cast<int>(percentage * width / 100.0);
    int remaining = width - filled;
    
    SetColor(Color::Bright_Green);
    for (int i = 0; i < filled; ++i) {
        std::cout << "█";
    }
    
    SetColor(Color::White);
    for (int i = 0; i < remaining; ++i) {
        std::cout << "░";
    }
    
    ResetColor();
    std::cout << "] " << std::fixed << std::setprecision(1) << percentage << "%\n";
}

void TerminalUI::UpdateStatus(const std::string& status) {
    SetCursorPosition(0, console_height_ - 1);
    Print(std::string(console_width_, ' '), Color::White); // Clear line
    SetCursorPosition(0, console_height_ - 1);
    char spinner = kSpinnerFrames[spinner_index_];
    spinner_index_ = (spinner_index_ + 1) % 4;
    Print(std::string(1, spinner) + " Status: " + status, Color::Bright_Cyan);
}

void TerminalUI::DisplayBanner() {
    Clear();
    PrintLine(R"(
    ██╗    ██╗██╗███╗   ██╗    ███████╗██╗   ██╗███████╗███████╗
    ██║    ██║██║████╗  ██║    ██╔════╝██║   ██║╚══███╔╝╚══███╔╝
    ██║ █╗ ██║██║██╔██╗ ██║    █████╗  ██║   ██║  ███╔╝   ███╔╝ 
    ██║███╗██║██║██║╚██╗██║    ██╔══╝  ██║   ██║ ███╔╝   ███╔╝  
    ╚███╔███╔╝██║██║ ╚████║    ██║     ╚██████╔╝███████╗███████╗
     ╚══╝╚══╝ ╚═╝╚═╝  ╚═══╝    ╚═╝      ╚═════╝ ╚══════╝╚══════╝
    )", Color::Bright_Cyan);
    
    PrintLine("            Windows Advanced Fuzzing Framework v2.0", Color::Bright_White);
    PrintLine("            Intelligent vulnerability discovery platform", Color::Cyan);
    PrintLine("", Color::White);
}

bool TerminalUI::ConfirmAction(const std::string& prompt) {
    Print(prompt + " [y/N]: ", Color::Bright_Yellow);
    std::string response;
    std::getline(std::cin, response);
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);
    return response == "y" || response == "yes";
}

std::string TerminalUI::GetInput(const std::string& prompt) {
    Print(prompt + ": ", Color::Bright_Cyan);
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void TerminalUI::UpdateConsoleSize() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(console_handle_, &csbi);
    console_width_ = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    console_height_ = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}

std::string TerminalUI::FormatTime(std::chrono::seconds duration) {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration % std::chrono::hours(1));
    auto seconds = duration % std::chrono::minutes(1);
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours.count() << ":"
       << std::setw(2) << minutes.count() << ":"
       << std::setw(2) << seconds.count();
    return ss.str();
}

std::string TerminalUI::FormatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return ss.str();
}

std::string TerminalUI::FormatNumber(uint64_t number) {
    std::string num_str = std::to_string(number);
    std::string formatted;
    
    int count = 0;
    for (auto it = num_str.rbegin(); it != num_str.rend(); ++it) {
        if (count > 0 && count % 3 == 0) {
            formatted = ',' + formatted;
        }
        formatted = *it + formatted;
        count++;
    }
    
    return formatted;
}

// Spinner implementation
Spinner::Spinner(TerminalUI* ui) : ui_(ui), running_(false) {}

Spinner::~Spinner() {
    Stop();
}

void Spinner::Start(const std::string& message) {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&Spinner::Run, this, message);
}

void Spinner::Run(std::string message) {
    const char* frames = "|/-\\";
    size_t index = 0;
    int line = ui_->GetHeight() - 2;
    while (running_) {
        ui_->SetCursorPosition(0, line);
        ui_->Print(message + " " + frames[index % 4], Color::Bright_Cyan);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        index++;
    }
    ui_->ClearLine(line);
}

void Spinner::Stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

// FuzzingStatsDisplay implementation
FuzzingStatsDisplay::FuzzingStatsDisplay(TerminalUI* ui) 
    : ui_(ui), iterations_(0), crashes_(0), hangs_(0), exec_per_sec_(0.0),
      coverage_percentage_(0.0), basic_blocks_hit_(0), corpus_size_(0) {
    start_time_ = std::chrono::steady_clock::now();
}

void FuzzingStatsDisplay::UpdateIterations(uint64_t iterations) {
    iterations_ = iterations;
}

void FuzzingStatsDisplay::UpdateCrashes(uint64_t crashes) {
    crashes_ = crashes;
}

void FuzzingStatsDisplay::UpdateHangs(uint64_t hangs) {
    hangs_ = hangs;
}

void FuzzingStatsDisplay::UpdateExecPerSec(double exec_per_sec) {
    exec_per_sec_ = exec_per_sec;
}

void FuzzingStatsDisplay::UpdateCoverage(double coverage_percentage, uint64_t basic_blocks) {
    coverage_percentage_ = coverage_percentage;
    basic_blocks_hit_ = basic_blocks;
}

void FuzzingStatsDisplay::UpdateCorpusSize(uint64_t size) {
    corpus_size_ = size;
}

void FuzzingStatsDisplay::UpdateStartTime(std::chrono::steady_clock::time_point start) {
    start_time_ = start;
}

void FuzzingStatsDisplay::Refresh() {
    // Calculate runtime
    auto now = std::chrono::steady_clock::now();
    auto runtime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    
    // Position cursor and draw stats box
    ui_->SetCursorPosition(0, 10);
    DrawBox(0, 10, 80, 15, "Fuzzing Statistics");
    
    // Draw statistics
    DrawStatLine(12, "Runtime", ui_->FormatTime(runtime), Color::Bright_White);
    DrawStatLine(13, "Iterations", ui_->FormatNumber(iterations_), Color::Bright_Green);
    DrawStatLine(14, "Exec/sec", std::to_string(static_cast<int>(exec_per_sec_)), Color::Green);
    DrawStatLine(15, "Crashes", ui_->FormatNumber(crashes_), Color::Bright_Red);
    DrawStatLine(16, "Hangs", ui_->FormatNumber(hangs_), Color::Yellow);
    DrawStatLine(17, "Corpus Size", ui_->FormatNumber(corpus_size_), Color::Cyan);
    
    if (basic_blocks_hit_ > 0) {
        DrawStatLine(18, "Coverage", 
                    std::to_string(static_cast<int>(coverage_percentage_)) + "% (" + 
                    ui_->FormatNumber(basic_blocks_hit_) + " blocks)", Color::Bright_Cyan);
    }
    
    // Draw progress bar for coverage if available
    if (coverage_percentage_ > 0) {
        ui_->SetCursorPosition(2, 20);
        ui_->DrawProgressBar("Code Coverage", coverage_percentage_, 50);
    }
}

void FuzzingStatsDisplay::Clear() {
    for (int i = 10; i < 25; ++i) {
        ui_->SetCursorPosition(0, i);
        ui_->Print(std::string(80, ' '), Color::White);
    }
}

void FuzzingStatsDisplay::DrawBox(int x, int y, int width, int height, const std::string& title) {
    ui_->SetCursorPosition(x, y);
    ui_->Print("┌", Color::Bright_White);
    ui_->Print(std::string(width - 2, '─'), Color::Bright_White);
    ui_->Print("┐", Color::Bright_White);
    
    // Title
    if (!title.empty()) {
        ui_->SetCursorPosition(x + 2, y);
        ui_->Print("[ " + title + " ]", Color::Bright_Cyan);
    }
    
    // Sides
    for (int i = 1; i < height - 1; ++i) {
        ui_->SetCursorPosition(x, y + i);
        ui_->Print("│", Color::Bright_White);
        ui_->SetCursorPosition(x + width - 1, y + i);
        ui_->Print("│", Color::Bright_White);
    }
    
    // Bottom
    ui_->SetCursorPosition(x, y + height - 1);
    ui_->Print("└", Color::Bright_White);
    ui_->Print(std::string(width - 2, '─'), Color::Bright_White);
    ui_->Print("┘", Color::Bright_White);
}

void FuzzingStatsDisplay::DrawStatLine(int y, const std::string& label, const std::string& value, Color color) {
    ui_->SetCursorPosition(3, y);
    ui_->Print(label + ":", Color::White);
    ui_->SetCursorPosition(20, y);
    ui_->Print(value, color);
}

// HelpSystem implementation
void HelpSystem::ShowFullHelp() {
    TerminalUI ui;
    ui.Clear();
    ui.PrintLine("WinFuzz - Advanced Windows Fuzzing Framework v2.0", Color::Bright_Cyan);
    ui.PrintLine("==================================================", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("USAGE:", Color::Bright_Yellow);
    ui.PrintLine("  winuzzf [TARGET] [OPTIONS]", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("TARGET TYPES:", Color::Bright_Yellow);
    ui.PrintLine("  --target-api <module> <function>    Fuzz Windows API function", Color::White);
    ui.PrintLine("  --target-driver <device>            Fuzz kernel driver via IOCTL", Color::White);
    ui.PrintLine("  --target-exe <path>                 Fuzz executable with file inputs", Color::White);
    ui.PrintLine("  --target-dll <path> <export>        Fuzz DLL export function", Color::White);
    ui.PrintLine("  --target-network <host:port>        Fuzz network service", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("CORE OPTIONS:", Color::Bright_Yellow);
    ui.PrintLine("  --corpus <dir>                      Input corpus directory", Color::White);
    ui.PrintLine("  --crashes <dir>                     Crash output directory", Color::White);
    ui.PrintLine("  --logs <dir>                        Log output directory", Color::White);
    ui.PrintLine("  --iterations <count>                Maximum iterations (default: 1000000)", Color::White);
    ui.PrintLine("  --timeout <ms>                      Execution timeout (default: 5000)", Color::White);
    ui.PrintLine("  --threads <count>                   Worker threads (default: 8)", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("COVERAGE OPTIONS:", Color::Bright_Yellow);
    ui.PrintLine("  --coverage <type>                   Coverage type: etw|intel-pt|lbr|none", Color::White);
    ui.PrintLine("  --coverage-modules <list>           Modules to track (comma-separated)", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("MUTATION OPTIONS:", Color::Bright_Yellow);
    ui.PrintLine("  --mutation <strategy>               Strategy: random|dict|havoc|splice", Color::White);
    ui.PrintLine("  --dict <file>                       Dictionary file", Color::White);
    ui.PrintLine("  --seed <file>                       Seed input file (can be repeated)", Color::White);
    ui.PrintLine("  --max-input-size <bytes>            Maximum input size", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("ADVANCED OPTIONS:", Color::Bright_Yellow);
    ui.PrintLine("  --ioctl <code>                      IOCTL code (hex)", Color::White);
    ui.PrintLine("  --minimize                          Minimize corpus", Color::White);
    ui.PrintLine("  --dedupe                            Deduplicate crashes", Color::White);
    ui.PrintLine("  --dry-run                           Validate configuration only", Color::White);
    ui.PrintLine("  --verbose                           Enable verbose output", Color::White);
    ui.PrintLine("  --config <file>                     Load configuration from file", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("Use --examples to see usage examples", Color::Bright_Green);
    ui.PrintLine("Use --help-advanced for more options", Color::Bright_Green);
}

void HelpSystem::ShowQuickHelp() {
    TerminalUI ui;
    ui.PrintLine("WinFuzz Quick Help", Color::Bright_Cyan);
    ui.PrintLine("==================", Color::Cyan);
    ui.PrintLine("");
    ui.PrintLine("Basic Usage:", Color::Bright_Yellow);
    ui.PrintLine("  winuzzf --target-api kernel32.dll CreateFileW --corpus corpus", Color::White);
    ui.PrintLine("  winuzzf --target-exe notepad.exe --seed input.txt", Color::White);
    ui.PrintLine("");
    ui.PrintLine("Use --help for full documentation", Color::Bright_Green);
}

void HelpSystem::ShowExamples() {
    TerminalUI ui;
    ui.Clear();
    ui.PrintLine("WinFuzz Usage Examples", Color::Bright_Cyan);
    ui.PrintLine("======================", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("1. Fuzz CreateFileW API:", Color::Bright_Yellow);
    ui.PrintLine("   winuzzf --target-api kernel32.dll CreateFileW \\", Color::White);
    ui.PrintLine("           --corpus corpus --crashes crashes \\", Color::White);
    ui.PrintLine("           --coverage etw --iterations 100000", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("2. Fuzz driver IOCTL:", Color::Bright_Yellow);
    ui.PrintLine("   winuzzf --target-driver \\\\.\\MyDriver \\", Color::White);
    ui.PrintLine("           --ioctl 0x220000 --coverage intel-pt \\", Color::White);
    ui.PrintLine("           --threads 4 --timeout 10000", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("3. Fuzz executable with dictionary:", Color::Bright_Yellow);
    ui.PrintLine("   winuzzf --target-exe notepad.exe \\", Color::White);
    ui.PrintLine("           --corpus inputs --dict dictionary.txt \\", Color::White);
    ui.PrintLine("           --mutation dict --seed sample.txt", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("4. Network fuzzing:", Color::Bright_Yellow);
    ui.PrintLine("   winuzzf --target-network 127.0.0.1:8080 \\", Color::White);
    ui.PrintLine("           --corpus http_corpus --mutation havoc", Color::White);
    ui.PrintLine("");
}

void HelpSystem::ShowAdvancedOptions() {
    TerminalUI ui;
    ui.Clear();
    ui.PrintLine("Advanced Configuration Options", Color::Bright_Cyan);
    ui.PrintLine("==============================", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("PERFORMANCE TUNING:", Color::Bright_Yellow);
    ui.PrintLine("  --cpu-affinity <mask>               Set CPU affinity mask", Color::White);
    ui.PrintLine("  --memory-limit <mb>                 Memory limit per worker", Color::White);
    ui.PrintLine("  --batch-size <count>                Inputs per batch", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("ANALYSIS OPTIONS:", Color::Bright_Yellow);
    ui.PrintLine("  --triage-crashes                    Auto-triage crashes", Color::White);
    ui.PrintLine("  --exploitability                    Assess exploitability", Color::White);
    ui.PrintLine("  --save-inputs                       Save all inputs", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("DEBUGGING:", Color::Bright_Yellow);
    ui.PrintLine("  --debug-target                      Debug target execution", Color::White);
    ui.PrintLine("  --trace-syscalls                    Trace system calls", Color::White);
    ui.PrintLine("  --log-level <level>                 Logging level (0-4)", Color::White);
}

void HelpSystem::ShowTargetTypes() {
    TerminalUI ui;
    ui.PrintLine("Supported Target Types", Color::Bright_Cyan);
    ui.PrintLine("======================", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("API Functions:", Color::Bright_Yellow);
    ui.PrintLine("  Fuzz Windows API functions with structured inputs", Color::White);
    ui.PrintLine("  Example: --target-api kernel32.dll CreateFileW", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("Executables:", Color::Bright_Yellow);
    ui.PrintLine("  Fuzz command-line applications with file inputs", Color::White);
    ui.PrintLine("  Example: --target-exe notepad.exe", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("Kernel Drivers:", Color::Bright_Yellow);
    ui.PrintLine("  Fuzz device drivers via IOCTL interface", Color::White);
    ui.PrintLine("  Example: --target-driver \\\\.\\MyDevice --ioctl 0x220000", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("Network Services:", Color::Bright_Yellow);
    ui.PrintLine("  Fuzz TCP/UDP network services", Color::White);
    ui.PrintLine("  Example: --target-network 127.0.0.1:8080", Color::Cyan);
}

void HelpSystem::ShowMutationStrategies() {
    TerminalUI ui;
    ui.PrintLine("Mutation Strategies", Color::Bright_Cyan);
    ui.PrintLine("===================", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("Random:", Color::Bright_Yellow);
    ui.PrintLine("  Pure random bit flipping and modifications", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("Dictionary:", Color::Bright_Yellow);
    ui.PrintLine("  Use predefined interesting values and keywords", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("Havoc:", Color::Bright_Yellow);
    ui.PrintLine("  Aggressive mutations with stacked operations", Color::White);
    ui.PrintLine("");
    
    ui.PrintLine("Splice:", Color::Bright_Yellow);
    ui.PrintLine("  Combine parts from different corpus inputs", Color::White);
}

void HelpSystem::ShowCoverageTypes() {
    TerminalUI ui;
    ui.PrintLine("Coverage Collection Types", Color::Bright_Cyan);
    ui.PrintLine("=========================", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("ETW (Event Tracing for Windows):", Color::Bright_Yellow);
    ui.PrintLine("  Software-based coverage via ETW events", Color::White);
    ui.PrintLine("  Modes: etw (user-mode), etw-kernel (kernel-mode)", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("Intel PT (Processor Trace):", Color::Bright_Yellow);
    ui.PrintLine("  Hardware-based high-performance coverage", Color::White);
    ui.PrintLine("  Mode: intel-pt", Color::Cyan);
    ui.PrintLine("");
    
    ui.PrintLine("LBR (Last Branch Records):", Color::Bright_Yellow);
    ui.PrintLine("  Hardware branch tracing", Color::White);
    ui.PrintLine("  Mode: lbr", Color::Cyan);
}

// ConfigValidator implementation  
ConfigValidator::ValidationResult ConfigValidator::ValidateConfig(const struct Config& config) {
    ValidationResult result;
    result.valid = true;
    
    // Validate target
    if (config.target_type.empty()) {
        result.errors.push_back("No target specified");
        result.valid = false;
    }
    
    auto target_validation = ValidateTarget(config.target_type, config.target_param1, config.target_param2);
    if (!target_validation.valid) {
        result.errors.insert(result.errors.end(), target_validation.errors.begin(), target_validation.errors.end());
        result.valid = false;
    }
    
    // Validate directories
    if (!ValidateDirectory(config.corpus_dir, true)) {
        result.warnings.push_back("Corpus directory does not exist, will create");
    }
    
    if (!ValidateDirectory(config.crashes_dir, true)) {
        result.warnings.push_back("Crashes directory does not exist, will create");
    }
    
    // Validate numeric parameters
    if (config.threads == 0 || config.threads > 64) {
        result.errors.push_back("Invalid thread count (1-64)");
        result.valid = false;
    }
    
    if (config.timeout_ms == 0 || config.timeout_ms > 300000) {
        result.warnings.push_back("Unusual timeout value");
    }
    
    if (config.max_iterations == 0) {
        result.warnings.push_back("Unlimited iterations specified");
    }
    
    return result;
}

bool ConfigValidator::ValidateDirectory(const std::string& path, bool create_if_missing) {
    if (std::filesystem::exists(path)) {
        return std::filesystem::is_directory(path);
    }
    
    if (create_if_missing) {
        try {
            return std::filesystem::create_directories(path);
        } catch (const std::exception&) {
            return false;
        }
    }
    
    return false;
}

bool ConfigValidator::ValidateFile(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

ConfigValidator::ValidationResult ConfigValidator::ValidateTarget(const std::string& target_type, const std::string& param1, const std::string& param2) {
    ValidationResult result;
    result.valid = true;
    
    if (target_type == "api") {
        if (param1.empty()) {
            result.errors.push_back("API target requires module name");
            result.valid = false;
        }
        if (param2.empty()) {
            result.errors.push_back("API target requires function name");
            result.valid = false;
        }
    } else if (target_type == "driver") {
        if (param1.empty()) {
            result.errors.push_back("Driver target requires device name");
            result.valid = false;
        }
    } else if (target_type == "exe") {
        if (param1.empty()) {
            result.errors.push_back("Executable target requires path");
            result.valid = false;
        } else if (!ValidateFile(param1)) {
            result.errors.push_back("Executable file does not exist: " + param1);
            result.valid = false;
        }
    } else if (target_type == "dll") {
        if (param1.empty()) {
            result.errors.push_back("DLL target requires path");
            result.valid = false;
        } else if (!ValidateFile(param1)) {
            result.errors.push_back("DLL file does not exist: " + param1);
            result.valid = false;
        }
        if (param2.empty()) {
            result.errors.push_back("DLL target requires function name");
            result.valid = false;
        }
    } else if (target_type == "network") {
        if (param1.empty()) {
            result.errors.push_back("Network target requires address:port");
            result.valid = false;
        } else if (param1.find(':') == std::string::npos) {
            result.errors.push_back("Network target requires address:port format");
            result.valid = false;
        }
    } else {
        result.errors.push_back("Unknown target type: " + target_type);
        result.valid = false;
    }
    
    return result;
}

} // namespace cli
} // namespace winuzzf
