@echo off
REM Enhanced build script for WinFuzz v2.0

echo ============================================
echo WinFuzz v2.0 - Advanced Windows Fuzzer
echo ============================================
echo.

REM Check for required tools
echo Checking prerequisites...

REM Check for CMake
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ CMake not found. Please install CMake 3.16+
    exit /b 1
)

echo ✅ CMake found

REM Check for Visual Studio
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ MSBuild not found. Please install Visual Studio 2019+
    exit /b 1
)

echo ✅ Visual Studio Build Tools found

REM Create build directory
echo Setting up build environment...
if exist build (
    echo 🧹 Cleaning existing build directory...
    rmdir /s /q build
)

mkdir build
cd build

echo ✅ Build directory created

REM Configure project
echo Configuring project...
cmake .. -G "Visual Studio 16 2019" -A x64

if %errorlevel% neq 0 (
    echo ❌ CMake configuration failed
    exit /b 1
)

echo ✅ Project configured

REM Build project
echo Building WinFuzz...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo ❌ Build failed
    exit /b 1
)

echo ✅ Build completed successfully

REM Check if executable exists
if exist "bin\Release\winuzzf.exe" (
    echo ✅ WinFuzz executable created
) else if exist "bin\winuzzf.exe" (
    echo ✅ WinFuzz executable created
) else (
    echo ⚠️  WinFuzz executable not found in expected location
)

echo.
echo ============================================
echo 🎉 WinFuzz v2.0 build complete!
echo ============================================
echo.
echo Usage examples:
echo   bin\Release\winuzzf.exe --help
echo   bin\Release\winuzzf.exe --examples
echo   bin\Release\winuzzf.exe --target-api kernel32.dll CreateFileW --corpus corpus
echo.
echo For more information, see README.md

cd ..
pause
