param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot "..\build-release"),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "..\site\downloads")
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildPath = [System.IO.Path]::GetFullPath($BuildDirectory)
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$stagePath = Join-Path $buildPath "rdrand-dashboard-windows-x64"
$archivePath = Join-Path $outputPath "rdrand-dashboard-windows-x64.zip"

cmake -S $projectRoot -B $buildPath -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }
cmake --build $buildPath --config Release
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

$executable = Join-Path $buildPath "Release\rdrand_dashboard.exe"
if (-not (Test-Path $executable)) {
    $executable = Join-Path $buildPath "rdrand_dashboard.exe"
}
if (-not (Test-Path $executable)) {
    throw "Could not find rdrand_dashboard.exe in the build output."
}

if (Test-Path $stagePath) {
    Remove-Item -LiteralPath $stagePath -Recurse -Force
}
New-Item -ItemType Directory -Path $stagePath -Force | Out-Null
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $projectRoot "web") -Destination $stagePath -Recurse

@'
@echo off
cd /d "%~dp0"
start "" rdrand_dashboard.exe
'@ | Set-Content -LiteralPath (Join-Path $stagePath "Start RDRAND Lab.cmd") -Encoding ASCII

@'
RDRAND Lab portable Windows app

Run "Start RDRAND Lab.cmd", then open http://127.0.0.1:8787.
Keep the app running while using the browser extension.
The app binds only to loopback and does not expose its API to the network.

xoshiro256** is not cryptographic. Do not use it for keys, tokens, or nonces.
'@ | Set-Content -LiteralPath (Join-Path $stagePath "README.txt") -Encoding UTF8

if (Test-Path $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -Path (Join-Path $stagePath "*") -DestinationPath $archivePath
Write-Output "Created $archivePath"
