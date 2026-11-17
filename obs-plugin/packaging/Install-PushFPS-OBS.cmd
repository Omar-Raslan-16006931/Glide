@echo off
rem Glide for OBS installer: runs Install-Glide-OBS.ps1 next to this file.
rem Install-Glide-OBS.cmd -Uninstall removes the plugin.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Glide-OBS.ps1" %*
set "code=%ERRORLEVEL%"
echo.
pause
exit /b %code%
