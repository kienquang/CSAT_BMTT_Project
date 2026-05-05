@echo off
REM ======== RUN SERVER ========

set SERVER_PATH=.\Build\bin\Release\Server.exe

if not exist "%SERVER_PATH%" (
    echo [ERROR] Server.exe not found at %SERVER_PATH%
    echo Please build the project first using build.bat
    pause
    exit /b 1
)

echo.
echo ========== Starting Server ==========
echo.
echo Server executable: %SERVER_PATH%
echo.

"%SERVER_PATH%"
