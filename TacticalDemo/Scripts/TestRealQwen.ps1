param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'StartQwen.ps1')
$report=Join-Path $root 'Saved/RealQwenAutomation'
$index=Join-Path $report 'index.json'
if(Test-Path $index){Remove-Item -LiteralPath $index}
& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$root/TacticalDemo.uproject" -unattended -nullrhi -nosound -nop4 '-ExecCmds=Automation RunTests Integration.RealQwen' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$root/Saved/Logs/RealQwen.log"
if($LASTEXITCODE -ne 0){throw 'UE real Qwen test process failed'}
$summary=Get-Content -Raw $index | ConvertFrom-Json
if($summary.failed -ne 0 -or ($summary.succeeded+$summary.succeededWithWarnings) -ne 1){throw 'Real Qwen integration failed; inspect Saved/Logs/RealQwen.log'}
Write-Output 'UE -> local Qwen -> create/count/position/model replacement/static route/route clear passed.'
