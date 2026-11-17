<#
  Glide for OBS Studio: installer.

  Copies pushfps-obs.dll into OBS's per-machine plugin folder. OBS Studio 33 and newer load
  %ProgramData%\obs-studio\plugins\pushfps-obs\pushfps-obs.dll (the new layout; OBS 34 loads only
  this one); OBS Studio 28 to 32 load ...\pushfps-obs\bin\64bit\pushfps-obs.dll. Both are written, so
  the plugin keeps working when OBS is updated, and OBS 33+ uses the new one and skips the other.
  No administrator rights are needed and the OBS installation itself is not touched.

  Usage (normally just double-click Install-Glide-OBS.cmd):
    Install-Glide-OBS.ps1             install or update
    Install-Glide-OBS.ps1 -Uninstall  remove the plugin

  Works with Windows PowerShell 5.1 and PowerShell 7.
#>
[CmdletBinding()]
param([switch]$Uninstall)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$pluginRoot = Join-Path $env:ProgramData 'obs-studio\plugins\pushfps-obs'
$binDir = Join-Path $pluginRoot 'bin\64bit'   # OBS Studio 28-32 (legacy layout in OBS 33)
$dataDir = Join-Path $pluginRoot 'data'

function Test-ObsRunning { [bool](Get-Process -Name 'obs64' -ErrorAction SilentlyContinue) }

if (Test-ObsRunning) {
    Write-Host 'OBS Studio is running. Close OBS Studio first, then run this again.' -ForegroundColor Yellow
    exit 2
}

if ($Uninstall) {
    if (Test-Path $pluginRoot) {
        Remove-Item -Recurse -Force $pluginRoot
        Write-Host 'Glide for OBS was removed.' -ForegroundColor Green
    } else {
        Write-Host 'Glide for OBS is not installed.'
    }
    exit 0
}

$dll = Join-Path $here 'pushfps-obs\bin\64bit\pushfps-obs.dll'
if (-not (Test-Path $dll)) { throw "pushfps-obs.dll not found next to this script ($dll). Unzip the whole download first." }

$obs = $null
foreach ($key in 'HKLM:\SOFTWARE\OBS Studio', 'HKLM:\SOFTWARE\WOW6432Node\OBS Studio') {
    $item = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
    if ($item -and $item.'(default)') { $obs = $item.'(default)'; break }
}
if (-not $obs -and (Test-Path "$env:ProgramFiles\obs-studio\bin\64bit\obs64.exe")) { $obs = "$env:ProgramFiles\obs-studio" }
if ($obs) { Write-Host "OBS Studio found: $obs" } else {
    Write-Host 'OBS Studio was not found. The plugin is installed anyway and loads once OBS Studio 28 or newer is installed.' -ForegroundColor Yellow
}
# An old copy in OBS's program folder (manual installs) would be loaded as a Legacy plugin by OBS 33.
if ($obs) {
    $old = Join-Path $obs 'obs-plugins\64bit\pushfps-obs.dll'
    if (Test-Path $old) { Write-Host "Note: an older manual copy exists at $old. You can delete it; this installer does not touch the OBS folder." -ForegroundColor Yellow }
}

New-Item -ItemType Directory -Force $binDir, $dataDir | Out-Null
Copy-Item -Force $dll $pluginRoot   # OBS Studio 33+ (required from OBS 34)
Copy-Item -Force $dll $binDir
$readme = Join-Path $here 'READ ME FIRST.txt'
if (Test-Path $readme) { Copy-Item -Force $readme (Join-Path $dataDir 'README.txt') }

Write-Host ''
Write-Host 'Glide for OBS is installed.' -ForegroundColor Green
Write-Host 'Start OBS Studio, then in a scene: + (Add source) > Glide.'
Write-Host 'Tools > Open Glide starts the app with OBS game capture support.'
exit 0
