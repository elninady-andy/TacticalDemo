param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'StartQwen.ps1')
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project=Join-Path $root 'TacticalDemo.uproject'
$report=Join-Path $root 'Saved/ImportedQwenAutomation'
$index=Join-Path $report 'index.json'
if(Test-Path $index){Remove-Item -LiteralPath $index}
& $editor $project -unattended -nullrhi -nosound -nop4 '-ExecCmds=Automation RunTests Integration.ImportedQwen' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$root/Saved/Logs/ImportedQwenAutomation.log"
if($LASTEXITCODE -ne 0){throw 'UE model replacement test process failed'}
$summary=Get-Content -Raw $index | ConvertFrom-Json
if($summary.failed -ne 0 -or $summary.notRun -ne 0 -or ($summary.succeeded+$summary.succeededWithWarnings) -ne 1){throw 'Imported model Qwen test failed; inspect Saved/Logs/ImportedQwenAutomation.log'}
$shot=Join-Path $root 'Saved/Evidence/AirportSceneSwitch.png'
$log=Join-Path $root 'Saved/Logs/AirportSceneSwitch.log'
if(Test-Path $shot){Remove-Item -LiteralPath $shot}
 & $editor $project '/Game/Maps/Demo?game=/Script/TacticalDemo.DemoGameMode' -game -windowed -ResX=1280 -ResY=720 -unattended -nosound -nop4 -RenderOffscreen '-trace=cpu,frame,log,bookmark' -DemoVerifySceneSwitch -DemoAutoExit -DemoScreenshotDelay=30 -DemoExitDelay=38 "-DemoScreenshot=$shot" "-abslog=$log"
if($LASTEXITCODE -ne 0 -or !(Test-Path $shot) -or !(Select-String -Path $log -Pattern 'SCENE_SWITCH_VERIFIED' -Quiet) -or (Select-String -Path $log -Pattern 'Error:|Ensure condition failed' -Quiet)){throw 'Actual scene switch failed; inspect Saved/Logs/AirportSceneSwitch.log'}
Write-Output 'Actual local Qwen verified: basic models -> J20/J10, UI Apply -> airport map travel, screenshot captured.'
