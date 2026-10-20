@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\StartQwen.ps1"
if errorlevel 1 (
    if /I "%~1"=="-Verify" exit /b 1
    pause
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\RunDemo.ps1" %*
exit /b %errorlevel%
