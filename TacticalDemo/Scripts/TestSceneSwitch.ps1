param([string]$EngineRoot='', [string]$StartScene='airport', [string]$TargetScene='map3')
$ErrorActionPreference='Stop'
. "$PSScriptRoot/PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$root=Split-Path $PSScriptRoot -Parent
$scenes=(Get-Content -LiteralPath "$root/Config/SceneLibrary.json" -Raw | ConvertFrom-Json).scenes
if($StartScene -eq $TargetScene -or $StartScene -notmatch '^[a-zA-Z0-9_-]+$' -or $TargetScene -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Specify two different registered scene ids.' }
$source=$scenes.PSObject.Properties[$StartScene]
$target=$scenes.PSObject.Properties[$TargetScene]
if(-not $source -or -not $target) { throw 'Scene id is not registered.' }
& "$PSScriptRoot/StartQwen.ps1"
$out=Join-Path $root 'Saved/Map3Import'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$name="$StartScene-to-$TargetScene"
$log=Join-Path $out "$name.log"
$shot=Join-Path $out "$name.png"
$started=Get-Date
$arguments=@((Join-Path $root 'TacticalDemo.uproject'),$source.Value.asset,'-game','-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-nop4','-nosplash','-RenderOffscreen','-trace=cpu,frame,log,bookmark','-DemoVerifySceneSwitch',"-DemoSceneTarget=$TargetScene","-DemoScreenshot=$shot",'-DemoScreenshotDelay=20','-DemoAutoExit','-DemoExitDelay=25',"-abslog=$log")
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @arguments
if($LASTEXITCODE -ne 0) { throw "Runtime exit $LASTEXITCODE" }
$text=Get-Content -LiteralPath $log -Raw
if($text -match 'Error:|Ensure condition failed' -or $text -notmatch "SCENE_SWITCH_CANDIDATE_VERIFIED $TargetScene " -or $text -notmatch "SCENE_SWITCH_VERIFIED $TargetScene ") { throw "Scene travel acceptance failed: $log" }
if(-not (Test-Path -LiteralPath $shot) -or (Get-Item -LiteralPath $shot).LastWriteTime -lt $started) { throw 'New target screenshot missing.' }
Write-Output "SCENE_TRAVEL_PASSED $StartScene -> $TargetScene; Qwen candidate, unchanged before Apply, loaded target, rendered screenshot."
