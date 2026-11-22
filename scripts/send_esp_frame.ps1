# Sends one ESP32 frame (docs/ESP32_UART_PROTOCOL.md) from the PC and shows
# the board's log. On the Nano the frame goes through the USB port, so this
# tests hotori_hardware_test without an ESP32.
#
# Example:
#   .\scripts\send_esp_frame.ps1 -Port COM5 -Neck -NeckLR 45 -NeckUD -15 -NeckSpeed 60
#   .\scripts\send_esp_frame.ps1 -Port COM5 -Eye -EyeState 1
param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [switch]$State,
    [ValidateRange(1, 9)][int]$ActionId = 1,
    [ValidateRange(0, 3)][int]$CatState = 0,
    [switch]$Voice,
    [ValidateRange(0, 6)][int]$VoiceId = 0,
    [switch]$Breath,
    [ValidateRange(0, 60)][int]$BreathRate = 0,
    [switch]$Tail,
    [ValidateRange(0, 3)][int]$TailDir = 0,
    [ValidateRange(0, 100)][int]$TailSpeed = 0,
    [ValidateRange(0, 255)][int]$TailTime = 0,
    [switch]$Neck,
    [ValidateRange(-90, 90)][int]$NeckLR = 0,
    [ValidateRange(-30, 30)][int]$NeckUD = 0,
    [ValidateRange(0, 100)][int]$NeckSpeed = 0,
    [switch]$Eye,
    [ValidateRange(0, 1)][int]$EyeState = 0,
    [int]$BaudRate = 115200,
    # Opening the port may reset the board; wait for it to boot first.
    [int]$WaitMs = 2500,
    # How long to show the board's log after sending.
    [int]$ListenMs = 1500
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-Crc8([byte[]]$Data) {
    $crc = 0
    foreach ($b in $Data) {
        $crc = $crc -bxor $b
        for ($i = 0; $i -lt 8; $i++) {
            if ($crc -band 0x80) {
                $crc = (($crc -shl 1) -bxor 0x07) -band 0xFF
            } else {
                $crc = ($crc -shl 1) -band 0xFF
            }
        }
    }
    return [byte]$crc
}

$flags = 0
if ($Voice) { $flags = $flags -bor 0x01 }
if ($Breath) { $flags = $flags -bor 0x02 }
if ($Tail) { $flags = $flags -bor 0x04 }
if ($Neck) { $flags = $flags -bor 0x08 }
if ($Eye) { $flags = $flags -bor 0x10 }
if ($State) { $flags = $flags -bor 0x20 }
if ($flags -eq 0) {
    throw 'Select at least one group: -State, -Voice, -Breath, -Tail, -Neck or -Eye.'
}

# Unselected groups are sent as 0, as the protocol asks. Parameters keep
# their range checks, so the zeros go into separate variables.
function Select-Value([bool]$Selected, [int]$Value) {
    if ($Selected) { return $Value } else { return 0 }
}

[byte[]]$payload = @(
    $flags,
    (Select-Value $State $ActionId),
    (Select-Value $State $CatState),
    (Select-Value $Voice $VoiceId),
    (Select-Value $Breath $BreathRate),
    (Select-Value $Tail $TailDir),
    (Select-Value $Tail $TailSpeed),
    (Select-Value $Tail $TailTime),
    ((Select-Value $Neck $NeckLR) -band 0xFF),
    ((Select-Value $Neck $NeckUD) -band 0xFF),
    (Select-Value $Neck $NeckSpeed),
    (Select-Value $Eye $EyeState)
)
[byte[]]$frame = @(0xAA, 0x55) + $payload + @(Get-Crc8 $payload)
Write-Host ('Frame: ' + (($frame | ForEach-Object { $_.ToString('X2') }) -join ' '))

$serial = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, 'None', 8, 'One'
$serial.ReadTimeout = 50
$serial.Open()
try {
    Start-Sleep -Milliseconds $WaitMs
    $serial.DiscardInBuffer()
    $serial.Write($frame, 0, $frame.Length)
    $deadline = (Get-Date).AddMilliseconds($ListenMs)
    while ((Get-Date) -lt $deadline) {
        $text = $serial.ReadExisting()
        if ($text) { Write-Host -NoNewline $text }
        Start-Sleep -Milliseconds 20
    }
    Write-Host ''
} finally {
    $serial.Close()
}
