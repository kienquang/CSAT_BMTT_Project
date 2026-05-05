@echo off
REM ======== RUN GUI CLIENT ========

set CLIENT_PATH=.\Build\bin\Release\CSATApp.exe

if not exist "%CLIENT_PATH%" (
    echo [ERROR] CSATApp.exe not found at %CLIENT_PATH%
    echo Please build the project first using build.bat
    pause
    exit /b 1
)

echo.
echo ========== Starting GUI ==========
echo.
echo GUI executable: %CLIENT_PATH%
echo.

"%CLIENT_PATH%"
