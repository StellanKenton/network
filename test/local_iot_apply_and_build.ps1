param(
    [string]$Ip,

    [int]$HttpPort = 8800,

    [int]$MqttPort = 1883,

    [ValidateSet("Build", "Rebuild")]
    [string]$Mode = "Rebuild"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$pythonPath = Join-Path $workspaceRoot ".venv\Scripts\python.exe"
$switchScript = Join-Path $workspaceRoot "test\switch_iot_to_local.py"
$buildScript = Join-Path $workspaceRoot "scripts\vscode\keil-build.ps1"

if (-not (Test-Path $pythonPath)) {
    $pythonPath = "python"
}

Write-Host "Applying local IoT override..."
$args = @($switchScript, "apply", "--http-port", $HttpPort, "--mqtt-port", $MqttPort)
if ($Ip) {
    $args += @("--ip", $Ip)
}

& $pythonPath @args
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Running Keil $Mode..."
& powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript -Mode $Mode
exit $LASTEXITCODE