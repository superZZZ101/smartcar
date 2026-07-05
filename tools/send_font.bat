@echo off
setlocal enabledelayedexpansion

set TOOLS_DIR=%~dp0
set SCRIPT=%TOOLS_DIR%gen_send_font.py
set BIN_FILE=%TOOLS_DIR%gb2312_hzk16.bin

if "%~1"=="" (
    echo.
    echo OLED Zi Ku Xie Ru Gong Ju
    echo ======================
    echo.
    echo Yong Fa:  %~nx0 COM Duan Kou [Bo Te Lu]
    echo.
    echo Shi Li:
    echo   %~nx0 COM4
    echo   %~nx0 COM4 38400
    echo.
    pause
    exit /b 1
)

set PORT=%1
set BAUDRATE=%~2
if "%BAUDRATE%"=="" set BAUDRATE=115200

echo.
echo  Port: %PORT%  (%BAUDRATE% 8N1)
echo.

py -3 "%SCRIPT%" --port "%PORT%" --baud %BAUDRATE% --bin-only "%BIN_FILE%" --flash-addr 0

if errorlevel 1 (
    echo.
    echo [ERROR] Write Failed!
    echo.
    pause
    exit /b 1
)

echo.
echo [OK] Font write complete!
echo.
pause
