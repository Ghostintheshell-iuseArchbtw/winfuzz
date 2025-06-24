#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <sstream>

namespace winuzzf {

class Logger::Impl {
public:
    Impl() 
        : log_level_(LogLevel::Info)
        , max_file_size_(10 * 1024 * 1024) // 10MB default
        , console_output_(true)
        , log_dir_("logs")
        , current_file_size_(0) 
    {
        OpenLogFile();
    }
    
    ~Impl() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
    
    void SetLogDirectory(const std::string& dir) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_dir_ = dir;
        std::filesystem::create_directories(log_dir_);
        OpenLogFile();
    }
    
    void SetLogLevel(LogLevel level) {
        log_level_ = level;
    }
    
    void SetMaxFileSize(size_t max_size_bytes) {
        max_file_size_ = max_size_bytes;
    }
    
    void EnableConsoleOutput(bool enable) {
        console_output_ = enable;
    }
    
    void Log(LogLevel level, const std::string& message) {
        if (level < log_level_) {
            return; // Skip messages below the current log level
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string formatted_message = FormatMessage(level, message);
        
        // Write to console if enabled
        if (console_output_) {
            if (level >= LogLevel::Error) {
                std::cerr << formatted_message << std::endl;
            } else {
                std::cout << formatted_message << std::endl;
            }
        }
        
        // Write to file
        if (log_file_.is_open()) {
            log_file_ << formatted_message << std::endl;
            log_file_.flush();
            
            current_file_size_ += formatted_message.length() + 1; // +1 for newline
            
            // Check if we need to rotate the log file
            if (current_file_size_ > max_file_size_) {
                RotateLogFile();
            }
        }
    }

private:
    LogLevel log_level_;
    size_t max_file_size_;
    bool console_output_;
    std::string log_dir_;
    std::ofstream log_file_;
    size_t current_file_size_;
    std::mutex mutex_;
    
    std::string FormatMessage(LogLevel level, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        ss << " [" << GetLevelString(level) << "] " << message;
        
        return ss.str();
    }
    
    std::string GetLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Critical: return "CRIT";
            default: return "UNKNOWN";
        }
    }
    
    void OpenLogFile() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
        
        // Generate log filename with timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream filename;
        filename << log_dir_ << "/winuzzf_" 
                << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") 
                << ".log";
        
        log_file_.open(filename.str(), std::ios::app);
        current_file_size_ = 0;
        
        if (log_file_.is_open()) {
            log_file_ << "=== WinFuzz Log Started ===" << std::endl;
        }
    }
    
    void RotateLogFile() {
        if (log_file_.is_open()) {
            log_file_ << "=== Log Rotated ===" << std::endl;
            log_file_.close();
        }
        
        OpenLogFile();
    }
};

Logger::Logger() : pImpl(std::make_unique<Impl>()) {}
Logger::~Logger() = default;

void Logger::SetLogDirectory(const std::string& dir) {
    pImpl->SetLogDirectory(dir);
}

void Logger::SetLogLevel(LogLevel level) {
    pImpl->SetLogLevel(level);
}

void Logger::SetMaxFileSize(size_t max_size_bytes) {
    pImpl->SetMaxFileSize(max_size_bytes);
}

void Logger::EnableConsoleOutput(bool enable) {
    pImpl->EnableConsoleOutput(enable);
}

void Logger::LogDebug(const std::string& message) {
    pImpl->Log(LogLevel::Debug, message);
}

void Logger::LogInfo(const std::string& message) {
    pImpl->Log(LogLevel::Info, message);
}

void Logger::LogWarning(const std::string& message) {
    pImpl->Log(LogLevel::Warning, message);
}

void Logger::LogError(const std::string& message) {
    pImpl->Log(LogLevel::Error, message);
}

void Logger::LogCritical(const std::string& message) {
    pImpl->Log(LogLevel::Critical, message);
}

void Logger::Log(LogLevel level, const std::string& message) {
    pImpl->Log(level, message);
}

} // namespace winuzzf
