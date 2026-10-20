param([int]$Port=18764)
$ErrorActionPreference='Stop'
$listener=[System.Net.HttpListener]::new()
$listener.Prefixes.Add("http://127.0.0.1:$Port/")
$listener.Start()
try {
    for($index=0;$index -lt 2;$index++) {
        $pending=$listener.GetContextAsync()
        if(-not $pending.Wait(120000)) { throw 'Fixture timed out waiting for UE' }
        $context=$pending.Result
        $reader=[System.IO.StreamReader]::new($context.Request.InputStream,[System.Text.Encoding]::UTF8)
        $request=$reader.ReadToEnd() | ConvertFrom-Json
        if($request.model -ne 'test-fixture' -or $request.messages.Count -lt 2 -or $request.stream) { throw 'Unexpected request schema' }
        if($index -eq 0) {
            $content=@{reply='测试接口：已生成候选（非真实 Qwen）。';scenario=@{scenarioName='HttpFixture';units=@(@{id='BLUE';modelId='aircraft_default';type='aircraft';side='blue';count=2;position=@{x=0;y=0;z=20};spacing=6})}} | ConvertTo-Json -Depth 8 -Compress
        } else {
            if($request.messages[0].content -notmatch 'HttpFixture') { throw 'Current applied scene missing in system context' }
            $content='{"reply":"bad","scenario":{"units":123}}'
        }
        $body=@{choices=@(@{message=@{role='assistant';content=$content}})} | ConvertTo-Json -Depth 8 -Compress
        $bytes=[System.Text.Encoding]::UTF8.GetBytes($body)
        $context.Response.ContentType='application/json; charset=utf-8'
        $context.Response.ContentLength64=$bytes.Length
        $context.Response.OutputStream.Write($bytes,0,$bytes.Length)
        $context.Response.Close()
    }
} finally { $listener.Stop(); $listener.Close() }
