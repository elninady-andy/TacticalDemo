param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$log = Join-Path $root 'Saved\Logs\CreateAssets.log'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') (Join-Path $root 'TacticalDemo.uproject') '/Engine/Maps/Entry' '-run=pythonscript' "-script=$PSScriptRoot\CreateDemoAssets.py" '-unattended' '-nullrhi' '-nosound' '-nop4' "-abslog=$log"
$exit = $LASTEXITCODE
if (Test-Path $log) { Select-String -Path $log -Pattern 'TACTICAL_ASSETS_READY|Error:|Traceback' }
if ($exit -ne 0) { throw "Asset generation failed: $exit" }
foreach ($asset in @('Content/Maps/Demo.umap','Content/Materials/M_Unit.uasset')) {
    if (-not (Test-Path (Join-Path $root $asset))) { throw "Asset not generated: $asset" }
}
