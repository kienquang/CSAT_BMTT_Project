@echo off
REM ======== BUILD SCRIPT FOR CSAT_BMTT_PROJECT ========
REM Script này tự động build project bằng CMake

setlocal enabledelayedexpansion

echo.
echo ========== CSAT_BMTT Build System ==========
echo.

REM Kiểm tra CMake đã cài đặt chưa
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake not found! Please install CMake first.
    exit /b 1
)

REM Tạo thư mục build nếu chưa có
if not exist "build" (
    echo [*] Creating build directory...
    mkdir build
)

REM Di chuyển vào thư mục build
cd build

REM Generate Makefile/Project files
echo [*] Generating build files...
cmake .. -G "Visual Studio 16 2019" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake generation failed!
    exit /b 1
)

REM Build project
echo [*] Building project...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo [SUCCESS] Build completed successfully!
echo.
echo Output files:
echo   - Server: .\bin\Server.exe
echo   - Client: .\bin\Client.exe
echo   - TestMasking: .\bin\TestMasking.exe
echo.
echo DLL files have been automatically copied to .\bin\
echo.
pause
