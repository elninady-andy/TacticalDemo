. "$PSScriptRoot\PortablePaths.ps1"
$portableServer=Join-Path (Resolve-PortableQwen '') 'qwen_local_server.py'
$ErrorActionPreference='Stop'
$startScript=Join-Path $PSScriptRoot 'StartQwen.ps1'

function Get-QwenServerProcesses {
    @(Get-CimInstance Win32_Process | Where-Object {
        $_.CommandLine -and $_.CommandLine -match [regex]::Escape($portableServer)
    })
}

# A cold start must survive the venv launcher process exiting while its real
# Python child loads the model. This is the production failure this test catches.
$existing=Get-QwenServerProcesses
foreach($process in $existing){Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue}
$deadline=(Get-Date).AddSeconds(20)
while((Get-NetTCPConnection -LocalPort 8000 -State Listen -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline){Start-Sleep -Milliseconds 250}
if(Get-NetTCPConnection -LocalPort 8000 -State Listen -ErrorAction SilentlyContinue){throw 'Port 8000 did not close before cold-start test'}

# Two near-simultaneous launches model a double-click or immediate retry. They
# must converge on one launcher/child pair instead of loading the model twice.
$pwsh=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$evidence=Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved\Evidence'
$first=Start-Process -FilePath $pwsh -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$startScript`"") -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $evidence 'StartQwenConcurrent1.log') -RedirectStandardError (Join-Path $evidence 'StartQwenConcurrent1.err.log')
$second=Start-Process -FilePath $pwsh -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$startScript`"") -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $evidence 'StartQwenConcurrent2.log') -RedirectStandardError (Join-Path $evidence 'StartQwenConcurrent2.err.log')
$first.WaitForExit(150000) | Out-Null
$second.WaitForExit(150000) | Out-Null
if(-not $first.HasExited -or -not $second.HasExited){throw 'Concurrent startup scripts did not finish'}
if($first.ExitCode -ne 0 -or $second.ExitCode -ne 0){throw "Concurrent startup failed: exits $($first.ExitCode), $($second.ExitCode)"}
$health=Invoke-RestMethod -Uri 'http://127.0.0.1:8000/health' -NoProxy -TimeoutSec 5
if($health.status -ne 'ok'){throw 'Concurrent cold start returned without a healthy Qwen server'}
$firstIds=@(Get-QwenServerProcesses | Sort-Object ProcessId | Select-Object -ExpandProperty ProcessId)
if($firstIds.Count -ne 2){throw "Concurrent cold start created duplicate model processes: $($firstIds -join ',')"}

# Starting again must reuse the healthy service and create no duplicate model.
& $pwsh -NoProfile -ExecutionPolicy Bypass -File $startScript
if($LASTEXITCODE -ne 0){throw 'Repeated startup failed under Windows PowerShell 5.1'}
$secondIds=@(Get-QwenServerProcesses | Sort-Object ProcessId | Select-Object -ExpandProperty ProcessId)
if(($firstIds -join ',') -ne ($secondIds -join ',')){throw "Second start created duplicate processes: $($firstIds -join ',') -> $($secondIds -join ',')"}
Write-Output "Qwen concurrent cold start and repeated start passed. Processes: $($secondIds -join ',')"
