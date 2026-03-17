@echo off
REM Compile with MinGW (GCC) - minimal size
REM Requirements: MinGW/TDM-GCC installed

echo Compiling Calculator with MinGW...
gcc -mwindows calc.c -o calc.exe -luser32 -lkernel32 -Os -s

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilation successful!
    echo Executable: calc.exe
) else (
    echo Compilation failed. Make sure you have MinGW/GCC installed.
)
pause
