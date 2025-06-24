@echo off
echo Building WinFuzz...

REM Check if Visual Studio is available
where cl >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Visual Studio compiler not found. Please run from VS Developer Command Prompt.
    pause
    exit /b 1
)

REM Check if CMake is available
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo CMake not found. Please install CMake and add it to PATH.
    pause
    exit /b 1
)

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring with CMake...
cmake .. -G "Visual Studio 16 2019" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo.
echo Executables are in: build\bin\Release\
echo.
echo To run examples:
echo   cd build\bin\Release
echo   api_fuzzing_example.exe
echo   driver_fuzzing_example.exe
echo.
echo To run main fuzzer:
echo   winuzzf.exe --help
echo.

pause
