param(
    [switch]$Rebuild,

    [ValidateSet("Build", "Rebuild")]
    [string]$Mode = "Build"
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

Write-Host "Restoring original IoT override macros..."
& $pythonPath $switchScript restore
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($Rebuild) {
    Write-Host "Running Keil $Mode..."
    & powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript -Mode $Mode
    exit $LASTEXITCODE
}