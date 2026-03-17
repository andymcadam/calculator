# Windows Calculator in C

A minimal, lightweight Windows calculator program written in pure C using the Win32 API.

## Features

- **Tiny Executable**: Compiled exe is typically 20-40 KB
- **Low Memory**: Minimal memory footprint, runs efficiently on any Windows system
- **Basic Operations**: Addition, subtraction, multiplication, division
- **Editing Controls**: Full clear, clear entry, and backspace support
- **Decimal Support**: Full floating-point number support
- **Classic Interface**: Simple button layout similar to Windows 3.11 calculator

## Button Layout

```text
C   CE  <- /
7   8   9  *
4   5   6  -
1   2   3  +
0   .   =
```

## Compilation

### Option 1: MSVC (Visual Studio / Build Tools)

```batch
build.bat
```

Or manually:

```cmd
cl /O1 /Os calc.c user32.lib kernel32.lib /link /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup
```

### Option 2: MinGW/GCC

```batch
build_mingw.bat
```

Or manually:

```cmd
gcc -mwindows calc.c -o calc.exe -luser32 -lkernel32 -Os -s
```

## Usage

Simply run `calc.exe` - no installation required!

Keyboard shortcuts:

- `Esc`: Clear all
- `Delete`: Clear current entry
- `Backspace`: Delete the last digit

## File Size

- **MSVC compiled (release)**: ~20-25 KB
- **MinGW compiled (stripped)**: ~30-40 KB

## Code Highlights

- Pure Win32 API (no dependencies beyond system libraries)
- No CRT bloat when compiled with MSVC optimizations
- Efficient double-precision floating-point calculations
- Minimal code footprint (~200 lines)

## Compatibility

- Windows XP and later
- x86 and x64 compatible
