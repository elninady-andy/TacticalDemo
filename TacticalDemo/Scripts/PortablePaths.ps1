function Read-PortableSettings {
    $bundle = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Join-Path $bundle 'LocalPaths.json'
    if(Test-Path -LiteralPath $file){return Get-Content -LiteralPath $file -Raw | ConvertFrom-Json}
    return [pscustomobject]@{}
}
function Resolve-PortableEngine([string]$Explicit) {
    $settings=Read-PortableSettings
    $candidates=@($Explicit,$env:UE_ENGINE_ROOT,$settings.EngineRoot)
    $registry=Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.3' -ErrorAction SilentlyContinue
    if($registry){$candidates+= $registry.InstalledDirectory}
    $candidates+=Join-Path $env:ProgramFiles 'Epic Games\UE_5.3'
    foreach($candidate in $candidates){
        if($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor.exe'))){return [IO.Path]::GetFullPath($candidate)}
    }
    throw 'UE 5.3 not found. Set EngineRoot in bundle LocalPaths.json or pass -EngineRoot.'
}
function Resolve-PortableQwen([string]$Explicit) {
    $settings=Read-PortableSettings
    if($Explicit){return [IO.Path]::GetFullPath($Explicit)}
    if($env:QWEN_RUNTIME_ROOT){return [IO.Path]::GetFullPath($env:QWEN_RUNTIME_ROOT)}
    if($settings.QwenRuntimeRoot){return [IO.Path]::GetFullPath($settings.QwenRuntimeRoot)}
    return Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'QwenRuntime'
}
