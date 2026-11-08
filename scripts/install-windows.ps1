# Installs the morf VST3 and standalone app you just built on this PC.
#
#   powershell -ExecutionPolicy Bypass -File scripts\install-windows.ps1              # from build\
#   powershell -ExecutionPolicy Bypass -File install-windows.ps1 -BuildDir .          # from an unzipped CI download
#
# Run it from a PowerShell window opened with "Run as administrator", because the shared VST3
# folder (C:\Program Files\Common Files\VST3) belongs to the administrator.
param([string]$BuildDir = "build")

$ErrorActionPreference = "Stop"
$artefacts = Join-Path $BuildDir "plugin\morf_plugin_artefacts\Release"

if (Test-Path $artefacts) {
    $vst3Source = Join-Path $artefacts "VST3\morf.vst3"
    $appSource = Join-Path $artefacts "Standalone\morf.exe"
} elseif (Test-Path (Join-Path $BuildDir "morf.vst3")) {
    $vst3Source = Join-Path $BuildDir "morf.vst3"
    $appSource = Join-Path $BuildDir "morf.exe"
} else {
    Write-Host "No morf build found in $BuildDir."
    Write-Host "Build first:  cmake --preset default; cmake --build --preset default"
    exit 1
}

$vst3Folder = Join-Path $env:CommonProgramFiles "VST3"
New-Item -ItemType Directory -Force -Path $vst3Folder | Out-Null
Remove-Item -Recurse -Force (Join-Path $vst3Folder "morf.vst3") -ErrorAction SilentlyContinue
Copy-Item -Recurse -Force $vst3Source $vst3Folder
Write-Host "installed $vst3Folder\morf.vst3"

$appFolder = Join-Path $env:ProgramFiles "morf"
New-Item -ItemType Directory -Force -Path $appFolder | Out-Null
Copy-Item -Force $appSource $appFolder
Write-Host "installed $appFolder\morf.exe"

Write-Host ""
Write-Host "Done. In your DAW, rescan plug-ins (in Ableton Live: Preferences > Plug-ins, hold Alt and click Rescan)."
