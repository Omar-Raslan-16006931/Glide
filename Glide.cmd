@echo off
rem Glide: double-click to run.
rem   First time: downloads the newest Glide build from GitHub, installs it into the "app" folder next to
rem   this file (no admin rights, no shortcuts), then starts it. After that it just starts Glide.
rem   Glide.cmd update   = download the newest build again and reinstall.
setlocal
set "HERE=%~dp0"
set "APP=%HERE%app"
set "REPO=Omar-Raslan-16006931/Glide"

if /i "%~1"=="update" goto install
if exist "%APP%\Glide.exe" goto run

:install
echo.
echo   Glide is getting ready. The first time it downloads about 800 MB, so give it a few minutes.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue';" ^
  "try { $r = Invoke-RestMethod -UseBasicParsing -Headers @{ 'User-Agent'='Glide' } 'https://api.github.com/repos/%REPO%/releases/latest' } catch { Write-Host '  No Glide build is published yet. Check https://github.com/%REPO%/actions and try again when the build is done.' -ForegroundColor Yellow; exit 2 };" ^
  "$a = $r.assets | Where-Object { $_.name -match '^Glide-.*-win64\.zip$' } | Select-Object -First 1;" ^
  "if (-not $a) { Write-Host '  The newest release has no Glide zip in it.' -ForegroundColor Yellow; exit 2 };" ^
  "Write-Host ('  Downloading ' + $a.name);" ^
  "$zip = Join-Path $env:TEMP $a.name; $curl = Join-Path $env:SystemRoot 'System32\curl.exe';" ^
  "if (Test-Path $curl) { & $curl -L --fail --retry 3 -o $zip $a.browser_download_url; if ($LASTEXITCODE) { exit 3 } } else { Invoke-WebRequest -UseBasicParsing $a.browser_download_url -OutFile $zip };" ^
  "$setup = Join-Path '%HERE%' '_setup'; if (Test-Path $setup) { Remove-Item $setup -Recurse -Force }; Expand-Archive $zip $setup -Force;" ^
  "$installer = Get-ChildItem $setup -Recurse -Filter 'Install-Glide.ps1' | Select-Object -First 1;" ^
  "if (-not $installer) { Write-Host '  Install-Glide.ps1 is missing from the zip.' -ForegroundColor Red; exit 4 };" ^
  "& $installer.FullName -InstallDir '%APP%' -NoShortcuts -NoLaunch; exit $LASTEXITCODE"
if errorlevel 1 (
  echo.
  echo   Glide could not be installed. See the message above.
  pause
  exit /b 1
)

:run
start "" /d "%APP%" "%APP%\Glide.exe"
exit /b 0
