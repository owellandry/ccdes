#!/bin/bash
# ============================================================
#  CCDES Build Script for MSYS2 MinGW64 Shell
#  Run this inside the MSYS2 MinGW64 terminal.
# ============================================================

set -e

echo "Building CCDES for Windows..."

gcc -Wall -Wextra -O2 -o ccdes.exe \
    main.c download.c parser.c reconstruct.c \
    -lcurl -lws2_32

echo "Build successful! Run with: ./ccdes.exe <url>"
