#!/bin/bash
export PATH=/mingw64/bin:$PATH
cd "$(dirname "$0")"
export CONFIG_WINDOWS=y
export CC=gcc
gcc --version
echo "Building Q2Pro..."
make clean
make q2pro.exe CONFIG_WINDOWS=1 CC=gcc -j$(nproc) > build.log 2>&1
