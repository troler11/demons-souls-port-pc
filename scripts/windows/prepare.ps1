# Turns YOUR copy of the game and YOUR PS3 firmware into the generated C++ the
# build compiles. Nothing it reads or writes leaves work\ (never commit it).
#
#   .\scripts\windows\prepare.ps1 -Rpcs3 "C:\path\to\rpcs3"
#
# Inputs (see README):
#   work\disc\PS3_GAME\...     the extracted, decrypted disc (BLUS30443)
#   RPCS3 with the official PS3 firmware installed -- used to decrypt
#   EBOOT.BIN and libsre.sprx (or put EBOOT.elf next to EBOOT.BIN and
#   work\lle\libsre.prx there yourself and omit -Rpcs3).
#
# Output:
#   work\game\  EBOOT.ELF, out\ (function tables), recompiled\ (PPU C++, ~2.4 GB),
#               jobs\ (raw SPU programs wrapped as ELFs)
#   work\lle\   libsre.prx, out\ (libsre_image.bin...), recomp\, libsre_exports.inc
param([string]$Rpcs3 = "", [switch]$SkipLift)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$kit = Join-Path $root "third_party\ps3recomp"
$py = Join-Path $root "third_party\venv\Scripts\python.exe"
$work = Join-Path $root "work"
$usr = Join-Path $work "disc\PS3_GAME\USRDIR"
if (-not (Test-Path $py)) { throw "run scripts\windows\setup.ps1 first" }
if (-not (Test-Path (Join-Path $usr "EBOOT.BIN"))) { throw "missing $usr\EBOOT.BIN -- extract your disc into work\disc (README)" }

function Run($exe, [string[]]$argv) {
    & $exe @argv
    if ($LASTEXITCODE) { throw "failed ($LASTEXITCODE): $exe $($argv -join ' ')" }
}
function Decrypt($file) {
    if (-not $Rpcs3) { throw "need -Rpcs3 to decrypt $file" }
    $exe = Join-Path $Rpcs3 "rpcs3.exe"
    & $exe --headless --decrypt $file | Out-Null
}

# 1. EBOOT.BIN (SELF) -> EBOOT.elf
$elf = Join-Path $usr "EBOOT.elf"
if (-not (Test-Path $elf)) {
    "decrypting EBOOT.BIN with RPCS3..."
    Decrypt (Join-Path $usr "EBOOT.BIN")
    if (-not (Test-Path $elf)) { throw "RPCS3 did not write $elf (is the PS3 firmware installed in it?)" }
}

# 2. libsre.sprx from the firmware -> libsre.prx
$lle = Join-Path $work "lle"
New-Item -ItemType Directory -Force $lle | Out-Null
$prx = Join-Path $lle "libsre.prx"
if (-not (Test-Path $prx)) {
    if (-not $Rpcs3) { throw "need -Rpcs3 (or put a decrypted libsre.prx in $lle)" }
    $sprx = Join-Path $Rpcs3 "dev_flash\sys\external\libsre.sprx"
    if (-not (Test-Path $sprx)) { throw "no $sprx -- install the PS3 firmware in RPCS3" }
    Copy-Item $sprx (Join-Path $lle "libsre.sprx")
    "decrypting libsre.sprx with RPCS3..."
    Decrypt (Join-Path $lle "libsre.sprx")
    if (-not (Test-Path $prx)) { throw "RPCS3 did not write $prx" }
}
if ($SkipLift) { "inputs ready"; return }

# 3. PPU: load, add the TOC-switch trampolines, lift, wire the 60 fps hooks.
$game = Join-Path $work "game"
New-Item -ItemType Directory -Force (Join-Path $game "out"), (Join-Path $game "jobs") | Out-Null
Copy-Item $elf (Join-Path $game "EBOOT.ELF") -Force
Push-Location $game
try {
    "loading EBOOT..."
    Run $py @("$kit\tools\ppu_loader.py", "EBOOT.ELF", "-o", "out/")
    Run $py @("-I", "$root\scripts\add_toc_trampolines.py", "EBOOT.ELF", "out/EBOOT.functions.json", "out/EBOOT.functions_tramp.json")
    # Jump tables whose base the compiler spilled to the stack, which the
    # lifter's discovery misses (scripts/find_spilled_jt.py finds them).
    $env:JT_EXTRA = "0x127612C:0x1276130:108,0x1277A2C:0x1277A30:108,0x15EFE34:0x15EFE38:29,0x15F1604:0x15F1608:29"
    "lifting the PPU code (a few minutes, ~2.4 GB)..."
    Run $py @("$kit\tools\ppu_lifter.py", "EBOOT.ELF", "--functions", "out/EBOOT.functions_tramp.json",
              "--hle-stubs", "out/EBOOT.imports.json", "--output", "recompiled/")
    Remove-Item Env:JT_EXTRA
    Run $py @("-I", "$root\scripts\apply_fps_unlock.py", "recompiled/ppu_recomp_000.cpp")
    Run $py @("-I", "$root\scripts\apply_title_hooks.py", "recompiled", "EBOOT.ELF")

    # 4. Raw SPU programs (not ELF images inside the EBOOT) wrapped as SPU ELFs.
    Run $py @("-I", "$root\scripts\extract_spu_jobs.py", "EBOOT.ELF", $prx, "jobs")
    Get-ChildItem (Join-Path $game "jobs") -Filter *.json | ForEach-Object {
        $w = Get-Content $_.FullName -Raw | ConvertFrom-Json
        $bin = [IO.Path]::ChangeExtension($_.FullName, ".bin")
        $out = [IO.Path]::ChangeExtension($_.FullName, ".elf")
        $a = @("$kit\tools\wrap_spu_elf.py", $bin, "--base", $w.base, "--entry", $w.entry)
        if ($w.symbols) { $a += @("--symbols", $w.symbols) }
        Run $py ($a + @("--out", $out))
    }
} finally { Pop-Location }

# 5. libsre (the real SPURS kernel): relocate, lift, export table.
Push-Location $lle
try {
    "lifting libsre..."
    Run $py @("$kit\tools\lift_prx.py", "libsre.prx", "--base", "0x30000000", "--output", "out")
    Run $py @("-I", "$root\scripts\gen_libsre_stubs.py", "out/libsre_imports.json", "out/libsre_hle_stubs.json")
    Run $py @("$kit\tools\ppu_lifter.py", "out/libsre_image.bin", "--raw", "--base", "0x30000000",
              "--toc", "0x30039AB0", "--functions", "out/libsre_functions.json",
              "--hle-stubs", "out/libsre_hle_stubs.json", "--symbol-prefix", "libsre_", "--output", "recomp")
    # Its own header name, so it can sit in one build with the game's ppu_recomp.h.
    Move-Item -Force "recomp\ppu_recomp.h" "recomp\libsre_recomp.h"
    $l1 = [Text.Encoding]::GetEncoding(28591)   # byte-for-byte
    $src = Join-Path $lle "recomp\ppu_recomp_000.cpp"
    $c = [IO.File]::ReadAllText($src, $l1).Replace('#include "ppu_recomp.h"', '#include "libsre_recomp.h"')
    [IO.File]::WriteAllText((Join-Path $lle "recomp\libsre_recomp_000.cpp"), $c, $l1)
    Remove-Item $src
    Run $py @("-I", "$root\scripts\gen_libsre_exports.py", "out/libsre_exports.json", "libsre_exports.inc")
} finally { Pop-Location }
"prepare done. Next: .\scripts\windows\build.ps1"
