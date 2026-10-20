param([string]$EngineRoot='', [ValidateSet('airport','map3','both')][string]$Scene='both')
$ErrorActionPreference='Stop'
. "$PSScriptRoot/PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$projectRoot=Split-Path $PSScriptRoot -Parent
$outputRoot=Join-Path $projectRoot 'Saved/MultiSelection'
$scenes=Get-Content (Join-Path $projectRoot 'Config/SceneLibrary.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$selected=if($Scene -eq 'both') { @('airport','map3') } else { @($Scene) }
foreach($key in $selected) {
    $destination=Join-Path $outputRoot $key
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    $log=Join-Path $destination 'Runtime.log'
    $started=Get-Date
    $arguments=@((Join-Path $projectRoot 'TacticalDemo.uproject'),$scenes.scenes.$key.asset,'-game','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-nosound','-nop4','-unattended','-trace=cpu,frame,log,bookmark','-DemoVerifySelection','-DemoAutoExit','-DemoExitDelay=31',"-abslog=$log")
    & (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @arguments
    if($LASTEXITCODE -ne 0) { throw "UE exit $LASTEXITCODE for $key" }
    $text=Get-Content -LiteralPath $log -Raw -Encoding UTF8
    if($text -match 'Error:|Ensure condition failed|MULTI_SELECTION_RUNTIME_FAILED') { throw "Selection/route verification failed: $log" }
    if(!$text.Contains('MULTI_SELECTION_RUNTIME_VERIFIED')) { throw "Missing acceptance marker: $log" }
    foreach($name in @('Selected.png','BatchEditor.png','RouteEditor.png','RouteMoving.png')) {
        $file=Join-Path $outputRoot $name
        if(!(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).LastWriteTime -lt $started) { throw "Missing new screenshot: $name" }
        Copy-Item -LiteralPath $file -Destination (Join-Path $destination $name) -Force
    }
    Write-Output "MULTI_SELECTION_PASSED $key picking, multiselect glow, batch formation/edit, routes, saving and actual playback."
}
