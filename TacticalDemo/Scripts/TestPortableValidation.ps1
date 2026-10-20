param([string]$ShellExe='powershell.exe')
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$caseRoot=Join-Path $project ('Saved/PortableValidation/'+[Guid]::NewGuid().ToString('N'))
$fixture=Join-Path $caseRoot 'TacticalDemo'
New-Item -ItemType Directory -Path "$fixture/Scripts","$fixture/Config" -Force | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot/Validate.ps1" -Destination "$fixture/Scripts/Validate.ps1"
Get-ChildItem -LiteralPath "$project/Config" -Filter '*.json' -File | Copy-Item -Destination "$fixture/Config"
Copy-Item -LiteralPath "$project/Config/DefaultGame.ini" -Destination "$fixture/Config"
# Empty stand-ins exercise file-presence checks only. UE tests validate actual assets.
$files=@('TacticalDemo.uproject','Source/TacticalDemo/TacticalDemo.Build.cs','Source/TacticalDemo/ScenarioConfig.h','Source/TacticalDemo/ScenarioConfigLoader.cpp','Source/TacticalDemo/EquipmentLibrary.cpp','Source/TacticalDemo/DemoScenarioManager.cpp','Source/TacticalDemo/DemoUnitActor.cpp','Source/TacticalDemo/DemoGameMode.cpp','Source/TacticalDemo/FlightPreview.cpp','Content/Materials/M_Unit.uasset')
$library=Get-Content "$fixture/Config/EquipmentLibrary.json" -Raw -Encoding UTF8
foreach($line in (Get-Content "$fixture/Config/DefaultGame.ini" -Encoding UTF8)) {
    if($line -match '^(UnitMaterialPath|HighlightMaterialPath)=(/Game/.+)$') { $files+='Content/'+$Matches[2].Split('.')[0].Substring(6)+'.uasset' }
}
foreach($match in [regex]::Matches($library,'"(/Game/[^"\s]+)"')) {
    $package=$match.Groups[1].Value.Split('.')[0]
    $files+='Content/'+$package.Substring(6)+'.uasset'
}
$scenes=Get-Content "$fixture/Config/SceneLibrary.json" -Raw -Encoding UTF8 | ConvertFrom-Json
foreach($scene in $scenes.scenes.PSObject.Properties) { $files+='Content/'+$scene.Value.asset.Substring(6)+'.umap' }
foreach($relative in ($files | Sort-Object -Unique)) {
    $file=Join-Path $fixture $relative
    New-Item -ItemType Directory -Path (Split-Path $file -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($file,'fixture')
}
$results=@()
function Check([string]$Name,[bool]$ShouldPass,[string]$Expected='') {
    $output=& $ShellExe -NoProfile -ExecutionPolicy Bypass -File "$fixture/Scripts/Validate.ps1" 2>&1
    $code=$LASTEXITCODE
    $output | Out-File -LiteralPath "$caseRoot/$Name.log" -Encoding utf8
    if(($code -eq 0) -ne $ShouldPass -or ($Expected -and ($output -join "`n") -notlike "*$Expected*")) { throw "Case failed: $Name (exit $code); see $caseRoot" }
    $script:results+=$Name
}
Check 'complete-fixture' $true
function Missing([string]$Name,[string]$Relative) {
    $file=Join-Path $fixture $Relative
    [IO.File]::Move($file,$file+'.test-backup')
    try { Check $Name $false ([IO.Path]::GetFileName($file)) }
    finally { [IO.File]::Move($file+'.test-backup',$file) }
}
Missing 'missing-map3' 'Content/Maps/map3.umap'
Missing 'missing-flight-config' 'Config/Map3FlightPaths.json'
Missing 'missing-f35' 'Content/plane/F35/mesh/SM_F35.uasset'
Missing 'missing-highlight' 'Content/Materials/M_AircraftHighlight.uasset'
$mapFile="$fixture/Config/Map3Scenario.json"
$original=[IO.File]::ReadAllText($mapFile)
try {
    [IO.File]::WriteAllText($mapFile,$original.Replace('"count":3','"count":-1'),[Text.UTF8Encoding]::new($false))
    Check 'invalid-map3-count' $false 'Invalid count'
} finally { [IO.File]::WriteAllText($mapFile,$original,[Text.UTF8Encoding]::new($false)) }
Check 'restored-fixture' $true
@{shell=$ShellExe;passed=$results;fixture=$fixture} | ConvertTo-Json -Depth 4 | Set-Content "$caseRoot/Result.json" -Encoding UTF8
Write-Output "PORTABLE_VALIDATION_TESTS_PASSED cases=$($results.Count) report=$caseRoot/Result.json"
