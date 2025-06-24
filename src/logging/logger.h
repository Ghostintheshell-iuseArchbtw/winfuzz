#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <mutex>

namespace winuzzf {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    Logger();
    ~Logger();
    
    void SetLogDirectory(const std::string& dir);
    void SetLogLevel(LogLevel level);
    void SetMaxFileSize(size_t max_size_bytes);
    void EnableConsoleOutput(bool enable);
    
    void LogDebug(const std::string& message);
    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    void LogCritical(const std::string& message);
    
    void Log(LogLevel level, const std::string& message);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace winuzzf
