param([string]$EngineRoot='', [string]$Filter = 'TacticalDemo')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$report = Join-Path $root 'Saved\Automation'
$log = Join-Path $root 'Saved\Logs\Automation.log'
$index = Join-Path $report 'index.json'
if (Test-Path $index) { Remove-Item -LiteralPath $index }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') (Join-Path $root 'TacticalDemo.uproject') '-unattended' '-nullrhi' '-nosound' '-nop4' "-ExecCmds=Automation RunTests $Filter" '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$log"
$exit = $LASTEXITCODE
if (Test-Path $log) { Select-String -Path $log -Pattern 'Test Completed|Result=|Error:|TEST COMPLETE' }
if ($exit -ne 0) { throw "UE tests failed: $exit. See $log" }
$summary = Get-Content -Raw $index | ConvertFrom-Json
$passed = $summary.succeeded + $summary.succeededWithWarnings
if ($summary.failed -gt 0 -or $passed -lt 1 -or $summary.notRun -gt 0) { throw "Incomplete or failing test suite; see $report" }
Write-Output "Tests passed: $passed (including $($summary.succeededWithWarnings) with expected rejection/fallback warnings)"
