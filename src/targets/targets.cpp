#include "winuzzf.h"
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <utility>

#ifdef _WIN32
#    include <windows.h>
#    include <winsock2.h>
#    include <ws2tcpip.h>

#    pragma comment(lib, "ws2_32.lib")
#endif

namespace winuzzf {

#ifdef _WIN32

// API Target Implementation
class APITarget::Impl {
public:
    Impl(const std::string& module, const std::string& function)
        : module_name_(module), function_name_(function), module_handle_(nullptr), function_ptr_(nullptr) {}
    
    ~Impl() {
        Cleanup();
    }
    
    void Setup() {
        // Load the module
        module_handle_ = LoadLibraryA(module_name_.c_str());
        if (!module_handle_) {
            throw std::runtime_error("Failed to load module: " + module_name_);
        }
        
        // Get function address
        function_ptr_ = GetProcAddress(module_handle_, function_name_.c_str());
        if (!function_ptr_) {
            throw std::runtime_error("Failed to find function: " + function_name_);
        }
    }
    
    void Cleanup() {
        if (module_handle_) {
            FreeLibrary(module_handle_);
            module_handle_ = nullptr;
        }
        function_ptr_ = nullptr;
    }
      FuzzResult Execute(const std::vector<uint8_t>& input) {
        if (!function_ptr_) {
            return FuzzResult::Error;
        }
        
        // Set up structured exception handling using SetUnhandledExceptionFilter
        LPTOP_LEVEL_EXCEPTION_FILTER oldFilter = SetUnhandledExceptionFilter(
            [](PEXCEPTION_POINTERS) -> LONG { return EXCEPTION_EXECUTE_HANDLER; }
        );
        
        FuzzResult result;
        try {
            // Parse input based on parameter template
            auto params = ParseInputToParameters(input);
            
            // Call the function
            DWORD func_result = CallFunction(params);
            
            // Check result if checker is set
            if (return_checker_ && !return_checker_(func_result)) {
                result = FuzzResult::Error;
            } else {
                result = FuzzResult::Success;
            }
        }
        catch (...) {
            // Any exception occurred
            result = FuzzResult::Crash;
        }
        
        // Restore original exception filter
        SetUnhandledExceptionFilter(oldFilter);
        
        return result;
    }
    
    void SetParameterTemplate(const std::vector<uint32_t>& param_sizes) {
        param_template_ = param_sizes;
    }
    
    void SetReturnValueCheck(std::function<bool(DWORD)> checker) {
        return_checker_ = checker;
    }
    
    std::string GetName() const {
        return module_name_ + "::" + function_name_;
    }

private:
    std::string module_name_;
    std::string function_name_;
    HMODULE module_handle_;
    FARPROC function_ptr_;
    std::vector<uint32_t> param_template_;
    std::function<bool(DWORD)> return_checker_;
    
    std::vector<DWORD_PTR> ParseInputToParameters(const std::vector<uint8_t>& input) {
        std::vector<DWORD_PTR> params;
        size_t offset = 0;
        
        for (size_t i = 0; i < param_template_.size() && offset < input.size(); ++i) {
            uint32_t param_size = param_template_[i];
            
            if (param_size == sizeof(DWORD)) {
                DWORD value = 0;
                if (offset + sizeof(DWORD) <= input.size()) {
                    memcpy(&value, input.data() + offset, sizeof(DWORD));
                    offset += sizeof(DWORD);
                }
                params.push_back(static_cast<DWORD_PTR>(value));
            }
            else if (param_size == sizeof(DWORD_PTR)) {
                DWORD_PTR value = 0;
                if (offset + sizeof(DWORD_PTR) <= input.size()) {
                    memcpy(&value, input.data() + offset, sizeof(DWORD_PTR));
                    offset += sizeof(DWORD_PTR);
                }
                params.push_back(value);
            }
            else {
                // For other sizes, treat as pointer to data
                if (offset < input.size()) {
                    params.push_back(reinterpret_cast<DWORD_PTR>(input.data() + offset));
                    offset += std::min(param_size, static_cast<uint32_t>(input.size() - offset));
                }
            }
        }
        
        return params;
    }
    
    DWORD CallFunction(const std::vector<DWORD_PTR>& params) {
        // This is a simplified version - in reality, you'd need to handle different calling conventions
        // and parameter counts dynamically
        
        switch (params.size()) {
            case 0:
                return reinterpret_cast<DWORD(*)()>(function_ptr_)();
            case 1:
                return reinterpret_cast<DWORD(*)(DWORD_PTR)>(function_ptr_)(params[0]);
            case 2:
                return reinterpret_cast<DWORD(*)(DWORD_PTR, DWORD_PTR)>(function_ptr_)(params[0], params[1]);
            case 3:
                return reinterpret_cast<DWORD(*)(DWORD_PTR, DWORD_PTR, DWORD_PTR)>(function_ptr_)(params[0], params[1], params[2]);
            case 4:
                return reinterpret_cast<DWORD(*)(DWORD_PTR, DWORD_PTR, DWORD_PTR, DWORD_PTR)>(function_ptr_)(
                    params[0], params[1], params[2], params[3]);
            case 5:
                return reinterpret_cast<DWORD(*)(DWORD_PTR, DWORD_PTR, DWORD_PTR, DWORD_PTR, DWORD_PTR)>(function_ptr_)(
                    params[0], params[1], params[2], params[3], params[4]);
            default:
                // For more parameters, you'd need assembly or libffi
                throw std::runtime_error("Too many parameters for simple calling");
        }
    }
};

APITarget::APITarget(const std::string& module, const std::string& function) 
    : pImpl(std::make_unique<Impl>(module, function)) {
}

APITarget::~APITarget() = default;

FuzzResult APITarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void APITarget::Setup() {
    pImpl->Setup();
}

void APITarget::Cleanup() {
    pImpl->Cleanup();
}

std::string APITarget::GetName() const {
    return pImpl->GetName();
}

Architecture APITarget::GetArchitecture() const {
#ifdef _WIN64
    return Architecture::x64;
#else
    return Architecture::x86;
#endif
}

void APITarget::SetParameterTemplate(const std::vector<uint32_t>& param_sizes) {
    pImpl->SetParameterTemplate(param_sizes);
}

void APITarget::SetReturnValueCheck(std::function<bool(DWORD)> checker) {
    pImpl->SetReturnValueCheck(checker);
}

// Driver Target Implementation
class DriverTarget::Impl {
public:
    Impl(const std::string& device_name) 
        : device_name_(device_name), device_handle_(INVALID_HANDLE_VALUE), ioctl_code_(0), use_input_buffer_(true), output_buffer_size_(0) {}
    
    ~Impl() {
        Cleanup();
    }
    
    void Setup() {
        // Open device handle
        device_handle_ = CreateFileA(
            device_name_.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        
        if (device_handle_ == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open device: " + device_name_);
        }
    }
    
    void Cleanup() {
        if (device_handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(device_handle_);
            device_handle_ = INVALID_HANDLE_VALUE;
        }
    }
      FuzzResult Execute(const std::vector<uint8_t>& input) {
        if (device_handle_ == INVALID_HANDLE_VALUE) {
            return FuzzResult::Error;
        }
        
        // Set up structured exception handling using SetUnhandledExceptionFilter
        LPTOP_LEVEL_EXCEPTION_FILTER oldFilter = SetUnhandledExceptionFilter(
            [](PEXCEPTION_POINTERS) -> LONG { return EXCEPTION_EXECUTE_HANDLER; }
        );
        
        FuzzResult result;
        try {
            std::vector<uint8_t> output_buffer(output_buffer_size_);
            DWORD bytes_returned = 0;
            
            BOOL ioctl_result = DeviceIoControl(
                device_handle_,
                ioctl_code_,
                use_input_buffer_ ? const_cast<uint8_t*>(input.data()) : nullptr,
                use_input_buffer_ ? static_cast<DWORD>(input.size()) : 0,
                output_buffer_size_ > 0 ? output_buffer.data() : nullptr,
                static_cast<DWORD>(output_buffer_size_),
                &bytes_returned,
                nullptr
            );
            
            // Even if the IOCTL fails, it's not necessarily a crash
            result = FuzzResult::Success;
        }
        catch (...) {
            result = FuzzResult::Crash;
        }
        
        // Restore original exception filter
        SetUnhandledExceptionFilter(oldFilter);
        
        return result;
    }
    
    void SetIoctlCode(uint32_t ioctl_code) {
        ioctl_code_ = ioctl_code;
    }
    
    void SetInputMethod(bool use_input_buffer) {
        use_input_buffer_ = use_input_buffer;
    }
    
    void SetOutputBuffer(size_t size) {
        output_buffer_size_ = size;
    }
    
    std::string GetName() const {
        return device_name_ + " (IOCTL: 0x" + std::to_string(ioctl_code_) + ")";
    }

private:
    std::string device_name_;
    HANDLE device_handle_;
    uint32_t ioctl_code_;
    bool use_input_buffer_;
    size_t output_buffer_size_;
};

DriverTarget::DriverTarget(const std::string& device_name) 
    : pImpl(std::make_unique<Impl>(device_name)) {
}

DriverTarget::~DriverTarget() = default;

FuzzResult DriverTarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void DriverTarget::Setup() {
    pImpl->Setup();
}

void DriverTarget::Cleanup() {
    pImpl->Cleanup();
}

std::string DriverTarget::GetName() const {
    return pImpl->GetName();
}

Architecture DriverTarget::GetArchitecture() const {
#ifdef _WIN64
    return Architecture::x64;
#else
    return Architecture::x86;
#endif
}

void DriverTarget::SetIoctlCode(uint32_t ioctl_code) {
    pImpl->SetIoctlCode(ioctl_code);
}

void DriverTarget::SetInputMethod(bool use_input_buffer) {
    pImpl->SetInputMethod(use_input_buffer);
}

void DriverTarget::SetOutputBuffer(size_t size) {
    pImpl->SetOutputBuffer(size);
}

// Executable Target Implementation
class ExecutableTarget::Impl {
public:
    Impl(const std::string& exe_path) 
        : exe_path_(exe_path), input_method_("stdin"), working_dir_(".") {}
    
    void Setup() {
        // Verify executable exists
        if (GetFileAttributesA(exe_path_.c_str()) == INVALID_FILE_ATTRIBUTES) {
            throw std::runtime_error("Executable not found: " + exe_path_);
        }
    }
    
    void Cleanup() {
        // Nothing to cleanup for executable target
    }
    
    FuzzResult Execute(const std::vector<uint8_t>& input) {
        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        
        std::string command_line = BuildCommandLine(input);
        
        // Create process
        BOOL result = CreateProcessA(
            exe_path_.c_str(),
            const_cast<char*>(command_line.c_str()),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            working_dir_.c_str(),
            &si,
            &pi
        );
        
        if (!result) {
            return FuzzResult::Error;
        }
        
        // Wait for process with timeout
        DWORD wait_result = WaitForSingleObject(pi.hProcess, 5000); // 5 second timeout
        
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        
        // Clean up handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        switch (wait_result) {
            case WAIT_OBJECT_0:
                // Process completed
                if (exit_code != 0) {
                    return FuzzResult::Crash;
                }
                return FuzzResult::Success;
            case WAIT_TIMEOUT:
                // Process hung
                TerminateProcess(pi.hProcess, 1);
                return FuzzResult::Hang;
            default:
                return FuzzResult::Error;
        }
    }
    
    void SetCommandLineTemplate(const std::string& template_str) {
        cmdline_template_ = template_str;
    }
    
    void SetInputMethod(const std::string& method) {
        input_method_ = method;
    }
    
    void SetWorkingDirectory(const std::string& dir) {
        working_dir_ = dir;
    }
    
    std::string GetName() const {
        return exe_path_;
    }

private:
    std::string exe_path_;
    std::string cmdline_template_;
    std::string input_method_;
    std::string working_dir_;
    
    std::string BuildCommandLine(const std::vector<uint8_t>& input) {
        if (input_method_ == "cmdline") {
            // Replace placeholder in command line template
            std::string cmdline = cmdline_template_;
            // Simple placeholder replacement - in real implementation you'd want more sophisticated parsing
            size_t pos = cmdline.find("%INPUT%");
            if (pos != std::string::npos) {
                std::string input_str(input.begin(), input.end());
                cmdline.replace(pos, 7, input_str);
            }
            return cmdline;
        } else if (input_method_ == "file") {
            // Write input to temporary file and pass filename
            std::string temp_file = "temp_input.bin";
            std::ofstream file(temp_file, std::ios::binary);
            if (file) {
                file.write(reinterpret_cast<const char*>(input.data()), input.size());
            }
            return temp_file;
        }
        
        return ""; // For stdin method, we don't modify command line
    }
};

ExecutableTarget::ExecutableTarget(const std::string& exe_path) 
    : pImpl(std::make_unique<Impl>(exe_path)) {
}

ExecutableTarget::~ExecutableTarget() = default;

FuzzResult ExecutableTarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void ExecutableTarget::Setup() {
    pImpl->Setup();
}

void ExecutableTarget::Cleanup() {
    pImpl->Cleanup();
}

std::string ExecutableTarget::GetName() const {
    return pImpl->GetName();
}

Architecture ExecutableTarget::GetArchitecture() const {
#ifdef _WIN64
    return Architecture::x64;
#else
    return Architecture::x86;
#endif
}

void ExecutableTarget::SetCommandLineTemplate(const std::string& template_str) {
    pImpl->SetCommandLineTemplate(template_str);
}

void ExecutableTarget::SetInputMethod(const std::string& method) {
    pImpl->SetInputMethod(method);
}

void ExecutableTarget::SetWorkingDirectory(const std::string& dir) {
    pImpl->SetWorkingDirectory(dir);
}

// DLL Target Implementation
class DLLTarget::Impl {
public:
    Impl(const std::string& dll_path, const std::string& function_name)
        : dll_path_(dll_path), function_name_(function_name), module_handle_(nullptr), function_ptr_(nullptr) {}
    
    ~Impl() {
        Cleanup();
    }
    
    void Setup() {
        // Load the DLL
        module_handle_ = LoadLibraryA(dll_path_.c_str());
        if (!module_handle_) {
            throw std::runtime_error("Failed to load DLL: " + dll_path_);
        }
        
        // Get function address
        function_ptr_ = GetProcAddress(module_handle_, function_name_.c_str());
        if (!function_ptr_) {
            throw std::runtime_error("Failed to find function: " + function_name_);
        }
    }
    
    void Cleanup() {
        if (module_handle_) {
            FreeLibrary(module_handle_);
            module_handle_ = nullptr;
        }
        function_ptr_ = nullptr;
    }
    
    FuzzResult Execute(const std::vector<uint8_t>& input) {
        if (!function_ptr_) {
            return FuzzResult::Error;
        }
        
        __try {
            // Call the function with the input data
            // This is a simplified implementation - in reality, you'd need to
            // properly marshal the input data according to the function signature
            typedef int (__stdcall *FunctionPtr)(void*, size_t);
            FunctionPtr func = reinterpret_cast<FunctionPtr>(function_ptr_);
            func(const_cast<uint8_t*>(input.data()), input.size());
            return FuzzResult::Success;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            return FuzzResult::Crash;
        }
    }
    
    std::string GetName() const {
        return dll_path_ + "::" + function_name_;
    }
    
private:
    std::string dll_path_;
    std::string function_name_;
    HMODULE module_handle_;
    FARPROC function_ptr_;
};

DLLTarget::DLLTarget(const std::string& dll_path, const std::string& function_name)
    : pImpl(std::make_unique<Impl>(dll_path, function_name)) {
}

DLLTarget::~DLLTarget() = default;

FuzzResult DLLTarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void DLLTarget::Setup() {
    pImpl->Setup();
}

void DLLTarget::Cleanup() {
    pImpl->Cleanup();
}

std::string DLLTarget::GetName() const {
    return pImpl->GetName();
}

Architecture DLLTarget::GetArchitecture() const {
#ifdef _WIN64
    return Architecture::x64;
#else
    return Architecture::x86;
#endif
}

void DLLTarget::SetParameterTemplate(const std::vector<uint32_t>& param_sizes) {
    // Placeholder implementation
}

void DLLTarget::SetCallingConvention(const std::string& convention) {
    // Placeholder implementation
}

// Network Target Implementation
class NetworkTarget::Impl {
public:
    Impl(const std::string& address_port) : address_port_(address_port), socket_(INVALID_SOCKET) {
        // Parse address:port
        auto colon_pos = address_port.find(':');
        if (colon_pos != std::string::npos) {
            address_ = address_port.substr(0, colon_pos);
            port_ = std::stoi(address_port.substr(colon_pos + 1));
        }
    }
    
    ~Impl() {
        Cleanup();
    }
    
    void Setup() {
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
        }
        
        // Create socket
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Failed to create socket");
        }
    }
    
    void Cleanup() {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
        WSACleanup();
    }
    
    FuzzResult Execute(const std::vector<uint8_t>& input) {
        if (socket_ == INVALID_SOCKET) {
            return FuzzResult::Error;
        }
        
        // Connect to target
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port_);
        inet_pton(AF_INET, address_.c_str(), &serverAddr.sin_addr);
        
        if (connect(socket_, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            return FuzzResult::Error;
        }
        
        // Send data
        int result = send(socket_, reinterpret_cast<const char*>(input.data()), 
                          static_cast<int>(input.size()), 0);
        
        // Close connection
        closesocket(socket_);
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        return (result == SOCKET_ERROR) ? FuzzResult::Error : FuzzResult::Success;
    }
    
    std::string GetName() const {
        return "network://" + address_port_;
    }
    
private:
    std::string address_port_;
    std::string address_;
    int port_;
    SOCKET socket_;
};

NetworkTarget::NetworkTarget(const std::string& address_port)
    : pImpl(std::make_unique<Impl>(address_port)) {
}

NetworkTarget::~NetworkTarget() = default;

FuzzResult NetworkTarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void NetworkTarget::Setup() {
    pImpl->Setup();
}

void NetworkTarget::Cleanup() {
    pImpl->Cleanup();
}

std::string NetworkTarget::GetName() const {
    return pImpl->GetName();
}

Architecture NetworkTarget::GetArchitecture() const {
#ifdef _WIN64
    return Architecture::x64;
#else
    return Architecture::x86;
#endif
}

void NetworkTarget::SetProtocol(const std::string& protocol) {
    // Placeholder implementation
}

void NetworkTarget::SetTimeout(uint32_t timeout_ms) {
    // Placeholder implementation
}

#else // _WIN32

namespace {
Architecture DetectHostArchitecture() {
    return sizeof(void*) == 8 ? Architecture::x64 : Architecture::x86;
}
}

class APITarget::Impl {
public:
    Impl(std::string module, std::string function)
        : module_name_(std::move(module)), function_name_(std::move(function)) {}

    void Setup() {
        throw std::runtime_error("APITarget is only supported on Windows builds.");
    }

    void Cleanup() {}

    FuzzResult Execute(const std::vector<uint8_t>&) {
        return FuzzResult::Error;
    }

    void SetParameterTemplate(const std::vector<uint32_t>&) {}

    void SetReturnValueCheck(std::function<bool(DWORD)>) {}

    std::string GetName() const {
        return module_name_ + "::" + function_name_;
    }

private:
    std::string module_name_;
    std::string function_name_;
};

APITarget::APITarget(const std::string& module, const std::string& function)
    : pImpl(std::make_unique<Impl>(module, function)) {}

APITarget::~APITarget() = default;

FuzzResult APITarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void APITarget::Setup() {
    pImpl->Setup();
}

void APITarget::Cleanup() {
    pImpl->Cleanup();
}

std::string APITarget::GetName() const {
    return pImpl->GetName();
}

Architecture APITarget::GetArchitecture() const {
    return DetectHostArchitecture();
}

void APITarget::SetParameterTemplate(const std::vector<uint32_t>& param_sizes) {
    pImpl->SetParameterTemplate(param_sizes);
}

void APITarget::SetReturnValueCheck(std::function<bool(DWORD)> checker) {
    pImpl->SetReturnValueCheck(std::move(checker));
}

class DriverTarget::Impl {
public:
    explicit Impl(std::string device) : device_name_(std::move(device)), ioctl_code_(0) {}

    void Setup() {
        throw std::runtime_error("DriverTarget is only supported on Windows builds.");
    }

    void Cleanup() {}

    FuzzResult Execute(const std::vector<uint8_t>&) {
        return FuzzResult::Error;
    }

    void SetIoctlCode(uint32_t code) { ioctl_code_ = code; }
    void SetInputMethod(bool) {}
    void SetOutputBuffer(size_t) {}

    std::string GetName() const {
        std::ostringstream oss;
        oss << device_name_ << " (IOCTL: 0x" << std::hex << ioctl_code_ << ')';
        return oss.str();
    }

private:
    std::string device_name_;
    uint32_t ioctl_code_;
};

DriverTarget::DriverTarget(const std::string& device_name)
    : pImpl(std::make_unique<Impl>(device_name)) {}

DriverTarget::~DriverTarget() = default;

FuzzResult DriverTarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void DriverTarget::Setup() {
    pImpl->Setup();
}

void DriverTarget::Cleanup() {
    pImpl->Cleanup();
}

std::string DriverTarget::GetName() const {
    return pImpl->GetName();
}

Architecture DriverTarget::GetArchitecture() const {
    return DetectHostArchitecture();
}

void DriverTarget::SetIoctlCode(uint32_t ioctl_code) {
    pImpl->SetIoctlCode(ioctl_code);
}

void DriverTarget::SetInputMethod(bool use_input_buffer) {
    pImpl->SetInputMethod(use_input_buffer);
}

void DriverTarget::SetOutputBuffer(size_t size) {
    pImpl->SetOutputBuffer(size);
}

class ExecutableTarget::Impl {
public:
    explicit Impl(std::string path) : exe_path_(std::move(path)) {}

    void Setup() {
        if (!std::filesystem::exists(exe_path_)) {
            throw std::runtime_error("Executable not found: " + exe_path_);
        }
    }

    void Cleanup() {}

    FuzzResult Execute(const std::vector<uint8_t>&) {
        throw std::runtime_error("Executable fuzzing is only supported on Windows builds.");
    }

    void SetCommandLineTemplate(const std::string& template_str) { cmdline_template_ = template_str; }
    void SetInputMethod(const std::string& method) { input_method_ = method; }
    void SetWorkingDirectory(const std::string& dir) { working_dir_ = dir; }

    std::string GetName() const { return exe_path_; }

private:
    std::string exe_path_;
    std::string cmdline_template_;
    std::string input_method_ = "stdin";
    std::string working_dir_ = ".";
};

ExecutableTarget::ExecutableTarget(const std::string& exe_path)
    : pImpl(std::make_unique<Impl>(exe_path)) {}

ExecutableTarget::~ExecutableTarget() = default;

FuzzResult ExecutableTarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void ExecutableTarget::Setup() {
    pImpl->Setup();
}

void ExecutableTarget::Cleanup() {
    pImpl->Cleanup();
}

std::string ExecutableTarget::GetName() const {
    return pImpl->GetName();
}

Architecture ExecutableTarget::GetArchitecture() const {
    return DetectHostArchitecture();
}

void ExecutableTarget::SetCommandLineTemplate(const std::string& template_str) {
    pImpl->SetCommandLineTemplate(template_str);
}

void ExecutableTarget::SetInputMethod(const std::string& method) {
    pImpl->SetInputMethod(method);
}

void ExecutableTarget::SetWorkingDirectory(const std::string& dir) {
    pImpl->SetWorkingDirectory(dir);
}

class DLLTarget::Impl {
public:
    Impl(std::string dll, std::string function)
        : dll_path_(std::move(dll)), function_name_(std::move(function)) {}

    void Setup() {
        throw std::runtime_error("DLLTarget is only supported on Windows builds.");
    }

    void Cleanup() {}

    FuzzResult Execute(const std::vector<uint8_t>&) {
        return FuzzResult::Error;
    }

    std::string GetName() const { return dll_path_ + "::" + function_name_; }

    void SetParameterTemplate(const std::vector<uint32_t>&) {}
    void SetCallingConvention(const std::string&) {}

private:
    std::string dll_path_;
    std::string function_name_;
};

DLLTarget::DLLTarget(const std::string& dll_path, const std::string& function_name)
    : pImpl(std::make_unique<Impl>(dll_path, function_name)) {}

DLLTarget::~DLLTarget() = default;

FuzzResult DLLTarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void DLLTarget::Setup() {
    pImpl->Setup();
}

void DLLTarget::Cleanup() {
    pImpl->Cleanup();
}

std::string DLLTarget::GetName() const {
    return pImpl->GetName();
}

Architecture DLLTarget::GetArchitecture() const {
    return DetectHostArchitecture();
}

void DLLTarget::SetParameterTemplate(const std::vector<uint32_t>& param_sizes) {
    pImpl->SetParameterTemplate(param_sizes);
}

void DLLTarget::SetCallingConvention(const std::string& convention) {
    pImpl->SetCallingConvention(convention);
}

class NetworkTarget::Impl {
public:
    explicit Impl(std::string address_port) : address_port_(std::move(address_port)) {}

    void Setup() {
        throw std::runtime_error("NetworkTarget is only supported on Windows builds.");
    }

    void Cleanup() {}

    FuzzResult Execute(const std::vector<uint8_t>&) {
        return FuzzResult::Error;
    }

    std::string GetName() const { return "network://" + address_port_; }

    void SetProtocol(const std::string& protocol) { protocol_ = protocol; }
    void SetTimeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }

private:
    std::string address_port_;
    std::string protocol_ = "tcp";
    uint32_t timeout_ms_ = 0;
};

NetworkTarget::NetworkTarget(const std::string& address_port)
    : pImpl(std::make_unique<Impl>(address_port)) {}

NetworkTarget::~NetworkTarget() = default;

FuzzResult NetworkTarget::Execute(const std::vector<uint8_t>& input) {
    return pImpl->Execute(input);
}

void NetworkTarget::Setup() {
    pImpl->Setup();
}

void NetworkTarget::Cleanup() {
    pImpl->Cleanup();
}

std::string NetworkTarget::GetName() const {
    return pImpl->GetName();
}

Architecture NetworkTarget::GetArchitecture() const {
    return DetectHostArchitecture();
}

void NetworkTarget::SetProtocol(const std::string& protocol) {
    pImpl->SetProtocol(protocol);
}

void NetworkTarget::SetTimeout(uint32_t timeout_ms) {
    pImpl->SetTimeout(timeout_ms);
}

#endif // _WIN32

} // namespace winuzzf
