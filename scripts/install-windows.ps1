# Installs the morf VST3 and standalone app you just built on this PC.
#
#   powershell -ExecutionPolicy Bypass -File scripts\install-windows.ps1
#
# Run it from a PowerShell window opened with "Run as administrator", because the shared VST3
# folder (C:\Program Files\Common Files\VST3) belongs to the administrator.
param([string]$BuildDir = "build")

$ErrorActionPreference = "Stop"
$artefacts = Join-Path $BuildDir "plugin\morf_plugin_artefacts\Release"

if (-not (Test-Path $artefacts)) {
    Write-Host "No build found in $artefacts."
    Write-Host "Build first:  cmake --preset default; cmake --build --preset default"
    exit 1
}

$vst3Source = Join-Path $artefacts "VST3\morf.vst3"
$vst3Folder = Join-Path $env:CommonProgramFiles "VST3"
New-Item -ItemType Directory -Force -Path $vst3Folder | Out-Null
Remove-Item -Recurse -Force (Join-Path $vst3Folder "morf.vst3") -ErrorAction SilentlyContinue
Copy-Item -Recurse -Force $vst3Source $vst3Folder
Write-Host "installed $vst3Folder\morf.vst3"

$appSource = Join-Path $artefacts "Standalone\morf.exe"
$appFolder = Join-Path $env:ProgramFiles "morf"
New-Item -ItemType Directory -Force -Path $appFolder | Out-Null
Copy-Item -Force $appSource $appFolder
Write-Host "installed $appFolder\morf.exe"

Write-Host ""
Write-Host "Done. In your DAW, rescan plug-ins (in Ableton Live: Preferences > Plug-ins, hold Alt and click Rescan)."
