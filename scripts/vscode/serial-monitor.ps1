param(
    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path,

    [string]$ConfigPath = (Join-Path $PSScriptRoot "workspace.config.psd1"),

    [string]$PortName,

    [int]$BaudRate,

    [int]$DataBits,

    [ValidateSet("None", "Odd", "Even", "Mark", "Space")]
    [string]$Parity,

    [ValidateSet("None", "One", "Two", "OnePointFive")]
    [string]$StopBits,

    [bool]$DtrEnable,

    [bool]$RtsEnable,

    [int]$ReadTimeoutMilliseconds = 50
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-WorkspaceToolsConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        throw "Workspace tools config not found: $Path"
    }

    return Import-PowerShellDataFile -Path $Path
}

function Get-SerialSetting {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Config,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        $DefaultValue
    )

    if ($Config.ContainsKey($Name)) {
        return $Config[$Name]
    }

    return $DefaultValue
}

function Get-SerialStopBits {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return [System.IO.Ports.StopBits]::$Name
}

function Get-SerialParity {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return [System.IO.Ports.Parity]::$Name
}

function Resolve-SerialPortName {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Config,

        [Parameter(Mandatory = $true)]
        [string[]]$AvailablePorts,

        [string]$ConfiguredPortName
    )

    if (-not [string]::IsNullOrWhiteSpace($ConfiguredPortName)) {
        return $ConfiguredPortName
    }

    if ($AvailablePorts.Count -eq 1) {
        return $AvailablePorts[0]
    }

    if ($AvailablePorts.Count -eq 0) {
        throw "No serial ports are available. Connect the target or specify SERIAL_PORT after the port appears."
    }

    throw "Serial port is not configured. Detected ports: $($AvailablePorts -join ', '). Set SERIAL_PORT, pass -PortName, or configure Serial.PortName in workspace.config.psd1."
}

function Write-SerialInput {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$SerialPort
    )

    while ([Console]::KeyAvailable) {
        $keyInfo = [Console]::ReadKey($true)

        if ($keyInfo.Key -eq [ConsoleKey]::Enter) {
            $SerialPort.Write("`r`n")
            continue
        }

        if ($keyInfo.Key -eq [ConsoleKey]::Backspace) {
            $SerialPort.Write([char]8)
            continue
        }

        if ($keyInfo.KeyChar -ne [char]0) {
            $SerialPort.Write(([string]$keyInfo.KeyChar))
        }
    }
}

$workspaceConfig = Get-WorkspaceToolsConfig -Path $ConfigPath

$serialConfig = @{}
if ($workspaceConfig.ContainsKey("Serial") -and $workspaceConfig.Serial) {
    $serialConfig = $workspaceConfig.Serial
}

if ((-not $PSBoundParameters.ContainsKey("PortName")) -or [string]::IsNullOrWhiteSpace($PortName)) {
    if ($env:SERIAL_PORT) {
        $PortName = $env:SERIAL_PORT
    } else {
        $PortName = [string](Get-SerialSetting -Config $serialConfig -Name "PortName" -DefaultValue "")
    }
}

if (-not $PSBoundParameters.ContainsKey("BaudRate")) {
    if ($env:SERIAL_BAUD) {
        $BaudRate = [int]$env:SERIAL_BAUD
    } else {
        $BaudRate = [int](Get-SerialSetting -Config $serialConfig -Name "BaudRate" -DefaultValue 115200)
    }
}

if (-not $PSBoundParameters.ContainsKey("DataBits")) {
    $DataBits = [int](Get-SerialSetting -Config $serialConfig -Name "DataBits" -DefaultValue 8)
}

if (-not $PSBoundParameters.ContainsKey("Parity")) {
    $Parity = [string](Get-SerialSetting -Config $serialConfig -Name "Parity" -DefaultValue "None")
}

if (-not $PSBoundParameters.ContainsKey("StopBits")) {
    $StopBits = [string](Get-SerialSetting -Config $serialConfig -Name "StopBits" -DefaultValue "One")
}

if (-not $PSBoundParameters.ContainsKey("DtrEnable")) {
    $DtrEnable = [bool](Get-SerialSetting -Config $serialConfig -Name "DtrEnable" -DefaultValue $false)
}

if (-not $PSBoundParameters.ContainsKey("RtsEnable")) {
    $RtsEnable = [bool](Get-SerialSetting -Config $serialConfig -Name "RtsEnable" -DefaultValue $false)
}

$availablePorts = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
$PortName = Resolve-SerialPortName -Config $serialConfig -AvailablePorts $availablePorts -ConfiguredPortName $PortName
if (-not ($availablePorts -contains $PortName)) {
    throw "Serial port $PortName is not available. Detected ports: $($availablePorts -join ', ')"
}

$serialPort = New-Object System.IO.Ports.SerialPort $PortName, $BaudRate, (Get-SerialParity -Name $Parity), $DataBits, (Get-SerialStopBits -Name $StopBits)
$serialPort.DtrEnable = $DtrEnable
$serialPort.RtsEnable = $RtsEnable
$serialPort.ReadTimeout = $ReadTimeoutMilliseconds
$serialPort.NewLine = "`r`n"

try {
    $serialPort.Open()

    Write-Host "Opening serial monitor"
    Write-Host "Workspace: $WorkspaceRoot"
    Write-Host "Port: $PortName"
    Write-Host "Baud rate: $BaudRate"
    Write-Host "Data bits: $DataBits"
    Write-Host "Parity: $Parity"
    Write-Host "Stop bits: $StopBits"
    Write-Host "DTR: $DtrEnable"
    Write-Host "RTS: $RtsEnable"
    Write-Host "Type in this terminal to send data. Press Ctrl+C to stop."

    while ($true) {
        Write-SerialInput -SerialPort $serialPort

        if ($serialPort.BytesToRead -gt 0) {
            [Console]::Write($serialPort.ReadExisting())
            continue
        }

        Start-Sleep -Milliseconds 20
    }
}
finally {
    if (($serialPort -ne $null) -and $serialPort.IsOpen) {
        $serialPort.Close()
    }
}