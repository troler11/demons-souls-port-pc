# One-time setup: fetch ps3recomp at the commit this port is built on, apply
# this project's changes to it (patches/ps3recomp.patch), and create the Python
# environment its tools run in.
#
#   .\scripts\windows\setup.ps1
#
# Result: third_party\ps3recomp (patched) and third_party\venv.
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$tp = Join-Path $root "third_party"
$kit = Join-Path $tp "ps3recomp"
$commit = "39a3629"
New-Item -ItemType Directory -Force $tp | Out-Null

if (-not (Test-Path (Join-Path $kit ".git"))) {
    git clone https://github.com/sp00nznet/ps3recomp.git $kit
    if ($LASTEXITCODE) { throw "git clone failed" }
}
Push-Location $kit
try {
    git checkout -q $commit
    if ($LASTEXITCODE) { throw "git checkout $commit failed" }
    # Applying twice would fail; check first.
    git apply --check --whitespace=nowarn (Join-Path $root "patches\ps3recomp.patch") 2>$null
    if ($LASTEXITCODE -eq 0) {
        git apply --whitespace=nowarn (Join-Path $root "patches\ps3recomp.patch")
        if ($LASTEXITCODE) { throw "git apply failed" }
        "ps3recomp: patch applied"
    } else {
        git apply --reverse --check --whitespace=nowarn (Join-Path $root "patches\ps3recomp.patch") 2>$null
        if ($LASTEXITCODE -eq 0) { "ps3recomp: patch already applied" }
        else { throw "the patch neither applies nor is applied: is third_party\ps3recomp at $commit and unmodified?" }
    }
} finally { Pop-Location }

$venv = Join-Path $tp "venv"
if (-not (Test-Path (Join-Path $venv "Scripts\python.exe"))) {
    python -m venv $venv
    if ($LASTEXITCODE) { throw "python -m venv failed (Python 3.10+ on PATH?)" }
}
& (Join-Path $venv "Scripts\python.exe") -m pip install -q -r (Join-Path $kit "requirements.txt")
if ($LASTEXITCODE) { throw "pip install failed" }
"setup done: $kit"
