$env:DES_ONE_LOG = "1"
$env:DES_UI = "1"
$env:DES_SKIP_INTRO = "1"

$log = "E:\demons-souls-port-pc\work\logs\verify_ui.log"
if (Test-Path $log) { Remove-Item -Force $log }

$proc = Start-Process -FilePath "E:\demons-souls-port-pc\build\MyGameRecomp.exe" `
    -ArgumentList "E:\demons-souls-port-pc\work\disc\PS3_GAME\USRDIR\EBOOT.elf" `
    -WorkingDirectory "E:\demons-souls-port-pc\work" `
    -RedirectStandardError $log `
    -PassThru

Start-Sleep -Seconds 5
if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
}

Write-Host "=== Output matching [ui] and [des] ==="
Get-Content $log | Select-String -Pattern "\[ui\]|\[des\]"
