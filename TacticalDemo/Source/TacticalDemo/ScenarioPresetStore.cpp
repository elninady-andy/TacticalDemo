#include "ScenarioPresetStore.h"
#include "Misc/Paths.h"
#include "ScenarioConfigLoader.h"
#include "EquipmentLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
FScenarioPresetStore::FScenarioPresetStore(const FString& InDirectory):Directory(InDirectory.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Scenarios"):InDirectory) {}
FString FScenarioPresetStore::PathFor(const FString& Id) const
{ FGuid Guid; return FGuid::ParseExact(Id,EGuidFormats::Digits,Guid)?Directory/(Id+TEXT(".json")):FString(); }
TArray<FScenarioPresetEntry> FScenarioPresetStore::List() const
{
    TArray<FScenarioPresetEntry> Entries; TArray<FString> Files;
    IFileManager::Get().FindFiles(Files,*(Directory/TEXT("*.json")),true,false);
    for(const FString& File:Files)
    {
        const FString Id=FPaths::GetBaseFilename(File); FString Json,Error; FDemoScenarioConfig Config;
        if(Read(Id,Json,Error) && FScenarioConfigLoader::Parse(Json,Config,Error))
        {
            FScenarioPresetEntry Entry{Id,Config.Name,Config.MapAsset,FString()};
            TSharedPtr<FJsonObject> Root;
            if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)) Root->TryGetStringField(TEXT("savedUtc"),Entry.CreatedUtc);
            Entries.Add(Entry);
        }
    }
    Entries.Sort([](const auto& A,const auto& B){return A.Name<B.Name;}); return Entries;
}
bool FScenarioPresetStore::Read(const FString& Id,FString& Json,FString& Error) const
{
    const FString Path=PathFor(Id);
    if(Path.IsEmpty() || IFileManager::Get().FileSize(*Path)>2*1024*1024 || !FFileHelper::LoadFileToString(Json,*Path))
    { Error=TEXT("无法读取方案，或文件超过 2 MB。"); return false; }
    FDemoScenarioConfig Config; return FScenarioConfigLoader::Parse(Json,Config,Error);
}
bool FScenarioPresetStore::SaveNew(const FString& Name,const FString& Json,FString& Id,FString& Error)
{
    Id.Reset(); Error.Reset(); const FString CleanName=Name.TrimStartAndEnd();
    if(CleanName.IsEmpty() || CleanName.Len()>80) { Error=TEXT("方案名称需要 1 至 80 个字。"); return false; }
    for(const auto& Entry:List()) if(Entry.Name.Equals(CleanName,ESearchCase::IgnoreCase))
    { Error=TEXT("方案名称已存在，请使用新名称保存。"); return false; }
    FDemoScenarioConfig Config;
    if(Json.Len()>2*1024*1024 || !FScenarioConfigLoader::Parse(Json,Config,Error)) return false;
    FString Map;
    if(!ValidateResources(Json,Map,Error)) return false;
    Config.Name=CleanName;
    TSharedPtr<FJsonObject> Root;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FScenarioConfigLoader::Serialize(Config)),Root);
    Root->SetStringField(TEXT("savedUtc"),FDateTime::UtcNow().ToIso8601());
    FString Output; FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Output));
    const FString NewId=FGuid::NewGuid().ToString(EGuidFormats::Digits),Path=PathFor(NewId),Temp=Path+TEXT(".tmp");
    IFileManager::Get().MakeDirectory(*Directory,true);
    if(!FFileHelper::SaveStringToFile(Output,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !IFileManager::Get().Move(*Path,*Temp,false,true))
    { IFileManager::Get().Delete(*Temp); Error=TEXT("保存失败，请检查目录权限。"); return false; }
    Id=NewId; return true;
}
bool FScenarioPresetStore::Import(const FString& Path,const FString& Name,FString& Id,FString& Error)
{
    FString Json;
    if(!Path.EndsWith(TEXT(".json"),ESearchCase::IgnoreCase) || IFileManager::Get().FileSize(*Path)>2*1024*1024 || !FFileHelper::LoadFileToString(Json,*Path))
    { Error=TEXT("请填写存在且不超过 2 MB 的 JSON 文件。"); return false; }
    return SaveNew(Name,Json,Id,Error);
}
bool FScenarioPresetStore::Export(const FString& Id,const FString& Path,FString& Error) const
{
    FString Json;
    if(!Read(Id,Json,Error)) return false;
    if(!Path.EndsWith(TEXT(".json"),ESearchCase::IgnoreCase) || IFileManager::Get().FileExists(*Path))
    { Error=TEXT("请填写新的 .json 文件路径；不会覆盖已有文件。"); return false; }
    if(!FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { Error=TEXT("导出失败，请确认目标目录存在且可写。"); return false; }
    return true;
}
bool FScenarioPresetStore::ValidateResources(const FString& Json,FString& MapAsset,FString& Error)
{
    FDemoScenarioConfig Config; FEquipmentLibrary Library;
    if(!FScenarioConfigLoader::Parse(Json,Config,Error) || !Library.Load(FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"),Error)) return false;
    MapAsset=Config.MapAsset;
    FString MapFilename;
    if(!MapAsset.IsEmpty() && (!MapAsset.StartsWith(TEXT("/Game/")) || !FPackageName::DoesPackageExist(MapAsset,&MapFilename)
        || !FPaths::GetExtension(MapFilename).Equals(TEXT("umap"),ESearchCase::IgnoreCase)))
    { Error=TEXT("方案地图资源不存在：")+MapAsset; return false; }
    for(const auto& Group:Config.Units)
    {
        auto* Mesh=Library.Resolve(Group.ModelId,Group.Type,Group.Side,Error,true);
        if(!Mesh) return false;
        UTexture* Texture=nullptr; UMaterialInterface* Material=nullptr; int32 Slot=0;
        if(!Library.ResolveTexture(Group.ModelId,Group.Type,Group.Side,Group.TextureId,Texture,Material,Slot,Error)) return false;
        if(Texture && Slot>=Mesh->GetStaticMaterials().Num()) { Error=TEXT("贴图材质槽不存在。"); return false; }
    }
    Error.Reset(); return true;
}
