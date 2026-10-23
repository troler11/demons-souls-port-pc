# Starts the game.
#
#   .\scripts\windows\play.ps1 -Rpcs3 "C:\path\to\rpcs3"   # first time (remembered)
#   .\scripts\windows\play.ps1                             # afterwards
#   .\scripts\windows\play.ps1 -Fps30                      # the stock 30 fps game
#   .\scripts\windows\play.ps1 -Uncapped                   # no 60 fps cap (experimental:
#                                                          # the game was made for 30/60)
#   .\scripts\windows\play.ps1 -Resolution 1080p           # window size: 720p, 1080p,
#                                                          # 1440p, 4k or e.g. 2560x1080
#   .\scripts\windows\play.ps1 -Resolution 4k -Scale 3     # internal resolution: the
#                                                          # scene renders 1..4x 1280x720
#                                                          # (default: matches -Resolution)
#   .\scripts\windows\play.ps1 -Fullscreen                 # borderless fullscreen
#                                                          # (F11 / Alt+Enter toggle in game)
#
# Saves go to work\gamedata\dev_hdd0. The log goes to work\logs\play.err.
param([string]$Rpcs3 = "", [switch]$Fps30, [switch]$Uncapped, [string]$Resolution = "",
      [int]$Scale = 0, [switch]$Fullscreen)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\env.ps1"
if ($Fps30) { $env:DES_FPS_UNLOCK = "0" }
if ($Uncapped) { $env:LD_PRESENT_MIN_US = "0" }
if ($Resolution) {
    $env:DES_WINDOW = $Resolution
    if ($Scale -eq 0) {
        # Render at least as many pixels as the window shows.
        $h = switch -regex ($Resolution) { '^1080p$' { 1080 } '^(1440p|2k)$' { 1440 } '^(4k|2160p)$' { 2160 }
                                            '^\d+x(\d+)$' { [int]$matches[1] } default { 720 } }
        $Scale = [math]::Min(4, [math]::Ceiling($h / 720.0))
    }
}
if ($Scale -gt 1) { $env:LD_RES_SCALE = "$Scale" }
if ($Fullscreen) { $env:DES_FULLSCREEN = "1" }
Set-DesEnv $Rpcs3
$logs = Join-Path $Work "logs"
New-Item -ItemType Directory -Force $logs | Out-Null
$p = Start-Process -FilePath $Exe -ArgumentList "`"$Eboot`"" -WorkingDirectory $Work `
    -RedirectStandardOutput "$logs\play.out" -RedirectStandardError "$logs\play.err" -PassThru
$p.WaitForExit()
"exited with $($p.ExitCode)"
