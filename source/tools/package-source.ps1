param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "..\site\downloads")
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$stagePath = Join-Path $projectRoot "build-source-package"
$archivePath = Join-Path $outputPath "rdrand-lab-source.zip"

if (Test-Path -LiteralPath $stagePath) {
    Remove-Item -LiteralPath $stagePath -Recurse -Force
}
New-Item -ItemType Directory -Path $stagePath -Force | Out-Null
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

foreach ($name in @("app", "examples", "include", "src", "tests", "tools", "web")) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination $stagePath -Recurse
}
foreach ($name in @("CMakeLists.txt", "README.md")) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination $stagePath
}

if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -Path (Join-Path $stagePath "*") -DestinationPath $archivePath
Remove-Item -LiteralPath $stagePath -Recurse -Force
Write-Output "Created $archivePath"
