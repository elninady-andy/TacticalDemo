#include "SceneLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
namespace
{
    TSharedPtr<FJsonObject> ReadScenes()
    {
        FString Text; TSharedPtr<FJsonObject> Root; const TSharedPtr<FJsonObject>* Scenes=nullptr;
        if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectConfigDir()/TEXT("SceneLibrary.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid() || !Root->TryGetObjectField(TEXT("scenes"),Scenes)) return nullptr;
        return *Scenes;
    }
}
bool FDemoSceneLibrary::IsSwitchRequest(const FString& User)
{
    return (User.Contains(TEXT("场景")) || User.Contains(TEXT("地图")) || User.Contains(TEXT("机场"))) &&
        (User.Contains(TEXT("切换")) || User.Contains(TEXT("加载")) || User.Contains(TEXT("换成")) || User.Contains(TEXT("使用"))) &&
        !User.Contains(TEXT("飞机")) && !User.Contains(TEXT("车辆"));
}
FString FDemoSceneLibrary::Catalog()
{
    auto Scenes=ReadScenes(); if(!Scenes) return FString();
    TArray<FString> Entries;
    for(const auto& Pair:Scenes->Values)
    {
        FString Name;
        if(Pair.Value->Type==EJson::Object && Pair.Value->AsObject()->TryGetStringField(TEXT("name"),Name)) Entries.Add(Pair.Key+TEXT("=")+Name);
    }
    Entries.Sort(); return FString::Join(Entries,TEXT("、"));
}
bool FDemoSceneLibrary::Resolve(const FString& Id,FString& Asset,FString& Error)
{
    Asset.Reset(); auto Scenes=ReadScenes();
    const auto Value=Scenes?Scenes->Values.Find(Id):nullptr;
    if(!Value || (*Value)->Type!=EJson::Object || !(*Value)->AsObject()->TryGetStringField(TEXT("asset"),Asset) || !Asset.StartsWith(TEXT("/Game/")) || !FPackageName::DoesPackageExist(Asset))
    { Asset.Reset(); Error=TEXT("场景未登记或资源不存在：")+Id; return false; }
    Error.Reset(); return true;
}
FString FDemoSceneLibrary::ScenarioOverride(const FString& MapAsset)
{
    auto Scenes=ReadScenes(); if(!Scenes) return FString();
    for(const auto& Pair:Scenes->Values)
    {
        if(Pair.Value->Type!=EJson::Object) continue;
        FString Asset,File; const auto Scene=Pair.Value->AsObject();
        if(Scene->TryGetStringField(TEXT("asset"),Asset) && Asset==MapAsset && Scene->TryGetStringField(TEXT("scenario"),File)
            && !File.IsEmpty() && FPaths::IsRelative(File) && FPaths::GetCleanFilename(File)==File
            && !File.Contains(TEXT("\\")) && !File.Contains(TEXT(":")) && File.EndsWith(TEXT(".json"))) return File;
    }
    return FString();
}
