@echo off
rem Run this from cmd.exe:  tools\env.cmd
rem Provides clang/clang++, cmake and ninja for the CursorBeacon build.

set "TOOLCHAIN_ROOT=%LOCALAPPDATA%\toolchains"
set "LLVM_MINGW=%TOOLCHAIN_ROOT%\llvm-mingw-20261020-ucrt-x86_64"
set "CMAKE_HOME=%TOOLCHAIN_ROOT%\cmake-4.4.5-windows-x86_64"
set "NINJA_HOME=%TOOLCHAIN_ROOT%\ninja-1.13.2"
set "GH_HOME=%TOOLCHAIN_ROOT%\gh-2.103.3"

set "PATH=%NINJA_HOME%;%CMAKE_HOME%\bin;%LLVM_MINGW%\bin;%GH_HOME%\bin;%PATH%"
set "CC=%LLVM_MINGW%\bin\clang.exe"
set "CXX=%LLVM_MINGW%\bin\clang++.exe"

echo CursorBeacon toolchain active:
clang++ --version | findstr /b /c:"clang version"
cmake --version | findstr /b /c:"cmake version"
ninja --version
