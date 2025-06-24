#include "winuzzf.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace winuzzf {
namespace utils {

std::vector<uint8_t> ReadFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
}

void WriteFile(const std::string& filename, const std::vector<uint8_t>& data) {
    // Create directory if it doesn't exist
    std::filesystem::path path(filename);
    std::filesystem::create_directories(path.parent_path());
    
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to create file: " + filename);
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
}

std::string GetExecutablePath() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return std::string(path);
}

std::string GetModulePath(HMODULE module) {
    char path[MAX_PATH];
    GetModuleFileNameA(module, path, MAX_PATH);
    return std::string(path);
}

bool IsProcessRunning(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return false;
    }
    
    DWORD exit_code;
    BOOL result = GetExitCodeProcess(process, &exit_code);
    CloseHandle(process);
    
    return result && exit_code == STILL_ACTIVE;
}

std::string GetLastErrorString() {
    DWORD error = GetLastError();
    if (error == 0) {
        return std::string();
    }
    
    LPSTR message_buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer),
        0,
        nullptr
    );
    
    std::string message(message_buffer, size);
    LocalFree(message_buffer);
    
    return message;
}

std::string BytesToHex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (uint8_t byte : data) {
        ss << std::setw(2) << static_cast<unsigned>(byte);
    }
    
    return ss.str();
}

std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    
    if (hex.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have even length");
    }
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_string = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::strtoul(byte_string.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    
    return bytes;
}

uint64_t HashData(const std::vector<uint8_t>& data) {
    // Simple FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    
    for (uint8_t byte : data) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    
    return hash;
}

} // namespace utils
} // namespace winuzzf
