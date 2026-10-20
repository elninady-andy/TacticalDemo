param([string]$EngineRoot='', [ValidateSet('airport','map3','both')][string]$Scene='both')
$ErrorActionPreference='Stop'
. "$PSScriptRoot/PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$projectRoot=Split-Path $PSScriptRoot -Parent
$outputRoot=Join-Path $projectRoot 'Saved/FormationLanguage'
$scenes=Get-Content (Join-Path $projectRoot 'Config/SceneLibrary.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$selected=if($Scene -eq 'both') { @('airport','map3') } else { @($Scene) }
foreach($key in $selected) {
    $destination=Join-Path $outputRoot $key
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    $log=Join-Path $destination 'Runtime.log'
    $started=Get-Date
    $arguments=@((Join-Path $projectRoot 'TacticalDemo.uproject'),$scenes.scenes.$key.asset,'-game','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-nosound','-nop4','-unattended','-trace=cpu,frame,log,bookmark','-DemoVerifyFormationLanguage','-DemoAutoExit','-DemoExitDelay=20',"-abslog=$log")
    & (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @arguments
    if($LASTEXITCODE -ne 0) { throw "UE exit $LASTEXITCODE for $key" }
    $text=Get-Content -LiteralPath $log -Raw -Encoding UTF8
    if($text -match 'Error:|Ensure condition failed|FORMATION_LANGUAGE_RUNTIME_FAILED') { throw "Formation verification failed: $log" }
    if(!$text.Contains('FORMATION_LANGUAGE_RUNTIME_VERIFIED')) { throw "Missing formation acceptance marker: $log" }
    foreach($name in @('CreationCandidate.png','Arranged.png')) {
        $file=Join-Path $outputRoot $name
        if(!(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).LastWriteTime -lt $started) { throw "Missing new screenshot: $name" }
        Copy-Item -LiteralPath $file -Destination (Join-Path $destination $name) -Force
    }
    Write-Output "FORMATION_LANGUAGE_PASSED $key creation, manual apply, arrangement, undo/redo, local dispatch and new rendered screenshots."
}
