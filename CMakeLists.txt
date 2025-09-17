cmake_minimum_required(VERSION 3.16)
project(winuzzf VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
    add_compile_definitions(_WIN32_WINNT=0x0A00)  # Windows 10
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Core library
add_subdirectory(src/core)
add_subdirectory(src/mutators)
add_subdirectory(src/coverage)
add_subdirectory(src/targets)
add_subdirectory(src/sandbox)
add_subdirectory(src/crash)
add_subdirectory(src/logging)
add_subdirectory(src/corpus)

# Examples
add_subdirectory(examples)

# Tools
add_subdirectory(tools)

# Tests
enable_testing()
add_subdirectory(tests)

# Main executable
add_executable(winuzzf src/main.cpp src/utils.cpp src/cli_ui.cpp)
target_link_libraries(winuzzf 
    winuzzf_core
    winuzzf_mutators
    winuzzf_coverage
    winuzzf_targets
    winuzzf_sandbox
    winuzzf_crash
    winuzzf_logging
    winuzzf_corpus
)

# Link Windows libraries
if(WIN32)
    target_link_libraries(winuzzf
        kernel32
        user32
        advapi32
        dbghelp
        psapi
        ntdll
        wintrust
        crypt32
        bcrypt
    )
endif()

# Install targets
install(TARGETS winuzzf DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(FILES README.md LICENSE DESTINATION .)
