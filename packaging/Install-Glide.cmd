@echo off
rem Glide installer: runs Install-Glide.ps1 next to this file. Extra arguments are passed on
rem (for example: Install-Glide.cmd -Uninstall).
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Glide.ps1" %*
set "code=%ERRORLEVEL%"
rem One click: the window closes by itself when everything worked (Glide opens); it stays open on an error.
if not "%code%"=="0" (
  echo.
  pause
)
exit /b %code%
