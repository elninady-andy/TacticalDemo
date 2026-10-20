param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference = 'Stop'
$project = Join-Path (Split-Path $PSScriptRoot -Parent) 'TacticalDemo.uproject'
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') TacticalDemoEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "UE build failed: $LASTEXITCODE" }
