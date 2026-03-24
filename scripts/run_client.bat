@echo off
REM ======== RUN CLIENT ========

set CLIENT_PATH=.\build\bin\Client.exe

if not exist "%CLIENT_PATH%" (
    echo [ERROR] Client.exe not found at %CLIENT_PATH%
    echo Please build the project first using build.bat
    pause
    exit /b 1
)

echo.
echo ========== Starting Client ==========
echo.
echo Client executable: %CLIENT_PATH%
echo.

"%CLIENT_PATH%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Client ran successfully
) else (
    echo.
    echo [ERROR] Client exited with error code %ERRORLEVEL%
)

pause
