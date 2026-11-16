@echo off
rem Double-clickable wrapper around build_windows.ps1.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_windows.ps1" %*
if errorlevel 1 pause
