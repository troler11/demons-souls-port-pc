# Configures and builds the port (build\MyGameRecomp.exe, see port\CMakeLists.txt)
# from the patched ps3recomp and the C++ prepare.ps1 generated.
#
#   .\scripts\windows\build.ps1            # configure (first time) + build
#   .\scripts\windows\build.ps1 -Clean     # reconfigure from scratch
#   .\scripts\windows\build.ps1 -Jobs 8    # cap parallel compiles (memory)
#
# Needs Visual Studio 2022 or newer with "Desktop development with C++" and the
# "C++ Clang tools for Windows" component (clang-cl), CMake and Ninja (both
# ship with Visual Studio). A first build compiles ~2.4 GB of generated C++:
# 30-60 minutes and 16 GB of memory or more; -Jobs lowers the peak.
param([switch]$Clean, [int]$Jobs = 0)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$kit = Join-Path $root "third_party\ps3recomp"
$game = Join-Path $root "work\game"
$lle = Join-Path $root "work\lle"
$build = Join-Path $root "build"
if (-not (Test-Path "$game\recompiled\ppu_recomp_000.cpp")) { throw "run scripts\windows\prepare.ps1 first" }

# Developer environment (cl/clang-cl, cmake, ninja on PATH).
if (-not (Get-Command clang-cl -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Visual Studio not found" }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "Visual Studio with the C++ tools not found" }
    $vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
    cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2]) }
    }
    if (-not (Get-Command clang-cl -ErrorAction SilentlyContinue)) { throw "clang-cl not found: install the 'C++ Clang tools for Windows' component" }
}

if ($Clean -and (Test-Path $build)) { Remove-Item -Recurse -Force $build }
if (-not (Test-Path "$build\build.ninja")) {
    $spu = @("$game\EBOOT.ELF") + (Get-ChildItem "$game\jobs" -Filter *.elf | ForEach-Object { $_.FullName }) + @("$lle\libsre.prx")
    $spu = ($spu | ForEach-Object { $_ -replace '\\', '/' }) -join ';'
    cmake -S "$root\port" -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl `
        "-DPS3RECOMP_DIR=$($kit -replace '\\','/')" `
        "-DRECOMP_DIR=$(("$game\recompiled") -replace '\\','/')" `
        "-DDES_LIBSRE_DIR=$($lle -replace '\\','/')" `
        -DPORT_ENABLE_SPU=ON "-DPORT_SPU_INPUTS=$spu"
    if ($LASTEXITCODE) { throw "cmake configure failed" }
}
$t = Get-Date
if ($Jobs -gt 0) { ninja -C $build -j $Jobs } else { ninja -C $build }
if ($LASTEXITCODE) { throw "build failed" }
"build done in $([int]((Get-Date)-$t).TotalMinutes) min: $build\MyGameRecomp.exe"
