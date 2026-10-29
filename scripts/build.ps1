param(
    [ValidateSet('hotori_hardware_test', 'dfplayer_min_test', 'mega_step_test', 'all')]
    [string]$Sketch = 'hotori_hardware_test'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    throw 'arduino-cli was not found in PATH. See README.md for setup.'
}

$projectRoot = Split-Path -Parent $PSScriptRoot
$sketches = @($Sketch)
if ($Sketch -eq 'all') {
    $sketches = @('hotori_hardware_test', 'dfplayer_min_test', 'mega_step_test')
}

foreach ($sketchName in $sketches) {
    $sketchPath = Join-Path $projectRoot $sketchName
    $buildPath = Join-Path $projectRoot "build/$sketchName"
    Write-Host "Building $sketchName for Arduino Mega 2560..."
    & arduino-cli compile --fqbn 'arduino:avr:mega:cpu=atmega2560' --warnings all --build-path $buildPath $sketchPath
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $sketchName (exit code $LASTEXITCODE)."
    }
}
