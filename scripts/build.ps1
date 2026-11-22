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
    # hotori_hardware_test runs on the Arduino Nano. The other sketches are
    # older tests kept for history.
    $fqbn = 'arduino:avr:mega:cpu=atmega2560'
    if ($sketchName -eq 'hotori_hardware_test') {
        $fqbn = 'arduino:avr:nano:cpu=atmega328'
    }
    Write-Host "Building $sketchName ($fqbn)..."
    & arduino-cli compile --fqbn $fqbn --warnings all --build-path $buildPath $sketchPath
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $sketchName (exit code $LASTEXITCODE)."
    }
}
