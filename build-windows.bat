@echo off
REM ============================================================
REM  CCDES Build Script for Windows (MSYS2 MinGW64)
REM  Run this from the MSYS2 MinGW64 terminal or CMD after
REM  adding MinGW64 bin directory to PATH.
REM ============================================================

echo Building CCDES...

gcc -Wall -Wextra -O2 -o ccdes.exe main.c download.c parser.c reconstruct.c -lcurl -lws2_32

if %ERRORLEVEL% EQU 0 (
    echo Build successful! Run with: ccdes.exe ^<url^>
) else (
    echo Build failed. Make sure you have:
    echo   1. MSYS2 installed: https://www.msys2.org/
    echo   2. MinGW toolchain: pacman -S mingw-w64-x86_64-gcc
    echo   3. libcurl:         pacman -S mingw-w64-x86_64-curl
)
