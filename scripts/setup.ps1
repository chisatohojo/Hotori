$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not (Get-Command arduino-cli -ErrorAction SilentlyContinue)) {
    throw 'Install Arduino CLI and add arduino-cli to PATH, then run this script again.'
}

# Versions verified with this project's Arduino Mega 2560 sketches.
& arduino-cli core update-index
if ($LASTEXITCODE -ne 0) { throw 'Failed to update the Arduino core index.' }
& arduino-cli core install 'arduino:avr@1.8.6'
if ($LASTEXITCODE -ne 0) { throw 'Failed to install Arduino AVR Boards 1.8.6.' }
& arduino-cli lib update-index
if ($LASTEXITCODE -ne 0) { throw 'Failed to update the Arduino library index.' }
& arduino-cli lib install 'DFRobotDFPlayerMini@1.0.6'
if ($LASTEXITCODE -ne 0) { throw 'Failed to install DFRobotDFPlayerMini 1.0.6.' }

Write-Host 'Hotori dependencies are ready. Run scripts/build.ps1 to compile.'
