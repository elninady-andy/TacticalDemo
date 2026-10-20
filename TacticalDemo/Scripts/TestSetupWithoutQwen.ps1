param([string]$ShellExe='powershell.exe')
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$caseRoot=Join-Path ([IO.Path]::GetTempPath()) ('TacticalDemo-SetupWithoutQwen-'+[Guid]::NewGuid().ToString('N'))
$fixture=Join-Path $caseRoot 'TacticalDemo'
$engine=Join-Path $caseRoot 'Engine'
$runtime=Join-Path $caseRoot 'QwenRuntime'
New-Item -ItemType Directory -Path "$fixture/Scripts","$fixture/Config","$engine/Engine/Binaries/Win64" -Force | Out-Null
foreach($script in @('SetupPortable.ps1','PortablePaths.ps1','Validate.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $script) -Destination "$fixture/Scripts"
}
Get-ChildItem -LiteralPath "$project/Config" -Filter '*.json' -File | Copy-Item -Destination "$fixture/Config"
Copy-Item -LiteralPath "$project/Config/DefaultGame.ini" -Destination "$fixture/Config"
# Stand-ins exercise presence/JSON checks only; no engine or model is started.
$files=@('TacticalDemo.uproject','Source/TacticalDemo/TacticalDemo.Build.cs','Source/TacticalDemo/ScenarioConfig.h','Source/TacticalDemo/ScenarioConfigLoader.cpp','Source/TacticalDemo/EquipmentLibrary.cpp','Source/TacticalDemo/DemoScenarioManager.cpp','Source/TacticalDemo/DemoUnitActor.cpp','Source/TacticalDemo/DemoGameMode.cpp','Source/TacticalDemo/FlightPreview.cpp','Content/Materials/M_Unit.uasset')
$library=Get-Content "$fixture/Config/EquipmentLibrary.json" -Raw -Encoding UTF8
foreach($line in (Get-Content "$fixture/Config/DefaultGame.ini" -Encoding UTF8)) {
    if($line -match '^(UnitMaterialPath|HighlightMaterialPath)=(/Game/.+)$') { $files+='Content/'+$Matches[2].Split('.')[0].Substring(6)+'.uasset' }
}
foreach($match in [regex]::Matches($library,'"(/Game/[^"\s]+)"')) {
    $files+='Content/'+$match.Groups[1].Value.Split('.')[0].Substring(6)+'.uasset'
}
$scenes=Get-Content "$fixture/Config/SceneLibrary.json" -Raw -Encoding UTF8 | ConvertFrom-Json
foreach($scene in $scenes.scenes.PSObject.Properties) { $files+='Content/'+$scene.Value.asset.Substring(6)+'.umap' }
foreach($relative in ($files | Sort-Object -Unique)) {
    $file=Join-Path $fixture $relative
    New-Item -ItemType Directory -Path (Split-Path $file -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($file,'fixture')
}
$editor=Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
[IO.File]::WriteAllText($editor,'fixture')
[IO.File]::WriteAllText("$fixture/Scripts/Build.ps1","throw 'CheckOnly unexpectedly invoked Build.ps1'")
# Isolate host auto-discovery while exercising the real engine file check.
@'
param([string]$EngineRoot,[string]$ProjectPath,[switch]$SkipQwen)
$env:UE_ENGINE_ROOT=''
$env:QWEN_RUNTIME_ROOT=Join-Path $PSScriptRoot 'QwenRuntime'
$env:ProgramFiles=Join-Path $PSScriptRoot 'NoInstalledPrograms'
function Get-ItemProperty {
    param([string]$LiteralPath,[string]$ErrorAction)
    if($LiteralPath -ne 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.3') { throw "Unexpected registry lookup: $LiteralPath" }
    return $null
}
& "$ProjectPath/Scripts/SetupPortable.ps1" -EngineRoot $EngineRoot -SkipQwen:$SkipQwen -CheckOnly
'@ | Set-Content -LiteralPath "$caseRoot/RunCase.ps1" -Encoding UTF8
$results=@()
function Check([string]$Name,[bool]$ShouldPass,[bool]$Skip,[string]$Expected,[string]$EnginePath=$engine,[string]$ProjectPath=$fixture) {
    $start=New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName=(Get-Command $ShellExe -ErrorAction Stop).Source
    $start.Arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$caseRoot+'/RunCase.ps1" -EngineRoot "'+$EnginePath+'" -ProjectPath "'+$ProjectPath+'"'
    if($Skip) { $start.Arguments+=' -SkipQwen' }
    $start.UseShellExecute=$false
    $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true
    $start.RedirectStandardError=$true
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo=$start
    try {
        [void]$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync()
        $stderr=$process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $output=$stdout.Result+"`n"+$stderr.Result
        $code=$process.ExitCode
    } finally { $process.Dispose() }
    $output | Out-File -LiteralPath "$caseRoot/$Name.log" -Encoding UTF8
    if(($code -eq 0) -ne $ShouldPass -or $output -notlike "*$Expected*") {
        Write-Output $output
        throw "Case failed: $Name (exit $code); see $caseRoot/$Name.log"
    }
    if($ShouldPass -and ($output -notlike '*Level 1 passed:*' -or $output -notlike "*UE: $EnginePath;*")) { throw "Checks not completed: $Name; see $caseRoot/$Name.log" }
    if(Test-Path -LiteralPath (Join-Path (Split-Path $ProjectPath -Parent) 'LocalPaths.json')) { throw "CheckOnly wrote LocalPaths.json: $Name" }
    $script:results+=$Name
}
Check 'default-missing-runtime' $false $false 'Qwen model files missing'
Check 'skip-missing-runtime' $true $true 'Portable paths verified'
if(Test-Path -LiteralPath $runtime) { throw 'SkipQwen created a runtime directory' }
Check 'skip-missing-engine' $false $true 'UE 5.3 not found' (Join-Path $caseRoot 'MissingEngine')
$missingMapFixture=Join-Path $caseRoot 'MissingMap/TacticalDemo'
foreach($file in (Get-ChildItem -LiteralPath $fixture -Recurse -File)) {
    $relative=$file.FullName.Substring($fixture.Length+1)
    if($relative -eq 'Content\Maps\map3.umap') { continue }
    $destination=Join-Path $missingMapFixture $relative
    New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
}
Check 'skip-missing-map3' $false $true 'Content/Maps/map3.umap' $engine $missingMapFixture
New-Item -ItemType Directory -Path "$runtime/model" -Force | Out-Null
Check 'skip-missing-model-config' $true $true 'Portable paths verified'
[IO.File]::WriteAllText("$runtime/model/config.json",'{}')
Check 'default-model-present' $true $false 'Portable paths verified'
@{shell=$ShellExe;passed=$results;fixture=$fixture} | ConvertTo-Json -Depth 4 | Set-Content "$caseRoot/Result.json" -Encoding UTF8
Write-Output "SETUP_WITHOUT_QWEN_TESTS_PASSED cases=$($results.Count) report=$caseRoot/Result.json"
