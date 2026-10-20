param([string]$EngineRoot='', [switch]$Verify, [switch]$Resources)
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
# GPU trace calibration is unreliable on this machine; rendering remains fully enabled.
$arguments = @((Join-Path $root 'TacticalDemo.uproject'),'/Game/VRAirport/Maps/VRAirportMap?game=/Script/TacticalDemo.DemoGameMode','-game','-windowed','-ResX=1280','-ResY=720','-nosplash','-nop4','-trace=cpu,frame,log,bookmark')
if ($Resources) { $arguments += '-DemoResources' }
if ($Verify) {
    $evidence = Join-Path $root 'Saved\Evidence'
    New-Item -ItemType Directory -Path $evidence -Force | Out-Null
    $shot = Join-Path $evidence $(if ($Resources) { 'Resources.png' } else { 'Demo.png' })
    $log = Join-Path $root 'Saved\Logs\Runtime.log'
    if (Test-Path $shot) { Remove-Item -LiteralPath $shot }
    $arguments += @('-unattended','-nosound','-RenderOffscreen',"-DemoScreenshot=$shot",'-DemoScreenshotDelay=30','-DemoAutoExit','-DemoExitDelay=38','-ExecCmds=Demo.Reload',"-abslog=$log")
    & (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') @arguments
    if ($LASTEXITCODE -ne 0) { throw "Runtime failed: $LASTEXITCODE" }
    if (-not (Test-Path $shot)) { throw 'Runtime exited without a screenshot' }
    if (@(Select-String -Path $log -Pattern 'SCENE_LOADED').Count -lt 2) { throw 'Startup load and Demo.Reload were not both observed' }
    if (Select-String -Path $log -Pattern 'Error:|Ensure condition failed' -Quiet) { throw "Runtime log contains errors: $log" }
    Write-Output "Runtime evidence: $shot"
} else {
    & (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') @arguments
}
