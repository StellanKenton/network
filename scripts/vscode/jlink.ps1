param(
    [ValidateSet("Tools", "Flash", "Reset", "RttServer", "RttClient")]
    [string]$Action = "Tools",

    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path,

    [string]$ConfigPath = (Join-Path $PSScriptRoot "workspace.config.psd1"),

    [string]$Device,

    [string]$Interface,

    [int]$SpeedKHz,

    [string]$SerialNumber = $env:JLINK_SERIAL,

    [int]$GdbPort,

    [int]$SwoPort,

    [int]$TelnetPort,

    [int]$RttTelnetPort
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

function Get-JLinkSetting {
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

$workspaceConfig = Get-WorkspaceToolsConfig -Path $ConfigPath

if (-not $workspaceConfig.ContainsKey("HexFilePath")) {
    throw "HexFilePath is missing in workspace.config.psd1."
}

$jlinkConfig = @{}
if ($workspaceConfig.ContainsKey("JLink") -and $workspaceConfig.JLink) {
    $jlinkConfig = $workspaceConfig.JLink
}

if (-not $PSBoundParameters.ContainsKey("Device")) {
    if ($env:JLINK_DEVICE) {
        $Device = $env:JLINK_DEVICE
    } else {
        $Device = Get-JLinkSetting -Config $jlinkConfig -Name "Device" -DefaultValue "GD32F407VG"
    }
}

if (-not $PSBoundParameters.ContainsKey("Interface")) {
    if ($env:JLINK_INTERFACE) {
        $Interface = $env:JLINK_INTERFACE
    } else {
        $Interface = Get-JLinkSetting -Config $jlinkConfig -Name "Interface" -DefaultValue "SWD"
    }
}

if (-not $PSBoundParameters.ContainsKey("SpeedKHz")) {
    if ($env:JLINK_SPEED_KHZ) {
        $SpeedKHz = [int]$env:JLINK_SPEED_KHZ
    } else {
        $SpeedKHz = [int](Get-JLinkSetting -Config $jlinkConfig -Name "SpeedKHz" -DefaultValue 10000)
    }
}

if (-not $PSBoundParameters.ContainsKey("GdbPort")) {
    $GdbPort = [int](Get-JLinkSetting -Config $jlinkConfig -Name "GdbPort" -DefaultValue 2331)
}

if (-not $PSBoundParameters.ContainsKey("SwoPort")) {
    $SwoPort = [int](Get-JLinkSetting -Config $jlinkConfig -Name "SwoPort" -DefaultValue 2332)
}

if (-not $PSBoundParameters.ContainsKey("TelnetPort")) {
    $TelnetPort = [int](Get-JLinkSetting -Config $jlinkConfig -Name "TelnetPort" -DefaultValue 2333)
}

if (-not $PSBoundParameters.ContainsKey("RttTelnetPort")) {
    $RttTelnetPort = [int](Get-JLinkSetting -Config $jlinkConfig -Name "RttTelnetPort" -DefaultValue 19021)
}

$hexFilePath = Join-Path $WorkspaceRoot $workspaceConfig.HexFilePath

function Get-JLinkSearchRoots {
    $searchRoots = @()

    if ($env:JLINK_PATH) {
        $searchRoots += $env:JLINK_PATH
    }

    if ($env:LOCALAPPDATA) {
        $searchRoots += (Join-Path $env:LOCALAPPDATA "Keil_v5\ARM\Segger")
    }

    $searchRoots += "C:\Program Files\SEGGER"
    $searchRoots += "C:\Program Files (x86)\SEGGER"

    return $searchRoots |
        Where-Object { $_ -and (Test-Path $_) } |
        ForEach-Object { (Resolve-Path $_).Path } |
        Select-Object -Unique
}

function Get-JLinkExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $searchRoots = Get-JLinkSearchRoots
    $installDirectories = @()

    foreach ($searchRoot in $searchRoots) {
        $leafName = Split-Path -Leaf $searchRoot

        if ($leafName -like "JLink*") {
            $installDirectories += $searchRoot
            continue
        }

        $installDirectories += Get-ChildItem -Path $searchRoot -Directory -Filter "JLink*" -ErrorAction SilentlyContinue |
            Sort-Object -Property Name -Descending |
            ForEach-Object { $_.FullName }
    }

    foreach ($installDirectory in ($installDirectories | Select-Object -Unique)) {
        $candidatePath = Join-Path $installDirectory $Name
        if (Test-Path $candidatePath) {
            return (Resolve-Path $candidatePath).Path
        }
    }

    throw "Unable to find $Name. Install SEGGER J-Link or set JLINK_PATH to the installation directory."
}

function Test-TcpPort {
    param(
        [Parameter(Mandatory = $true)]
        [int]$Port,

        [string]$HostName = "127.0.0.1",

        [int]$TimeoutMilliseconds = 500
    )

    $tcpClient = New-Object System.Net.Sockets.TcpClient
    $connectResult = $null

    try {
        $connectResult = $tcpClient.BeginConnect($HostName, $Port, $null, $null)
        if (-not $connectResult.AsyncWaitHandle.WaitOne($TimeoutMilliseconds, $false)) {
            return $false
        }

        $tcpClient.EndConnect($connectResult)
        return $true
    }
    catch {
        return $false
    }
    finally {
        if ($connectResult) {
            $connectResult.AsyncWaitHandle.Close()
        }

        $tcpClient.Close()
    }
}

function Test-TcpListenerPortAvailable {
    param(
        [Parameter(Mandatory = $true)]
        [int]$Port,

        [string]$HostName = "0.0.0.0"
    )

    $ipAddress = [System.Net.IPAddress]::Parse($HostName)
    $tcpListener = [System.Net.Sockets.TcpListener]::new($ipAddress, $Port)

    try {
        $tcpListener.Start()
        return $true
    }
    catch {
        return $false
    }
    finally {
        if ($tcpListener) {
            $tcpListener.Stop()
        }
    }
}

function Get-FreeTcpPort {
    $tcpListener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Any, 0)

    try {
        $tcpListener.Start()
        return ([System.Net.IPEndPoint]$tcpListener.LocalEndpoint).Port
    }
    finally {
        $tcpListener.Stop()
    }
}

function Resolve-AvailableTcpPort {
    param(
        [Parameter(Mandatory = $true)]
        [int]$PreferredPort,

        [Parameter(Mandatory = $true)]
        [string]$Name,

        [int[]]$ReservedPorts = @(),

        [int]$SearchSpan = 256
    )

    if ((-not ($ReservedPorts -contains $PreferredPort)) -and (Test-TcpListenerPortAvailable -Port $PreferredPort)) {
        return $PreferredPort
    }

    for ($lCandidatePort = $PreferredPort + 1; $lCandidatePort -le ($PreferredPort + $SearchSpan); $lCandidatePort++) {
        if ($ReservedPorts -contains $lCandidatePort) {
            continue
        }

        if (Test-TcpListenerPortAvailable -Port $lCandidatePort) {
            Write-Warning "$Name port $PreferredPort is unavailable. Using $lCandidatePort instead."
            return $lCandidatePort
        }
    }

    while ($true) {
        $lFallbackPort = Get-FreeTcpPort
        if ($ReservedPorts -contains $lFallbackPort) {
            continue
        }

        Write-Warning "$Name port $PreferredPort is unavailable. Using fallback port $lFallbackPort instead."
        return $lFallbackPort
    }
}

function New-JLinkCommanderScript {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Commands
    )

    $scriptPath = Join-Path ([System.IO.Path]::GetTempPath()) ("jlink-{0}.jlink" -f [System.Guid]::NewGuid().ToString("N"))
    Set-Content -Path $scriptPath -Value ($Commands -join [Environment]::NewLine) -Encoding Ascii
    return $scriptPath
}

function Invoke-JLinkCommander {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Operation,

        [Parameter(Mandatory = $true)]
        [string[]]$Commands
    )

    $jlinkExe = Get-JLinkExecutable -Name "JLink.exe"
    $commanderScript = New-JLinkCommanderScript -Commands $Commands

    try {
        $arguments = @(
            "-ExitOnError",
            "1",
            "-device",
            $Device,
            "-if",
            $Interface,
            "-speed",
            "$SpeedKHz",
            "-autoconnect",
            "1"
        )

        if ($SerialNumber) {
            $arguments += @("-USB", $SerialNumber)
        }

        $arguments += @("-CommanderScript", $commanderScript)

        Write-Host "$Operation with J-Link"
        Write-Host "Tool: $jlinkExe"
        Write-Host "Device: $Device"
        Write-Host "Interface: $Interface"
        Write-Host "Speed: ${SpeedKHz}kHz"

        if ($SerialNumber) {
            Write-Host "Probe serial: $SerialNumber"
        }

        & $jlinkExe @arguments

        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    finally {
        Remove-Item -Path $commanderScript -Force -ErrorAction SilentlyContinue
    }
}

function Show-ResolvedTools {
    $jlinkExe = Get-JLinkExecutable -Name "JLink.exe"
    $gdbServerExe = Get-JLinkExecutable -Name "JLinkGDBServerCL.exe"
    $rttClientExe = Get-JLinkExecutable -Name "JLinkRTTClient.exe"

    Write-Host "Workspace: $WorkspaceRoot"
    Write-Host "Device: $Device"
    Write-Host "Interface: $Interface"
    Write-Host "Speed: ${SpeedKHz}kHz"
    Write-Host "JLink.exe: $jlinkExe"
    Write-Host "JLinkGDBServerCL.exe: $gdbServerExe"
    Write-Host "JLinkRTTClient.exe: $rttClientExe"
    Write-Host "Hex file: $hexFilePath"
    Write-Host "Hex file exists: $(Test-Path $hexFilePath)"

    if ($SerialNumber) {
        Write-Host "Probe serial: $SerialNumber"
    }
}

function Start-RttServer {
    if (Test-TcpPort -Port $RttTelnetPort) {
        throw "Local RTT port $RttTelnetPort is already in use. Stop the existing RTT server first."
    }

    if (-not (Test-TcpListenerPortAvailable -Port $RttTelnetPort)) {
        throw "Local RTT port $RttTelnetPort is unavailable for listening. Stop the existing process first."
    }

    $resolvedGdbPort = Resolve-AvailableTcpPort -PreferredPort $GdbPort -Name "GDB"
    $resolvedSwoPort = Resolve-AvailableTcpPort -PreferredPort $SwoPort -Name "SWO" -ReservedPorts @($resolvedGdbPort)
    $resolvedTelnetPort = Resolve-AvailableTcpPort -PreferredPort $TelnetPort -Name "Terminal" -ReservedPorts @($resolvedGdbPort, $resolvedSwoPort)

    $gdbServerExe = Get-JLinkExecutable -Name "JLinkGDBServerCL.exe"
    $arguments = @(
        "-device",
        $Device,
        "-if",
        $Interface,
        "-speed",
        "$SpeedKHz",
        "-port",
        "$resolvedGdbPort",
        "-swoport",
        "$resolvedSwoPort",
        "-telnetport",
        "$resolvedTelnetPort",
        "-RTTTelnetPort",
        "$RttTelnetPort",
        "-nohalt",
        "-nogui"
    )

    if ($SerialNumber) {
        $arguments += @("-USB", $SerialNumber)
    }

    Write-Host "Starting J-Link RTT server"
    Write-Host "Tool: $gdbServerExe"
    Write-Host "Device: $Device"
    Write-Host "Interface: $Interface"
    Write-Host "Speed: ${SpeedKHz}kHz"
    Write-Host "GDB port: $resolvedGdbPort"
    Write-Host "SWO port: $resolvedSwoPort"
    Write-Host "Terminal port: $resolvedTelnetPort"
    Write-Host "RTT port: $RttTelnetPort"
    Write-Host "Target start mode: running (-nohalt)"
    Write-Host "Press Ctrl+C to stop the server"

    $process = Start-Process -FilePath $gdbServerExe -ArgumentList $arguments -PassThru -NoNewWindow
    $readyDeadline = (Get-Date).AddSeconds(15)
    $isReady = $false

    while (-not $process.HasExited) {
        if (-not $isReady -and (Test-TcpPort -Port $RttTelnetPort)) {
            $isReady = $true
            Write-Host "RTT server ready on localhost:$RttTelnetPort"
        }

        if (-not $isReady -and ((Get-Date) -gt $readyDeadline)) {
            try {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            }
            catch {
            }

            throw "J-Link RTT server did not open localhost:$RttTelnetPort within 15 seconds."
        }

        Start-Sleep -Milliseconds 250
    }

    $process.WaitForExit()

    if (-not $isReady) {
        throw "J-Link RTT server exited before it became ready."
    }

    exit $process.ExitCode
}

function Start-RttClient {
    if ($RttTelnetPort -ne 19021) {
        throw "This integration expects the default J-Link RTT port 19021 for JLinkRTTClient.exe."
    }

    if (-not (Test-TcpPort -Port $RttTelnetPort)) {
        throw "RTT server is not ready on localhost:$RttTelnetPort. Start the J-Link: RTT Server task first."
    }

    $rttClientExe = Get-JLinkExecutable -Name "JLinkRTTClient.exe"

    Write-Host "Opening RTT terminal on localhost:$RttTelnetPort"
    Write-Host "Press Ctrl+C to stop the client"

    & $rttClientExe
    exit $LASTEXITCODE
}

try {
    switch ($Action) {
        "Tools" {
            Show-ResolvedTools
        }
        "Flash" {
            if (-not (Test-Path $hexFilePath)) {
                throw "Hex file not found: $hexFilePath. Run the Keil build first."
            }

            $resolvedHexFile = (Resolve-Path $hexFilePath).Path

            Invoke-JLinkCommander -Operation "Flashing firmware" -Commands @(
                "r",
                "h",
                ('loadfile "{0}"' -f $resolvedHexFile),
                "r",
                "g",
                "q"
            )
        }
        "Reset" {
            Invoke-JLinkCommander -Operation "Resetting target" -Commands @(
                "r",
                "g",
                "q"
            )
        }
        "RttServer" {
            Start-RttServer
        }
        "RttClient" {
            Start-RttClient
        }
    }
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}