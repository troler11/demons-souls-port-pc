# Launcher: a window to pick the options and start the game (play.ps1).
# Settings are kept in work\launcher.json. Start it with Jogar.bat.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
$root = Split-Path (Split-Path $PSScriptRoot)
$cfgPath = Join-Path $root "work\launcher.json"

$cfg = [ordered]@{ Resolution = "1080p"; Scale = "Auto"; Fps = "60"; Fullscreen = $false; Fsr = $false;
                   Fxaa = $false; Sharpen = 0; FrameGen = $false; Movies = $true; Keyboard = $true }
if (Test-Path $cfgPath) {
    try { (Get-Content $cfgPath -Raw | ConvertFrom-Json).PSObject.Properties | ForEach-Object { $cfg[$_.Name] = $_.Value } } catch {}
}

$f = New-Object System.Windows.Forms.Form
$f.Text = "Demon's Souls Port PC"
$f.FormBorderStyle = "FixedDialog"; $f.MaximizeBox = $false; $f.StartPosition = "CenterScreen"
$f.ClientSize = New-Object System.Drawing.Size(420, 470)
$f.Font = New-Object System.Drawing.Font("Segoe UI", 10)
$y = 16
function Add-Label($text, $yy) {
    $l = New-Object System.Windows.Forms.Label
    $l.Text = $text; $l.Location = New-Object System.Drawing.Point(20, ($yy + 3)); $l.AutoSize = $true
    $f.Controls.Add($l)
}
function Add-Combo($text, $items, $value) {
    Add-Label $text $script:y
    $c = New-Object System.Windows.Forms.ComboBox
    $c.DropDownStyle = "DropDownList"; $c.Location = New-Object System.Drawing.Point(210, $script:y); $c.Width = 190
    $items | ForEach-Object { [void]$c.Items.Add($_) }
    $c.SelectedItem = $value; if ($c.SelectedIndex -lt 0) { $c.SelectedIndex = 0 }
    $f.Controls.Add($c); $script:y += 36; return $c
}
function Add-Check($text, $value, $tip) {
    $c = New-Object System.Windows.Forms.CheckBox
    $c.Text = $text; $c.Checked = [bool]$value; $c.AutoSize = $true
    $c.Location = New-Object System.Drawing.Point(20, $script:y)
    if ($tip) { $tt.SetToolTip($c, $tip) }
    $f.Controls.Add($c); $script:y += 30; return $c
}
$tt = New-Object System.Windows.Forms.ToolTip

$cRes   = Add-Combo "Resolução da janela" @("720p", "1080p", "1440p", "4k") $cfg.Resolution
$cScale = Add-Combo "Resolução interna" @("Auto", "1x (720p)", "2x (1440p)", "3x (2160p)", "4x (2880p)") $cfg.Scale
$cFps   = Add-Combo "Taxa de quadros" @("60", "30 (original)", "Sem limite") $cfg.Fps
$y += 4
$kFull  = Add-Check "Tela cheia (F11 alterna)" $cfg.Fullscreen ""
$kFsr   = Add-Check "AMD FSR 1 (renderiza menor e amplia)" $cfg.Fsr "Bom para placas de vídeo mais fracas."
$kFxaa  = Add-Check "Anti-aliasing FXAA" $cfg.Fxaa ""
$kGen   = Add-Check "Frame generation (dobra os quadros na tela)" $cfg.FrameGen "Precisa de monitor acima de 60 Hz. Meio quadro a mais de atraso."
$kMov   = Add-Check "Cinemáticas em vídeo" $cfg.Movies "Precisa do ffmpeg."
$kKey   = Add-Check "Teclado como controle (sem controle conectado)" $cfg.Keyboard ""
Add-Label "Nitidez (RCAS)" $y
$tSh = New-Object System.Windows.Forms.TrackBar
$tSh.Minimum = 0; $tSh.Maximum = 10; $tSh.Value = [int]$cfg.Sharpen; $tSh.TickFrequency = 1
$tSh.Location = New-Object System.Drawing.Point(205, ($y - 4)); $tSh.Width = 200
$f.Controls.Add($tSh); $y += 50

$info = New-Object System.Windows.Forms.Label
$info.Text = "Controles: Xbox (XInput), DualShock 4 e DualSense (USB ou Bluetooth)."
$info.Location = New-Object System.Drawing.Point(20, $y); $info.Size = New-Object System.Drawing.Size(380, 40)
$info.ForeColor = [System.Drawing.Color]::DimGray
$f.Controls.Add($info); $y += 44

$bPlay = New-Object System.Windows.Forms.Button
$bPlay.Text = "Jogar"; $bPlay.Width = 380; $bPlay.Height = 40
$bPlay.Location = New-Object System.Drawing.Point(20, $y)
$bPlay.Font = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Bold)
$f.Controls.Add($bPlay); $f.AcceptButton = $bPlay

$bPlay.Add_Click({
    $cfg.Resolution = $cRes.SelectedItem; $cfg.Scale = $cScale.SelectedItem; $cfg.Fps = $cFps.SelectedItem
    $cfg.Fullscreen = $kFull.Checked; $cfg.Fsr = $kFsr.Checked; $cfg.Fxaa = $kFxaa.Checked
    $cfg.FrameGen = $kGen.Checked; $cfg.Movies = $kMov.Checked; $cfg.Keyboard = $kKey.Checked
    $cfg.Sharpen = $tSh.Value
    New-Item -ItemType Directory -Force (Split-Path $cfgPath) | Out-Null
    $cfg | ConvertTo-Json | Set-Content -Encoding utf8 $cfgPath

    $msg = $null
    if (-not (Test-Path (Join-Path $root "build\MyGameRecomp.exe"))) { $msg = "O jogo ainda não foi compilado (scripts\windows\build.ps1)." }
    elseif (-not (Test-Path (Join-Path $root "work\disc\PS3_GAME\USRDIR\EBOOT.elf"))) { $msg = "O código do jogo ainda não foi gerado (scripts\windows\prepare.ps1)." }
    if ($msg) { [System.Windows.Forms.MessageBox]::Show($msg, "Demon's Souls Port PC") | Out-Null; return }
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSScriptRoot\play.ps1`"", "-Resolution", $cfg.Resolution)
    # First run: where RPCS3 is (its dev_flash has the audio codec); play.ps1 remembers it.
    if (-not (Test-Path (Join-Path $root "work\settings.json"))) {
        $fb = New-Object System.Windows.Forms.FolderBrowserDialog
        $fb.Description = "Pasta do RPCS3 (com o firmware do PS3 instalado)"
        if ($fb.ShowDialog() -ne "OK") { return }
        $a += @("-Rpcs3", "`"$($fb.SelectedPath)`"")
    }
    if ($cfg.Scale -ne "Auto") { $a += @("-Scale", $cfg.Scale.Substring(0, 1)) }
    if ($cfg.Fps -like "30*") { $a += "-Fps30" } elseif ($cfg.Fps -eq "Sem limite") { $a += "-Uncapped" }
    if ($cfg.Fullscreen) { $a += "-Fullscreen" }
    if ($cfg.Fsr) { $a += "-Fsr" }
    if ($cfg.Fxaa) { $a += "-Fxaa" }
    if ($cfg.FrameGen) { $a += "-FrameGen" }
    if ($cfg.Sharpen -gt 0) { $a += @("-Sharpen", ($cfg.Sharpen / 10.0).ToString([Globalization.CultureInfo]::InvariantCulture)) }
    # play.ps1 reads these from the environment of the process it starts
    $env:SAIL_HOST_PLAY = if ($cfg.Movies) { "1" } else { "0" }
    if ($cfg.Keyboard) { Remove-Item env:PAD_NO_KEYBOARD -ErrorAction SilentlyContinue } else { $env:PAD_NO_KEYBOARD = "1" }
    try {
        Start-Process powershell -ArgumentList $a -WindowStyle Hidden
        $f.Close()
    } catch {
        [System.Windows.Forms.MessageBox]::Show("Não foi possível iniciar: $_", "Demon's Souls Port PC") | Out-Null
    }
})
[void]$f.ShowDialog()
