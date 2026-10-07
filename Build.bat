@echo off
rem Builds Rockstar Table Tennis for PC from your own ISO.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" %*
echo.
pause
