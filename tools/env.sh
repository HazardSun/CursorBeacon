#!/usr/bin/env bash
# Source this from Git Bash:  source tools/env.sh
# Provides clang/clang++, cmake and ninja for the CursorBeacon build.

if command -v cygpath >/dev/null 2>&1; then
    TOOLCHAIN_ROOT="$(cygpath -u "${LOCALAPPDATA}")/toolchains"
else
    TOOLCHAIN_ROOT="${LOCALAPPDATA}/toolchains"
fi

export LLVM_MINGW="${TOOLCHAIN_ROOT}/llvm-mingw-20261020-ucrt-x86_64"
export CMAKE_HOME="${TOOLCHAIN_ROOT}/cmake-4.4.5-windows-x86_64"
export NINJA_HOME="${TOOLCHAIN_ROOT}/ninja-1.13.2"
export GH_HOME="${TOOLCHAIN_ROOT}/gh-2.103.3"

export PATH="${NINJA_HOME}:${CMAKE_HOME}/bin:${LLVM_MINGW}/bin:${GH_HOME}/bin:${PATH}"
export CC="${LLVM_MINGW}/bin/clang.exe"
export CXX="${LLVM_MINGW}/bin/clang++.exe"

echo "CursorBeacon toolchain active:"
echo "  clang++: $("${CXX}" --version | head -1)"
echo "  cmake:   $("${CMAKE_HOME}/bin/cmake.exe" --version | head -1)"
echo "  ninja:   $("${NINJA_HOME}/ninja.exe" --version)"
