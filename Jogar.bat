@echo off
rem Opens the launcher (scripts\windows\launcher.ps1) without a console window.
start "" powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%~dp0scripts\windows\launcher.ps1"
