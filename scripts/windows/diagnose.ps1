# Isolated gameplay/audio check. Copies saves; never restores over the player's
# savedata. Each tag owns its own process, logs and output directory.
param([string]$Tag = "diagnose", [int]$Seconds = 120, [int]$Fps = 60,
      [ValidateSet(0,1)][int]$AudioFix = 1, [ValidateSet(0,1)][int]$SpuReads = 1,
      [ValidateSet(0,1)][int]$Ui = 0, [int]$ShortSleepPauses = 16,
      [switch]$ForceAudioZero, [string]$Save = "", [string]$ExePath = "", [switch]$Movies,
      [switch]$Visible)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\env.ps1"
Set-DesEnv ""
if ($ExePath) { $Exe = (Resolve-Path -LiteralPath $ExePath).Path }
if ($Tag -notmatch '^[A-Za-z0-9_-]+$') { throw 'Tag must contain only letters, digits, _ or -' }
if ($Seconds -lt 5 -or $Seconds -gt 1800) { throw 'Seconds must be 5..1800' }
$run = Join-Path $Work "diagnostics\$Tag"
if (Test-Path $run) { throw "Use a fresh tag: $run already exists" }
$savedata = Join-Path $Work 'gamedata\dev_hdd0\home\00000001\savedata'
if ($Save) { $savedata = (Resolve-Path -LiteralPath $Save).Path }
New-Item -ItemType Directory -Force "$run\gamedata\dev_hdd0\home\00000001" | Out-Null
if (Test-Path $savedata) { Copy-Item -LiteralPath $savedata -Destination "$run\gamedata\dev_hdd0\home\00000001\savedata" -Recurse }
$vars = @{
    DES_UI="$Ui"; DES_SKIP_UI='1'; DES_SKIP_INTRO=$(if ($Movies) { '0' } else { '1' }); DES_AUTO_CONTINUE='1'; DES_FPS="$Fps";
    DES_VSYNC='0'; DES_DATA_DIR="$run\ui"; DES_AUDIO_NOTIFY_FIX="$AudioFix"; SAIL_HOST_PLAY=$(if ($Movies) { '1' } else { '0' });
    PS3_HDD0_ROOT="$run\gamedata\dev_hdd0"; PS3_NO_USER_INPUT='1'; PAD_NO_KEYBOARD='1'; PAD_NO_HID='1';
    PAD_NO_HOST='1';
    AUDIO_RATE='1'; AUDIO_PEAK='1'; AUDIO_NOTIFY_LOG='1'; AUDIO_SEQ_LOG='1'; AUDIO_MUTE='0';
    PS3_THREAD_CPU='10'; PS3_FC_PROF='1'; SPU_RDCH_FAST="$SpuReads"; PS3_SHORT_SLEEP_PAUSES="$ShortSleepPauses"
    DES_AUDIO_NOTIFY_TEST_ZERO=$(if ($ForceAudioZero) { '1' } else { '0' })
}
$old = @{}
$p = $null
try {
    foreach ($k in $vars.Keys) { $old[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k,$vars[$k]) }
    $windowStyle = if ($Visible) { 'Normal' } else { 'Hidden' }
    $p = Start-Process -FilePath $Exe -ArgumentList "`"$Eboot`"" -WorkingDirectory $run `
        -RedirectStandardOutput "$run\run.out" -RedirectStandardError "$run\run.err" -PassThru -WindowStyle $windowStyle
    $null = $p.Handle # Keep the process handle/exit code available in Windows PowerShell.
    "diagnostic pid=$($p.Id), output=$run"
    $lastProgressMs = $null
    $lastFrameLength = $null
    $watch = [Diagnostics.Stopwatch]::StartNew()
    if ($env:DES_FRAMELOG) {
        # File timestamps may change when Windows closes the process's handle.
        # Observe appends while it is alive, before terminating at the deadline.
        do {
            $remaining = $Seconds * 1000 - $watch.ElapsedMilliseconds
            $exited = $p.WaitForExit([int][Math]::Max(0, [Math]::Min(1000, $remaining)))
            if ([IO.File]::Exists($env:DES_FRAMELOG)) {
                $length = ([IO.FileInfo]::new($env:DES_FRAMELOG)).Length
                if ($length -gt 0 -and $length -ne $lastFrameLength) {
                    $lastProgressMs = $watch.ElapsedMilliseconds
                }
                $lastFrameLength = $length
            }
        } while (-not $exited -and $watch.ElapsedMilliseconds -lt $Seconds * 1000)
        $stopped = -not $exited
    } else {
        $stopped = -not $p.WaitForExit($Seconds * 1000)
    }
    $progressAge = if ($null -ne $lastProgressMs) { ($watch.ElapsedMilliseconds - $lastProgressMs) / 1000.0 } else { $null }
    $watch.Stop()
    if ($stopped) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
    $endedAt = Get-Date
    # A log can contain healthy FPS from before a hang. When the caller asks
    # for the title's frame log, also check that updates reached the deadline.
    $frameProgress = $null
    if ($env:DES_FRAMELOG) {
        $frameFile = Get-Item -LiteralPath $env:DES_FRAMELOG -ErrorAction SilentlyContinue
        $lastFrame = if ($frameFile) {
            Get-Content -LiteralPath $frameFile.FullName -Tail 16 |
                Where-Object { $_ -match '^\d+(?:\.\d+)?\s+\d' } | Select-Object -Last 1
        }
        $age = if ($frameFile) { [Math]::Max(0, ($endedAt - $frameFile.LastWriteTime).TotalSeconds) } else { $null }
        $frameProgress = [ordered]@{
            path=$env:DES_FRAMELOG; lastFrame=$(if ($lastFrame) { [string]$lastFrame } else { $null }); secondsSinceWrite=$age;
            secondsSinceProgress=$progressAge;
            stalledAtDeadline=($stopped -and $null -ne $progressAge -and $progressAge -gt 10);
            missing=($null -eq $frameFile -or -not $lastFrame)
        }
    }
    $lines = Get-Content -LiteralPath "$run\run.err"
    $fpsSamples = @($lines | Select-String '^\[fps\] ([0-9.]+)' | ForEach-Object { [double]$_.Matches[0].Groups[1].Value })
    $tail = @($fpsSamples | Select-Object -Last 12)
    $stats = $tail | Measure-Object -Average -Minimum -Maximum
    $audio = @($lines | Select-String '^\[audio-rate\]' | Select-Object -Last 6 | ForEach-Object { $_.Line })
    [ordered]@{tag=$Tag;duration=$Seconds;elapsedSeconds=$watch.Elapsed.TotalSeconds;visible=[bool]$Visible;stoppedAtDeadline=$stopped;exitCode=$p.ExitCode;
        fpsSamples=$fpsSamples.Count;fpsMean=$stats.Average;fpsMin=$stats.Minimum;fpsMax=$stats.Maximum;
        frameProgress=$frameProgress;
        audio=$audio;notifyFix=@($lines|Select-String '^\[audio-notify-fix\]'|ForEach-Object {$_.Line});
        cpu=@($lines|Select-String '^\[thread-cpu\]'|Select-Object -Last 6|ForEach-Object {$_.Line});
        flush=@($lines|Select-String '^\[flush-prof\]'|Select-Object -Last 6|ForEach-Object {$_.Line});
        framegen=@($lines|Select-String '^\[framegen\]'|Select-Object -Last 6|ForEach-Object {$_.Line});
        output=@($lines|Select-String '^\[output\]'|Select-Object -First 6|ForEach-Object {$_.Line});
        gpuPost=@($lines|Select-String '^\[gpu-post\]'|Select-Object -Last 6|ForEach-Object {$_.Line});
        presentProfile=@($lines|Select-String '^\[present-prof\]'|Select-Object -Last 6|ForEach-Object {$_.Line});
        presentStats=@($lines|Select-String '^\[present-stats\]'|Select-Object -Last 6|ForEach-Object {$_.Line});
        presentWorker=@($lines|Select-String '^\[present-worker\]'|Select-Object -Last 6|ForEach-Object {$_.Line});
        fifoIdle=@($lines|Select-String '^\[fifo-idle\]'|Select-Object -Last 6|ForEach-Object {$_.Line});
        exeSha256=(Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$run\summary.json" -Encoding UTF8
    Get-Content -LiteralPath "$run\summary.json"
    if (-not $stopped -and $null -eq $p.ExitCode) { throw 'Process exit status unavailable: this run did not validate gameplay' }
    if (-not $stopped -and $p.ExitCode -ne 0) { throw "diagnostic crashed: $($p.ExitCode)" }
    if ($frameProgress -and $frameProgress.stalledAtDeadline) { throw 'Frame updates stopped more than 10 seconds before the deadline; earlier FPS samples do not validate this run' }
    if ($frameProgress -and $frameProgress.missing) { throw 'No title frame updates: this run did not validate gameplay progress' }
    if (-not $fpsSamples.Count) { throw 'No FPS samples: this run did not validate rendering' }
} finally {
    if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    foreach ($k in $old.Keys) {
        if ($null -eq $old[$k]) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }
        else { [Environment]::SetEnvironmentVariable($k,$old[$k]) }
    }
}
