param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$evidence=Join-Path $root 'Saved/Evidence'
$fixture=Start-Process -FilePath (Get-Process -Id $PID).Path -ArgumentList @('-NoProfile','-File',"`"$PSScriptRoot/ChatHttpFixture.ps1`"") -WindowStyle Hidden -PassThru -RedirectStandardOutput "$evidence/ChatFixture.log" -RedirectStandardError "$evidence/ChatFixture.error.log"
try {
    $report=Join-Path $root 'Saved/ChatAutomation'
    $index=Join-Path $report 'index.json'
    if(Test-Path $index) { Remove-Item -LiteralPath $index }
    & "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$root/TacticalDemo.uproject" -unattended -nullrhi -nosound -nop4 '-ExecCmds=Automation RunTests Integration.LocalChatHTTP' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" '-DemoChatTestEndpoint=http://127.0.0.1:18764/v1/chat/completions' "-abslog=$root/Saved/Logs/ChatHttp.log"
    if($LASTEXITCODE -ne 0) { throw 'UE HTTP test process failed' }
    $summary=Get-Content -Raw $index | ConvertFrom-Json
    if($summary.failed -ne 0 -or ($summary.succeeded+$summary.succeededWithWarnings) -ne 1) { throw 'HTTP integration failed; inspect Saved/Logs/ChatHttp.log' }
    $fixture.WaitForExit(5000) | Out-Null
    if(-not $fixture.HasExited -or $fixture.ExitCode -ne 0) { throw 'HTTP fixture did not complete successfully' }
    Write-Output 'Local fixture HTTP round trip passed. This does not verify the real Qwen model.'
} finally {
    if(-not $fixture.HasExited) { Stop-Process -Id $fixture.Id }
}
