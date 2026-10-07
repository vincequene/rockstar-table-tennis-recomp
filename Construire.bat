@echo off
rem Construit Rockstar Table Tennis pour PC a partir de ton ISO.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" %*
echo.
pause
