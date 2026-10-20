$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$required = @('TacticalDemo.uproject','Source/TacticalDemo/TacticalDemo.Build.cs','Source/TacticalDemo/ScenarioConfig.h','Source/TacticalDemo/ScenarioConfigLoader.cpp','Source/TacticalDemo/EquipmentLibrary.cpp','Source/TacticalDemo/DemoScenarioManager.cpp','Source/TacticalDemo/DemoUnitActor.cpp','Source/TacticalDemo/DemoGameMode.cpp','Source/TacticalDemo/FlightPreview.cpp','Config/Scenario.json','Config/AirportScenario.json','Config/EquipmentLibrary.json','Config/SceneLibrary.json','Config/Map3FlightPaths.json','Content/Maps/Demo.umap','Content/Materials/M_Unit.uasset')
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf)) { throw "Missing: $relative" }
}
$library = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'Config/EquipmentLibrary.json') | ConvertFrom-Json
$scenes = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'Config/SceneLibrary.json') | ConvertFrom-Json
$flight = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'Config/Map3FlightPaths.json') | ConvertFrom-Json
function Test-FiniteNumber($Value) {
    return $null -ne $Value -and $Value -is [ValueType] -and $Value -isnot [bool] -and ![double]::IsNaN([double]$Value) -and ![double]::IsInfinity([double]$Value)
}
function Require-Asset([string]$Asset,[string]$Extension) {
    if(!$Asset.StartsWith('/Game/')) { throw "Expected project asset: $Asset" }
    $relative='Content/'+$Asset.Split('.')[0].Substring(6)+$Extension
    if(!(Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf)) { throw "Missing: $relative" }
}
function Check-References($Value) {
    if($Value -is [string]) {
        if($Value.StartsWith('/Game/')) { Require-Asset $Value '.uasset' }
    } elseif($Value -is [Array]) {
        foreach($item in $Value) { Check-References $item }
    } elseif($Value -is [pscustomobject]) {
        foreach($field in $Value.PSObject.Properties) { Check-References $field.Value }
    }
}
$scenarioFiles=@('Scenario.json','AirportScenario.json')
$materialSettings=@{}
foreach($line in (Get-Content -LiteralPath (Join-Path $root 'Config/DefaultGame.ini') -Encoding UTF8)) {
    if($line -match '^(UnitMaterialPath|HighlightMaterialPath)=(.+)$') { $materialSettings[$Matches[1]]=$Matches[2].Trim() }
}
foreach($key in @('UnitMaterialPath','HighlightMaterialPath')) {
    if(!$materialSettings[$key]) { throw "Missing material setting: $key" }
    Require-Asset $materialSettings[$key] '.uasset'
}
if(!$scenes.scenes -or @($scenes.scenes.PSObject.Properties).Count -eq 0) { throw 'Empty scene library' }
foreach($entry in $scenes.scenes.PSObject.Properties) {
    Require-Asset $entry.Value.asset '.umap'
    if($entry.Value.scenario) {
        $name=$entry.Value.scenario
        if($name -isnot [string] -or $name -ne [IO.Path]::GetFileName($name) -or ! $name.EndsWith('.json')) { throw 'Invalid scenario filename' }
        $scenarioFiles+=$name
    }
}
Require-Asset $flight.mapAsset '.umap'
if(!$flight.paths -or @($flight.paths).Count -eq 0) { throw 'Empty flight paths' }
Check-References $library
$scenarioFiles=@($scenarioFiles | Sort-Object -Unique)
$allUnits=0
foreach($name in $scenarioFiles) {
$path=Join-Path $root ('Config/'+$name)
if(!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing: Config/$name" }
$scenario=Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
$total = 0
$ids = @{}
if ($scenario.scenarioName -isnot [string] -or [string]::IsNullOrWhiteSpace($scenario.scenarioName) -or $scenario.units -isnot [Array]) { throw 'Invalid scenarioName/units' }
foreach ($unit in $scenario.units) {
    if ($unit.id -isnot [string] -or [string]::IsNullOrWhiteSpace($unit.id) -or $ids.ContainsKey($unit.id)) { throw 'Missing or duplicate id' }
    if ($unit.modelId -isnot [string] -or [string]::IsNullOrWhiteSpace($unit.modelId)) { throw 'Invalid modelId' }
    $ids[$unit.id] = $true
    if ($unit.count -isnot [ValueType] -or $unit.count -is [bool] -or $unit.count -lt 0 -or $unit.count -gt 100 -or $unit.count -ne [math]::Floor($unit.count)) { throw "Invalid count: $($unit.id)" }
    if ($unit.side -cnotin @('red','blue')) { throw 'Invalid side' }
    if ($unit.type -cnotin @('aircraft','vehicle','radar','target')) { throw 'Invalid type' }
    foreach ($axis in @('x','y','z')) {
        $n = $unit.position.$axis
        if (!(Test-FiniteNumber $n) -or [math]::Abs($n) -gt 1000000) { throw "Invalid position.$axis" }
    }
    $spacing = 10
    if ($unit.PSObject.Properties.Name -contains 'spacing') { $spacing = $unit.spacing }
    if (!(Test-FiniteNumber $spacing) -or $spacing -le 0 -or $spacing -gt 10000) { throw 'Invalid spacing' }
    if (!$unit.PSObject.Properties['instances'] -and [math]::Abs($unit.position.y + [math]::Max(0, $unit.count-1)*$spacing) -gt 1000000) { throw 'Generated positions exceed range' }
    $total += $unit.count
}
if ($total -gt 500) { throw 'Total exceeds 500' }
$allUnits+=$total
}
foreach ($type in @('aircraft','vehicle','radar','target')) {
    $id = $library.defaults.$type
    if (-not $id -or -not $library.models.$id -or $library.models.$id.type -cne $type) { throw "Invalid default: $type" }
}
foreach ($entry in $library.models.PSObject.Properties) {
    $model = $entry.Value
    if ($model.type -cnotin @('aircraft','vehicle','radar','target') -or $model.kind -cne 'staticMesh' -or $model.asset -isnot [string] -or [string]::IsNullOrWhiteSpace($model.asset)) { throw "Invalid model: $($entry.Name)" }
}
Write-Output "Level 1 passed: basic JSON and registered resource files checked; $($scenarioFiles.Count) scenarios, $allUnits configured units. Runtime C++/UE asset validation is authoritative."
