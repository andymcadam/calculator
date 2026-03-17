@echo off
REM Compile with MinGW (GCC) - minimal size
REM Requirements: MinGW/TDM-GCC installed

echo Compiling Calculator with MinGW...
windres app.rc -O coff -o app.res
if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed. Make sure windres is installed and on PATH.
    pause
    exit /b %ERRORLEVEL%
)

gcc -mwindows calc.c app.res -o calc.exe -luser32 -lkernel32 -Os -s

rem gcc -mwindows calc.c app.res -o calc.exe -Os -s -ffunction-sections -fdata-sections -Wl,--gc-sections -luser32 -lkernel32

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilation successful!
    echo Executable: calc.exe
) else (
    echo Compilation failed. Make sure you have MinGW/GCC installed.
)
pause
