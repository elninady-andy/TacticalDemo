param([ValidateRange(1,65535)][int]$Port=8000,[string]$RuntimeRoot='')
$ErrorActionPreference='Stop'
function Test-QwenHealth {
    # Windows PowerShell 5.1 (used by StartDemo.cmd) has no -NoProxy switch.
    # Disable the proxy explicitly on this one loopback request in both runtimes.
    $request=[System.Net.HttpWebRequest]::Create("http://127.0.0.1:$Port/health")
    $request.Proxy=$null
    $request.Timeout=2000
    $request.ReadWriteTimeout=2000
    $response=$null
    $reader=$null
    try {
        $response=$request.GetResponse()
        $reader=[System.IO.StreamReader]::new($response.GetResponseStream(),[System.Text.Encoding]::UTF8)
        $health=$reader.ReadToEnd() | ConvertFrom-Json
        if($health.status -eq 'ok' -and $health.model -eq 'qwen'){return $true}
        $script:HealthFailure='Health response did not identify a ready Qwen model.'
        return $false
    } catch [System.Net.WebException] {
        $script:HealthFailure=$_.Exception.Message
        if($_.Exception.Response){$_.Exception.Response.Close()}
        return $false
    } finally {
        if($reader){$reader.Dispose()}
        if($response){$response.Close()}
    }
}
. "$PSScriptRoot\PortablePaths.ps1"
$runtime=Resolve-PortableQwen $RuntimeRoot
$python=Join-Path $runtime 'venv\Scripts\python.exe'
$server=Join-Path $runtime 'qwen_local_server.py'
$model=Join-Path $runtime 'model'
foreach($required in @($python,$server,(Join-Path $model 'config.json'))){
    if(-not (Test-Path -LiteralPath $required)){throw "Qwen file missing: $required"}
}
$mutex=[System.Threading.Mutex]::new($false,'Local\TacticalDemoQwenStart')
$ownsMutex=$false
try {
    try {
        $ownsMutex=$mutex.WaitOne([TimeSpan]::FromMinutes(3))
    } catch [System.Threading.AbandonedMutexException] {
        $ownsMutex=$true
    }
    if(-not $ownsMutex){throw 'Timed out waiting for another Qwen startup to finish'}

    # Recheck after acquiring the mutex: another StartDemo may have completed
    # startup while this invocation was waiting.
    if(Test-QwenHealth){Write-Output "Qwen already ready: http://127.0.0.1:$Port"; return}

    $out=Join-Path $runtime 'ue-qwen.out.log'
    $err=Join-Path $runtime 'ue-qwen.err.log'
    $existing=@(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -eq 'python.exe' -and $_.CommandLine -and
        $_.CommandLine -match [regex]::Escape($server) -and
        $_.CommandLine -match "--port\s+$Port(?:\s|$)"
    })
    $process=$null
    if($existing.Count -eq 0){
        $process=Start-Process -FilePath $python -ArgumentList @("`"$server`"",'--model-dir',"`"$model`"",'--host','127.0.0.1','--port',"$Port") -WorkingDirectory $runtime -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
    }
    $deadline=(Get-Date).AddMinutes(2)
    do {
        if(Test-QwenHealth){
                $pidText=if($process){" (launcher PID $($process.Id))"}else{' (existing process)'}
                Write-Output "Qwen ready: http://127.0.0.1:$Port$pidText"
                return
        }
        # The venv launcher may exit while its real Python child continues loading.
        # Health is authoritative; launcher lifetime is not.
        Start-Sleep -Milliseconds 750
    } while((Get-Date) -lt $deadline)
    $tail=if(Test-Path -LiteralPath $err){(Get-Content -LiteralPath $err -Tail 12) -join [Environment]::NewLine}else{'No server error log was created.'}
    throw "Qwen startup timed out after 2 minutes. Health check: $script:HealthFailure`nLog tail:`n$tail"
} finally {
    if($ownsMutex){$mutex.ReleaseMutex()}
    $mutex.Dispose()
}
