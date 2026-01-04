#!/bin/bash
export PATH=/mnt/c/mingw64/bin:$PATH
echo "Starting build..."
make clean
# Attempting build with detected Windows compiler
make q2pro.exe CONFIG_WINDOWS=1 CPU=x86 CC=gcc.exe WINDRES=windres.exe -j4 > build_attempt.log 2>&1
EXIT_CODE=$?
echo "Build finished with exit code: $EXIT_CODE"
exit $EXIT_CODE
