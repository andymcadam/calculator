@echo off
REM Compile with MSVC - minimal size and Fast Link
REM Requirements: Visual Studio or MSVC Build Tools installed

echo Compiling Calculator...
cl /O1 /Os calc.c user32.lib kernel32.lib /link /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilation successful!
    echo Executable: calc.exe
) else (
    echo Compilation failed. Make sure you have MSVC Build Tools installed.
    echo You can also compile with:
    echo   gcc -mwindows calc.c -o calc.exe -luser32 -lkernel32 -Os
)
pause
