@echo off
REM Compile with MSVC - minimal size and Fast Link
REM Requirements: Visual Studio or MSVC Build Tools installed

echo Compiling Calculator...
rc /nologo app.rc
if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed. Ensure rc.exe is available in your MSVC environment.
    pause
    exit /b %ERRORLEVEL%
)

cl /O1 /Os calc.c app.res user32.lib kernel32.lib /link /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilation successful!
    echo Executable: calc.exe
) else (
    echo Compilation failed. Make sure you have MSVC Build Tools installed.
    echo You can also compile with:
    echo   windres app.rc -O coff -o app.res ^&^& gcc -mwindows calc.c app.res -o calc.exe -luser32 -lkernel32 -Os
)
pause
