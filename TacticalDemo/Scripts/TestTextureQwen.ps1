param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'StartQwen.ps1')
$report=Join-Path $root 'Saved/TextureQwenAutomation'
$index=Join-Path $report 'index.json'
if(Test-Path $index){Remove-Item -LiteralPath $index}
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project=Join-Path $root 'TacticalDemo.uproject'
& $editor $project -unattended -nullrhi -nosound -nop4 '-ExecCmds=Automation RunTests Integration.TextureQwen' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$root/Saved/Logs/TextureQwen.log"
if($LASTEXITCODE -ne 0){throw 'UE texture Qwen test process failed'}
$summary=Get-Content -Raw $index | ConvertFrom-Json
if($summary.failed -ne 0 -or $summary.notRun -ne 0 -or ($summary.succeeded+$summary.succeededWithWarnings) -ne 1){throw 'Actual Qwen texture test failed; see Saved/Logs/TextureQwen.log'}
$shot=Join-Path $root 'Saved/Evidence/AircraftTexture2.png'
$log=Join-Path $root 'Saved/Logs/TextureRuntime.log'
if(Test-Path $shot){Remove-Item -LiteralPath $shot}
& $editor $project '/Game/VRAirport/Maps/VRAirportMap?game=/Script/TacticalDemo.DemoGameMode' -game -windowed -ResX=1280 -ResY=720 -unattended -nosound -nop4 -RenderOffscreen '-trace=cpu,frame,log,bookmark' '-ExecCmds=DisableAllScreenMessages' -DemoVerifyTextures -DemoAutoExit -DemoScreenshotDelay=35 -DemoExitDelay=43 "-DemoScreenshot=$shot" "-abslog=$log"
if($LASTEXITCODE -ne 0 -or !(Test-Path $shot) -or !(Select-String -Path $log -Pattern 'TEXTURE_RUNTIME_VERIFIED' -Quiet) -or (Select-String -Path $log -Pattern 'Error:|Ensure condition failed' -Quiet)){throw 'Runtime texture switch failed; see Saved/Logs/TextureRuntime.log'}
Write-Output 'J20/J10 both textures verified through actual local Qwen, applied materials, unchanged positions, and real rendering.'
