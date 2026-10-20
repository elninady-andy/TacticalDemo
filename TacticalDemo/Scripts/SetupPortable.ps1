param([string]$EngineRoot='', [string]$PythonExe='', [switch]$SkipQwen, [switch]$CheckOnly)
$ErrorActionPreference='Stop'
. "$PSScriptRoot\PortablePaths.ps1"
$engine=Resolve-PortableEngine $EngineRoot
$bundle=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$project=Split-Path $PSScriptRoot -Parent
$runtime=Resolve-PortableQwen ''
foreach($relative in @('TacticalDemo.uproject','Content\Maps\Demo.umap','Content\VRAirport\Maps\VRAirportMap.umap','Config\EquipmentLibrary.json')){
    if(!(Test-Path -LiteralPath (Join-Path $project $relative))){throw "Missing project file: $relative"}
}
if(!$SkipQwen -and !(Test-Path -LiteralPath (Join-Path $runtime 'model\config.json'))){throw 'Qwen model files missing.'}
& "$PSScriptRoot/Validate.ps1"
if($CheckOnly){Write-Output "Portable paths verified. UE: $engine; Qwen: $runtime"; return}
$settings=@{EngineRoot=$engine;QwenRuntimeRoot=$runtime}
$settings | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $bundle 'LocalPaths.json') -Encoding UTF8
if(!$SkipQwen){
    if(!$PythonExe){$PythonExe=(Get-Command python.exe -ErrorAction Stop).Source}
    & $PythonExe -c 'import sys; assert sys.version_info[:2] == (3,14), "Install Python 3.14 x64 for the pinned runtime"'
    if($LASTEXITCODE -ne 0){throw 'Python 3.14 x64 required; alternatively configure an existing QwenRuntimeRoot.'}
    & $PythonExe -m venv (Join-Path $runtime 'venv')
    if($LASTEXITCODE -ne 0){throw 'Could not create Qwen Python environment'}
    $venvPython=Join-Path $runtime 'venv\Scripts\python.exe'
    & $venvPython -m pip install -r (Join-Path $runtime 'requirements-lock.txt')
    if($LASTEXITCODE -ne 0){throw 'Dependency installation failed; internet/package index access required on first setup.'}
    & $venvPython -c 'import torch, transformers; print(torch.__version__, transformers.__version__)'
    if($LASTEXITCODE -ne 0){throw 'Qwen dependencies failed to import'}
}
& "$PSScriptRoot\Build.ps1" -EngineRoot $engine
Write-Output 'Setup completed. Open TacticalDemo.uproject or run StartDemo.cmd.'
