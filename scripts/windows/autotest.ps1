# Boots the game for a fixed time with scripted input -- no one at the
# controller -- and keeps the log, for testing changes.
#
#   .\scripts\windows\autotest.ps1 -Seconds 385 -Tag r1 [-Route courtyard] [-Save work\save_base]
#
# Routes (PAD_SCRIPT button presses + PAD_PATH stick segments, wall-clock seconds;
# sticks are 0-255 with 128 centred):
#   menus      menus only: New Game / Continue confirmations up to ~260 s
#   courtyard  from a save in the tutorial's first corridor: corridor ->
#              courtyard -> wooden gate fight (docs/AUTOTEST.md)
# -Save restores that savedata folder first, so every run starts in the same
# place (the game autosaves; without it each run continues the last one).
# Logs: work\logs\<Tag>.out / .err ([fps] lines every 5 s).
param([int]$Seconds = 120, [string]$Tag = "run", [string]$Route = "menus", [string]$Save = "")
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\env.ps1"
# Tests skip the movies unless asked (SAIL_HOST_PLAY=1): minutes saved per run.
if (-not $env:SAIL_HOST_PLAY) { $env:SAIL_HOST_PLAY = "0" }
Set-DesEnv ""

$menus = "25:0x4000,35:0x4000,45:0x4000,55:0x4000,65:0x4000,75:0x4000,85:0x4000,130:0x0008,150:0x0008,170:0x4000,185:0x4000,200:0x4000,215:0x4000,230:0x4000,245:0x4000,260:0x4000"
if (-not $env:PAD_SCRIPT) {
    $env:PAD_SCRIPT = $menus
    if ($Route -eq "courtyard") { $env:PAD_SCRIPT += ",304:0x0800,306:0x0800,308:0x0800,310:0x0800,312:0x0800,314:0x0800" }
}
if (-not $env:PAD_PATH -and $Route -eq "courtyard") {
    $env:PAD_PATH = "270-302:128,0,128,128;302-316:128,128,128,128;316-322:255,128,128,128;322-345:128,0,128,128;345-350:0,128,128,128;350-380:128,0,128,128"
}
if ($Save) {
    $sv = Join-Path $Work "gamedata\dev_hdd0\home\00000001\savedata"
    if (Test-Path $sv) { Remove-Item -Recurse -Force $sv }
    New-Item -ItemType Directory -Force (Split-Path $sv) | Out-Null
    Copy-Item -Recurse $Save $sv
}
$logs = Join-Path $Work "logs"
New-Item -ItemType Directory -Force $logs | Out-Null
$out = "$logs\$Tag.out"; $err = "$logs\$Tag.err"
$p = Start-Process -FilePath $Exe -ArgumentList "`"$Eboot`"" -WorkingDirectory $Work `
    -RedirectStandardOutput $out -RedirectStandardError $err -PassThru
if (-not $p.WaitForExit($Seconds * 1000)) { Stop-Process $p -Force; "still running after ${Seconds}s (stopped)" }
else { "exited with $($p.ExitCode)" }
$fps = Select-String -Path $err -Pattern '^\[fps\] ([0-9.]+)' | Select-Object -Last 12 | ForEach-Object { $_.Matches[0].Groups[1].Value }
"fps (last minute): $($fps -join ' ')"
