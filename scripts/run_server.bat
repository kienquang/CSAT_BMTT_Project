@echo off
REM ======== RUN SERVER ========

set SERVER_PATH=.\build\bin\Server.exe

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

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Server ran successfully
) else (
    echo.
    echo [ERROR] Server exited with error code %ERRORLEVEL%
)

pause
