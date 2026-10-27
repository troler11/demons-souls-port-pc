# Shared by play.ps1 and autotest.ps1: the environment the port runs with.
# Every variable is only a default -- set it before calling to override.
# docs/ENVIRONMENT.md lists them all.

$script:Root = (Resolve-Path "$PSScriptRoot\..\..").Path
$script:Work = Join-Path $Root "work"
$script:Exe = Join-Path $Root "build\MyGameRecomp.exe"
$script:Eboot = Join-Path $Work "disc\PS3_GAME\USRDIR\EBOOT.elf"

function Set-DesEnv([string]$Rpcs3) {
    # Remember the RPCS3 folder (its dev_flash serves /dev_flash) between runs.
    $cfg = Join-Path $Work "settings.json"
    if ($Rpcs3) { @{ rpcs3 = $Rpcs3 } | ConvertTo-Json | Set-Content $cfg }
    elseif (Test-Path $cfg) { $Rpcs3 = (Get-Content $cfg -Raw | ConvertFrom-Json).rpcs3 }
    if (-not (Test-Path $Exe)) { throw "no $Exe -- run scripts\windows\build.ps1" }
    if (-not (Test-Path $Eboot)) { throw "no $Eboot -- run scripts\windows\prepare.ps1" }

    # D3D12 renderer, the real SPURS kernel (libsre), one-line logging.
    if (-not $env:RSX_LIVE_DRAW) { $env:RSX_LIVE_DRAW = "1" }
    if (-not $env:DES_LIBSRE)    { $env:DES_LIBSRE = (Join-Path $Work "lle\out\libsre_image.bin") }
    if (-not $env:DES_ONE_LOG)   { $env:DES_ONE_LOG = "1" }
    # The SPU publishes the GCM put assuming the command buffer starts 4 KB into IO.
    if (-not $env:PS3_GCM_BEGIN_OFFSET) { $env:PS3_GCM_BEGIN_OFFSET = "1000" }
    # Its render thread walks 32 KB command segments with its own callback.
    if (-not $env:PS3_GCM_NO_TITLE_RECYCLE) { $env:PS3_GCM_NO_TITLE_RECYCLE = "1" }
    # LFQueue push/init (host inflate for the decompressor) stay on the HLE side.
    if (-not $env:YDKJ_FORCE_HLE) { $env:YDKJ_FORCE_HLE = "8A85674D,011EE38B" }
    # Movies (.pam): played on the host with ffmpeg. Without it cellSail ends
    # every movie at once and the game skips its cinematics.
    if (-not $env:SAIL_HOST_PLAY) { $env:SAIL_HOST_PLAY = "1" }
    if (-not $env:SAIL_FFMPEG) {
        $ff = Join-Path $Root "third_party\ffmpeg\bin\ffmpeg.exe"
        if (Test-Path $ff) { $env:SAIL_FFMPEG = $ff }
        elseif (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
            Write-Warning "ffmpeg not found (PATH or third_party\ffmpeg): the cinematics will be skipped"
        }
    }
    # The SAIL events the movie task waits for; quiet logging.
    if (-not $env:SAIL_OPEN_DONE_EVENT) { $env:SAIL_OPEN_DONE_EVENT = "9" }
    if (-not $env:SAIL_CLOSE_EVENT)     { $env:SAIL_CLOSE_EVENT = "5" }
    if (-not $env:PS3_VERBOSE)          { $env:PS3_VERBOSE = "0" }
    # MultiStream loads the ATRAC codec from /dev_flash/sys/external/flashATRAC.pic.
    if (-not $env:PS3_DEV_FLASH) {
        if ($Rpcs3 -and (Test-Path (Join-Path $Rpcs3 "dev_flash"))) { $env:PS3_DEV_FLASH = Join-Path $Rpcs3 "dev_flash" }
        else { Write-Warning "no dev_flash (pass -Rpcs3 once): no music or voices" }
    }
    # Eye adaptation needs the small luminance targets copied back to guest VRAM.
    if (-not $env:LD_SURF_WRITEBACK) { $env:LD_SURF_WRITEBACK = "1" }
    # 60 fps (the RPCS3 "Unlock FPS" patch, real frame time) unless DES_FPS_UNLOCK=0.
    if (-not $env:DES_FPS_UNLOCK) { $env:DES_FPS_UNLOCK = "1" }
    # DES_FPS (live menu) owns the frame cap. Do not install a second fixed
    # cap in GCM/the renderer: it would keep 120 fps mode capped at 60.
    if (-not $env:LD_VSYNC)         { $env:LD_VSYNC = "0" }
    if (-not $env:LD_ASYNC_PRESENT) { $env:LD_ASYNC_PRESENT = "1" }
}
