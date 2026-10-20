param([string]$EngineRoot='')
$ErrorActionPreference='Stop'
. "$PSScriptRoot/PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root 'Saved/MoveImport'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$log=Join-Path $out 'FlightRuntime-Release.log'
$started=Get-Date
$arguments=@((Join-Path $root 'TacticalDemo.uproject'),'/Game/Maps/map3','-game','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-nosound','-nop4','-unattended','-trace=cpu,frame,log,bookmark','-DemoVerifyFlightPreview',"-DemoScreenshot=$out/FlightStopped.png",'-DemoScreenshotDelay=22','-DemoAutoExit','-DemoExitDelay=26',"-abslog=$log")
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @arguments
if($LASTEXITCODE -ne 0) { throw "UE exit $LASTEXITCODE" }
$text=Get-Content -LiteralPath $log -Raw
if($text -match 'Error:|Ensure condition failed|FLIGHT_RUNTIME_FAILED') { throw "Flight verification failed: $log" }
foreach($marker in @('FLIGHT_RUNTIME_MOVING','FLIGHT_FOLLOW_VERIFIED BLUE01_1','FLIGHT_RUNTIME_VERIFIED')) {
    if(!$text.Contains($marker)) { throw "Missing acceptance marker: $marker" }
}
foreach($name in @('FlightMoving.png','FlightF35.png','FlightStopped.png')) {
    $file=Join-Path $out $name
    if(!(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).LastWriteTime -lt $started) { throw "Missing new screenshot: $name" }
}
Write-Output 'FLIGHT_PREVIEW_PASSED movement, selection follow, snapshot isolation, stop and rendered screenshots.'
