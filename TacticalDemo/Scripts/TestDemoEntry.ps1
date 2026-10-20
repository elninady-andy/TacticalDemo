$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$evidence=Join-Path $root 'Saved\Evidence'
$launcher=Join-Path $root 'StartDemo.cmd'
$out=Join-Path $evidence 'StartDemoEntry.log'
$err=Join-Path $evidence 'StartDemoEntry.err.log'
$started=Get-Date
# Run the actual batch entry, including its Windows PowerShell 5.1 invocations.
# -Verify uses the same launch path but automatically renders, captures and exits.
$entry=Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',"`"$launcher`"",'-Verify') -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
if(-not $entry.WaitForExit(180000)){throw "Batch entry did not finish. See $out and $err"}
if($entry.ExitCode -ne 0){throw "Batch entry returned $($entry.ExitCode). See $out and $err"}
$result=Get-Content -LiteralPath $out -Raw
if($result -notmatch 'Qwen (already )?ready:' -or $result -notmatch 'Runtime evidence:'){throw 'Batch exited without completing both Qwen and UE startup'}
$shot=Get-Item -LiteralPath (Join-Path $evidence 'Demo.png')
if($shot.LastWriteTime -lt $started){throw 'Batch did not generate a new UE screenshot'}
Write-Output 'Actual StartDemo.cmd passed through Windows PowerShell 5.1: Qwen ready, UE rendered, new screenshot, exit 0.'
