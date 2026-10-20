param([string]$EngineRoot='',[switch]$Formation)
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project=Join-Path $root 'TacticalDemo.uproject'
$evidence=Join-Path $root 'Saved/Evidence'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
foreach($phase in @('Edit','Restart')) {
    $restore=$phase -eq 'Restart'
    $map=if($restore){'/Game/Maps/Demo'}else{'/Game/VRAirport/Maps/VRAirportMap'}
    $flag=if($restore){'-DemoVerifyLayoutRestore'}else{'-DemoVerifyLayout'}
    $marker=if($restore){'LAYOUT_RESTART_VERIFIED'}else{'LAYOUT_RUNTIME_VERIFIED'}
    $prefix=if($Formation){'Formation'}else{'Layout'}
    $shot=Join-Path $evidence "$prefix$phase.png"
    $log=Join-Path $root "Saved/Logs/$prefix$phase.log"
    [string[]]$extra=@()
    if($Formation){$extra=@('-DemoVerifyFormation')}
    if(Test-Path -LiteralPath $shot){Remove-Item -LiteralPath $shot}
    & $editor $project "$map`?game=/Script/TacticalDemo.DemoGameMode" -game -windowed -ResX=1600 -ResY=900 -unattended -nosound -nop4 -RenderOffscreen '-trace=cpu,frame,log,bookmark' '-ExecCmds=DisableAllScreenMessages' $flag @extra -DemoAutoExit -DemoScreenshotDelay=24 -DemoExitDelay=32 "-DemoScreenshot=$shot" "-abslog=$log"
    if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $shot) -or !(Select-String -LiteralPath $log -Pattern $marker -Quiet) -or (Select-String -LiteralPath $log -Pattern 'Error:|Ensure condition failed' -Quiet)){
        throw "Layout $phase verification failed. See $log"
    }
    Write-Output "$phase verified: $shot"
    if($Formation -and !$restore -and !(Select-String -LiteralPath $log -Pattern 'FORMATION_RUNTIME_VERIFIED' -Quiet)){ throw 'Formation runtime verification marker missing' }
}
Write-Output 'Airport editing and saved preset restoration through restart/map travel verified.'
