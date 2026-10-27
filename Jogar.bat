@echo off
rem Opens the launcher (build\DesLauncher.exe: options, then the game through
rem play.ps1). Before it is built, starts the game directly.
if exist "%~dp0build\DesLauncher.exe" (
    start "" "%~dp0build\DesLauncher.exe"
) else (
    start "" powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0scripts\windows\play.ps1"
)
