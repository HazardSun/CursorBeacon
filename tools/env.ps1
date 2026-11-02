# Dot-source this from PowerShell:  . .\tools\env.ps1
# Provides clang/clang++, cmake and ninja for the CursorBeacon build.

$ToolchainRoot = Join-Path $env:LOCALAPPDATA 'toolchains'
$env:LLVM_MINGW = Join-Path $ToolchainRoot 'llvm-mingw-20261020-ucrt-x86_64'
$env:CMAKE_HOME = Join-Path $ToolchainRoot 'cmake-4.4.5-windows-x86_64'
$env:NINJA_HOME = Join-Path $ToolchainRoot 'ninja-1.13.2'
$env:GH_HOME = Join-Path $ToolchainRoot 'gh-2.103.3'

$env:Path = "$($env:NINJA_HOME);$($env:CMAKE_HOME)\bin;$($env:LLVM_MINGW)\bin;$($env:GH_HOME)\bin;$($env:Path)"
$env:CC = Join-Path $env:LLVM_MINGW 'bin\clang.exe'
$env:CXX = Join-Path $env:LLVM_MINGW 'bin\clang++.exe'

Write-Host 'CursorBeacon toolchain active:'
Write-Host "  clang++: $(& $env:CXX --version | Select-Object -First 1)"
Write-Host "  cmake:   $(& (Join-Path $env:CMAKE_HOME 'bin\cmake.exe') --version | Select-Object -First 1)"
Write-Host "  ninja:   $(& (Join-Path $env:NINJA_HOME 'ninja.exe') --version)"
