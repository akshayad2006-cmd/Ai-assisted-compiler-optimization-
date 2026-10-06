#!/usr/bin/env bash
# Builds the project into the build/ folder.
set -e

# Always run from the project root, wherever the script is called from.
cd "$(dirname "$0")/.."

# On macOS we force Apple's own compiler, because Homebrew's LLVM is first in
# PATH and mixing the two compilers can cause confusing errors.
EXTRA_ARGS=""
if [ "$(uname)" = "Darwin" ]; then
    EXTRA_ARGS="-DCMAKE_CXX_COMPILER=/usr/bin/clang++"
fi

cmake -S . -B build $EXTRA_ARGS
cmake --build build
echo "Build finished: build/aicompiler"
