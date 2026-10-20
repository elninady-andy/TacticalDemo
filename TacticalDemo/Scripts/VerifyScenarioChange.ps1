param([string]$EngineRoot='')
. "$PSScriptRoot\PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$path = Join-Path $root 'Config/Scenario.json'
$evidence = Join-Path $root 'Saved/Evidence'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$original = [IO.File]::ReadAllBytes($path)
[IO.File]::WriteAllBytes((Join-Path $evidence 'Scenario.original.json'), $original)
$variant = [Text.Encoding]::UTF8.GetString($original) | ConvertFrom-Json
if ($variant.units.Count -lt 1) { throw 'Need at least one unit group for visual comparison' }
$variant.scenarioName = 'Config change - blue aircraft'
$variant.units[0].count = 5
$variant.units[0].side = 'blue'
$variant.units[0].position.z = 12
$variantText = $variant | ConvertTo-Json -Depth 12
$written = [Text.UTF8Encoding]::new($false).GetBytes($variantText)
try {
    [IO.File]::WriteAllBytes($path, $written)
    & (Join-Path $PSScriptRoot 'RunDemo.ps1') -EngineRoot $EngineRoot -Verify
    $expected = ($variant.units | Measure-Object -Property count -Sum).Sum
    $log = Join-Path $root 'Saved/Logs/Runtime.log'
    if (-not (Select-String -Path $log -Pattern "SCENE_LOADED.*count=$expected$" -Quiet)) { throw "Expected $expected generated units" }
    Copy-Item -LiteralPath (Join-Path $evidence 'Demo.png') -Destination (Join-Path $evidence 'ChangedScenario.png')
    Copy-Item -LiteralPath $path -Destination (Join-Path $evidence 'Scenario.changed.json')
    Copy-Item -LiteralPath $log -Destination (Join-Path $evidence 'ChangedScenario.log')
} finally {
    $current = [IO.File]::ReadAllBytes($path)
    if ([Convert]::ToBase64String($current) -eq [Convert]::ToBase64String($written)) {
        [IO.File]::WriteAllBytes($path, $original)
    } else {
        throw "Scenario changed externally; not overwritten. Original backup: $evidence/Scenario.original.json"
    }
}
& (Join-Path $PSScriptRoot 'RunDemo.ps1') -EngineRoot $EngineRoot -Verify
Write-Output 'Configuration-change render verified; original Scenario.json restored byte-for-byte.'
