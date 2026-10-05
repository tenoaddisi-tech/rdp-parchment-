param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "..\site\downloads")
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$extensionDirectory = Join-Path $projectRoot "extension"
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$archivePath = Join-Path $outputPath "rdrand-lab-extension.zip"

if (Test-Path $outputPath) {
    New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
} else {
    New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
}

if (Test-Path $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}

Compress-Archive -Path (Join-Path $extensionDirectory "*") -DestinationPath $archivePath
Write-Output "Created $archivePath"
