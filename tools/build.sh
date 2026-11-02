#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

source tools/env.sh >/dev/null

BUILD_DIR="${1:-build}"
CMAKE_RC_COMPILER="$(cygpath -m "$LLVM_MINGW")/bin/windres.exe"

cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DCMAKE_RC_COMPILER="$CMAKE_RC_COMPILER" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR"

CTEST_LOG="$BUILD_DIR/ctest.log"
if ctest --test-dir "$BUILD_DIR" --output-on-failure >"$CTEST_LOG" 2>&1; then
    cat "$CTEST_LOG"
else
    cat "$CTEST_LOG"
    if grep -qiE "Device Guard|Process not started|BAD_COMMAND|应用程序控制策略" "$CTEST_LOG"; then
        echo "ctest was blocked by the application control policy; running built-in self-test"
        "$BUILD_DIR/cursorbeacon.exe" --self-test
    else
        exit 1
    fi
fi

echo "binary: $ROOT/$BUILD_DIR/cursorbeacon.exe"
