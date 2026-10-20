param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'StartQwen.ps1')
$report=Join-Path $root 'Saved/LayoutQwenAutomation'
$index=Join-Path $report 'index.json'
if(Test-Path -LiteralPath $index){Remove-Item -LiteralPath $index}
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') (Join-Path $root 'TacticalDemo.uproject') -unattended -nullrhi -nosound -nop4 '-ExecCmds=Automation RunTests Integration.LayoutQwen' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$root/Saved/Logs/LayoutQwen.log"
if($LASTEXITCODE -ne 0){throw 'Local Qwen layout integration process failed'}
$summary=Get-Content -Raw -LiteralPath $index | ConvertFrom-Json
if($summary.failed -ne 0 -or $summary.notRun -ne 0 -or ($summary.succeeded+$summary.succeededWithWarnings) -ne 1){throw 'Local Qwen layout integration failed; see Saved/Logs/LayoutQwen.log'}
Write-Output 'Actual local Qwen retained all individual positions/yaws after preset reload through texture, model and side changes.'
