param([string]$EngineRoot='', [string]$VisualStudioRoot='')
$ErrorActionPreference='Stop'
. "$PSScriptRoot/../PortablePaths.ps1"
$EngineRoot=Resolve-PortableEngine $EngineRoot
if(-not $VisualStudioRoot) {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioRoot=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if(-not $VisualStudioRoot) { throw 'C++ toolchain not found; pass -VisualStudioRoot.' }
& (Join-Path $VisualStudioRoot 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$project=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$out=Join-Path $project 'Saved/Map3Import'
New-Item -ItemType Directory -Path "$out/Tools" -Force | Out-Null
$sdk=Join-Path $EngineRoot 'Engine/Source/ThirdParty/FBX/2020.2'
$exe=Join-Path $out 'Tools/ExtractScene.exe'
& cl.exe /nologo /EHsc /MD /utf-8 /DFBXSDK_SHARED "/I$sdk/include" "$PSScriptRoot/ExtractScene.cpp" "/Fe$exe" "/Fo$out/Tools/ExtractScene.obj" /link "$sdk/lib/vs2017/x64/release/libfbxsdk.lib"
if($LASTEXITCODE -ne 0) { throw 'FBX helper compilation failed.' }
$oldPath=$env:PATH
try {
    $env:PATH=(Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/FBX/2020.2/Win64')+';'+$oldPath
    & $exe "$project/Content/Maps/map3.fbx" "$out/SceneNodes.json" "$PSScriptRoot/NewEnvironment.txt" "$out/NewEnvironment.fbx"
    if($LASTEXITCODE -ne 0) { throw 'FBX extraction failed.' }
} finally { $env:PATH=$oldPath }
