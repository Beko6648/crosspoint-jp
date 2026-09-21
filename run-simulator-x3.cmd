@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\run_simulator_wsl.ps1" -Device X3
if errorlevel 1 pause
