#!/bin/bash
# Q2PRO Build Script for Windows x86 (32-bit)
# This script assumes you are running in an MSYS2 MinGW32 environment 
# or have /mingw32/bin in your path.

# Add MinGW32 to path (adjust if your installation is different)
export PATH=/mingw32/bin:$PATH

cd "$(dirname "$0")"

# Force Windows Configuration
export CONFIG_WINDOWS=y
export CC=gcc
export CPU=x86

echo "--- Checking GCC Version (Should be MinGW32) ---"
gcc -v
if [ $? -ne 0 ]; then
    echo "ERROR: gcc not found! Please make sure you are in a MinGW32 shell or have gcc in your PATH."
    exit 1
fi

echo ""
echo "--- Cleaning previous build ---"
make clean

echo ""
echo "--- Building Q2Pro x86 ---"
make q2pro.exe CONFIG_WINDOWS=1 CPU=x86 CC=gcc -j$(nproc)

if [ $? -eq 0 ]; then
    echo "Build SUCCESS: q2pro.exe created."
else
    echo "Build FAILED."
    exit 1
fi
