@echo off
REM ======== BUILD SCRIPT FOR PERSONAL RECORD VAULT ========

setlocal enabledelayedexpansion

echo.
echo ========== Personal Record Vault Build ==========
echo.

where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake not found! Please install CMake first.
    exit /b 1
)

if not exist "Build" (
    echo [*] Creating Build directory...
    mkdir Build
)

cd Build

if not exist "CMakeCache.txt" (
    echo [*] Configuring project...
    cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_QT_GUI=ON
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] CMake configure failed!
        exit /b 1
    )
)

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
echo   - Server: .\bin\Release\Server.exe
echo   - GUI:    .\bin\Release\CSATApp.exe
echo.
pause
