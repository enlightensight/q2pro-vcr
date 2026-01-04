#!/bin/bash
export PATH="/tmp/llvm-mingw-20231128-ucrt-ubuntu-20.04-x86_64/bin:$PATH"

echo "Checking compiler..."
i686-w64-mingw32-gcc --version

echo "Cleaning..."
make clean

echo "Building..."
make q2pro_update.exe TARG_c=q2pro_update.exe CONFIG_WINDOWS=1 CPU=x86 CC=i686-w64-mingw32-gcc WINDRES=i686-w64-mingw32-windres CONFIG_NO_ZLIB=1 -j4 > build_cross.log 2>&1

EXIT_CODE=$?
echo "Build finished with exit code: $EXIT_CODE"
exit $EXIT_CODE
