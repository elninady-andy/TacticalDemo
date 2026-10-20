param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'StartQwen.ps1')
$report=Join-Path $root 'Saved/CommandCatalogAutomation'
$index=Join-Path $report 'index.json'
if(Test-Path $index){Remove-Item -LiteralPath $index}
& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$root/TacticalDemo.uproject" -unattended -nullrhi -nosound -nop4 '-ExecCmds=Automation RunTests Integration.CommandCatalog' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$root/Saved/Logs/CommandCatalog.log"
if($LASTEXITCODE -ne 0){throw 'UE command catalog process failed'}
$summary=Get-Content -Raw $index | ConvertFrom-Json
if($summary.failed -ne 0 -or ($summary.succeeded+$summary.succeededWithWarnings) -ne 1){throw 'Command catalog failed; inspect Saved/Logs/CommandCatalog.log'}
Write-Output 'All 12 supported command categories verified through UE and actual local Qwen.'
