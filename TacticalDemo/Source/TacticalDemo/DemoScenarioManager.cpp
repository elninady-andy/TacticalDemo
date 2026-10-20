#include "DemoScenarioManager.h"
#include "FlightPreview.h"
#include "SceneLibrary.h"
#include "FormationCommands.h"
#include "DemoUnitActor.h"
#include "DemoGameMode.h"
#include "ScenarioConfigLoader.h"
#include "EquipmentLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"

namespace
{
    FAutoConsoleCommandWithWorld ReloadCommand(TEXT("Demo.Reload"), TEXT("Reload Scenario.json and EquipmentLibrary.json"),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
        {
            if (World) for (TActorIterator<ADemoScenarioManager> It(World); It; ++It) It->ReloadScenario();
        }));
    FAutoConsoleCommandWithWorld ClearCommand(TEXT("Demo.Clear"), TEXT("Clear generated units"),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
        {
            if (World) for (TActorIterator<ADemoScenarioManager> It(World); It; ++It) It->ClearScenario();
        }));
}

ADemoUnitActor* ADemoScenarioManager::FindUnit(const FString& Id) const
{ for(ADemoUnitActor* U:Units) if(IsValid(U) && U->UnitId==Id) return U; return nullptr; }
bool ADemoScenarioManager::EditUnit(const FString& Id,const FVector& Position,double Yaw)
{
    if(!FindUnit(Id)) { LastError=TEXT("单位不存在，请重新选择。"); return false; }
    if(Position.ContainsNaN() || Position.GetAbsMax()>1000000 || !FMath::IsFinite(Yaw) || FMath::Abs(Yaw)>360000)
    { LastError=TEXT("位置或朝向无效。"); return false; }
    FDemoScenarioConfig Next;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Next,LastError)) return false;
    for(auto& Group:Next.Units) for(auto& Instance:Group.Instances) if(Instance.Id==Id)
    { Instance.PositionMeters=Position; Instance.Yaw=FRotator::NormalizeAxis(Yaw); }
    return CommitEdit(Next);
}
bool ADemoScenarioManager::SetFormationMembers(const FString& Name,const TArray<FString>& MemberIds)
{
    const FString Clean=Name.TrimStartAndEnd();
    if(Clean.IsEmpty() || Clean.Len()>32 || Clean.Contains(TEXT("\n")) || Clean.Contains(TEXT("\r")))
    { LastError=TEXT("编队名称需要1至32个字符，不能包含换行。"); return false; }
    TSet<FString> Selected;
    for(const auto& Id:MemberIds)
    {
        if(!FindUnit(Id) || Selected.Contains(Id)) { LastError=TEXT("编队成员不存在或重复。"); return false; }
        Selected.Add(Id);
    }
    FDemoScenarioConfig Next;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Next,LastError)) return false;
    for(auto& Group:Next.Units) for(auto& Instance:Group.Instances)
    {
        if(Selected.Contains(Instance.Id)) Instance.Formation=Clean;
        else if(Instance.Formation==Clean) Instance.Formation.Reset();
    }
    return CommitEdit(Next);
}
bool ADemoScenarioManager::ArrangeFormation(const FString& Name,int32 Pattern,double Spacing,double Heading,const FVector2D& Anchor)
{
    FDemoScenarioConfig Next;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Next,LastError)) return false;
    if(!FFormationCommands::Arrange(Next,Name,Pattern,Spacing,Heading,Anchor,LastError)) return false;
    return CommitEdit(Next);
}
bool ADemoScenarioManager::ApplyFormationCandidate(const FString& Json,bool bMembershipOnly)
{
    FDemoScenarioConfig Before,Next;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Before,LastError) || !FScenarioConfigLoader::Parse(Json,Next,LastError)) return false;
    auto Shape=Next;
    if(Shape.Units.Num()!=Before.Units.Num()) { LastError=TEXT("编队候选不能新增或删除单位。"); return false; }
    for(int32 G=0;G<Shape.Units.Num();++G)
    {
        if(Shape.Units[G].Instances.Num()!=Before.Units[G].Instances.Num()) { LastError=TEXT("编队候选的成员数据不匹配。"); return false; }
        if(!bMembershipOnly) Shape.Units[G].PositionMeters=Before.Units[G].PositionMeters;
        for(int32 I=0;I<Shape.Units[G].Instances.Num();++I)
        {
            if(bMembershipOnly) Shape.Units[G].Instances[I].Formation=Before.Units[G].Instances[I].Formation;
            else
            {
                Shape.Units[G].Instances[I].PositionMeters=Before.Units[G].Instances[I].PositionMeters;
                Shape.Units[G].Instances[I].Yaw=Before.Units[G].Instances[I].Yaw;
            }
        }
    }
    if(FScenarioConfigLoader::Serialize(Shape)!=FScenarioConfigLoader::Serialize(Before))
    { LastError=bMembershipOnly?TEXT("成员管理候选只能改变编队归属，不能改变飞机、位置、朝向或地图。")
        :TEXT("编队排列候选只能改变位置与朝向，不能改变型号、贴图、成员或地图。"); return false; }
    return CommitEdit(Next);
}
bool ADemoScenarioManager::PrepareHighlight(const FDemoScenarioConfig& Config)
{
    for(const auto& G:Config.Units) for(const auto& I:G.Instances) if(I.bHighlighted)
    {
        auto* Material=LoadObject<UMaterialInterface>(nullptr,*HighlightMaterialPath,nullptr,LOAD_NoWarn);
        if(!Material) { LastError=TEXT("飞机高亮材质缺失，未应用修改：")+HighlightMaterialPath; return false; }
        HighlightMaterial=Material;
        return true;
    }
    return true;
}
bool ADemoScenarioManager::ApplyHighlightCandidate(const FString& Json)
{
    FDemoScenarioConfig Before,Next;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Before,LastError) || !FScenarioConfigLoader::Parse(Json,Next,LastError)) return false;
    auto Shape=Next;
    if(Shape.Units.Num()!=Before.Units.Num()) { LastError=TEXT("高亮候选不能新增或删除飞机。"); return false; }
    for(int32 G=0;G<Shape.Units.Num();++G)
    {
        if(Shape.Units[G].Instances.Num()!=Before.Units[G].Instances.Num()) { LastError=TEXT("高亮候选的飞机数据不匹配。"); return false; }
        for(int32 I=0;I<Shape.Units[G].Instances.Num();++I) Shape.Units[G].Instances[I].bHighlighted=Before.Units[G].Instances[I].bHighlighted;
    }
    if(FScenarioConfigLoader::Serialize(Shape)!=FScenarioConfigLoader::Serialize(Before))
    { LastError=TEXT("高亮候选只能开关发光，不能改变飞机型号、位置、编队或地图。"); return false; }
    return CommitEdit(Next);
}
bool ADemoScenarioManager::CommitEdit(const FDemoScenarioConfig& Config)
{
    auto Next=Config;
    for(auto& Group:Next.Units) if(Group.Instances.Num()) Group.PositionMeters=Group.Instances[0].PositionMeters;
    FDemoScenarioConfig Checked;
    if(!FScenarioConfigLoader::Parse(FScenarioConfigLoader::Serialize(Next),Checked,LastError)) return false;
    if(!PrepareHighlight(Checked)) return false;
    FDemoScenarioConfig Before;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Before,LastError)) return false;
    if(FScenarioConfigLoader::Serialize(Before)==FScenarioConfigLoader::Serialize(Checked)) { LastError.Reset(); return true; }
    UndoStack.Add({Before,Checked}); if(UndoStack.Num()>100) UndoStack.RemoveAt(0); RedoStack.Reset();
    RestoreEdit(Checked); return true;
}
void ADemoScenarioManager::RestoreEdit(const FDemoScenarioConfig& Config)
{
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) It->Stop();
    ScenarioConfig=Config;
    for(const auto& Group:Config.Units) for(const auto& Instance:Group.Instances) if(auto* Actor=FindUnit(Instance.Id))
    {
        Actor->SetActorLocationAndRotation(Instance.PositionMeters*100.,FRotator(0,Instance.Yaw,0));
        Actor->Visual->SetOverlayMaterial(Instance.bHighlighted?HighlightMaterial.Get():nullptr);
    }
    ScenarioJson=FScenarioConfigLoader::Serialize(ScenarioConfig); LastError.Reset(); ++Revision;
    RefreshUnitHighlights();
}
bool ADemoScenarioManager::UndoEdit()
{
    if(UndoStack.IsEmpty()) return false;
    const auto Edit=UndoStack.Pop(); RestoreEdit(Edit.Before); RedoStack.Add(Edit); return true;
}
bool ADemoScenarioManager::RedoEdit()
{
    if(RedoStack.IsEmpty()) return false;
    const auto Edit=RedoStack.Pop(); RestoreEdit(Edit.After); UndoStack.Add(Edit); return true;
}
FString ADemoScenarioManager::CurrentMapAsset() const
{
    FString Name=GetWorld()?GetWorld()->GetOutermost()->GetName():FString();
    if(GetWorld() && !GetWorld()->StreamingLevelsPrefix.IsEmpty()) Name.ReplaceInline(*GetWorld()->StreamingLevelsPrefix,TEXT(""));
    return Name.StartsWith(TEXT("/Game/"))?Name:FString();
}
FString ADemoScenarioManager::SnapshotJson() const
{
    auto Config=ScenarioConfig; if(!CurrentMapAsset().IsEmpty()) Config.MapAsset=CurrentMapAsset(); Config.SchemaVersion=2;
    int32 Index=0;
    for(auto& Group:Config.Units)
    {
        // Preview transforms are deliberately excluded: the committed config is authoritative.
        if(Group.Instances.Num()!=Group.Count)
        {
            Group.Instances.Reset();
            for(int32 I=0;I<Group.Count;++I)
                Group.Instances.Add({Units[Index+I]->UnitId,Group.PositionMeters+FVector(0,I*Group.SpacingMeters,0),0});
        }
        Index+=Group.Count;
    }
    return FScenarioConfigLoader::Serialize(Config);
}

ADemoScenarioManager::ADemoScenarioManager()
{
    RouteLines = CreateDefaultSubobject<ULineBatchComponent>(TEXT("StaticRouteLines"));
    SetRootComponent(RouteLines);
    RouteLines->SetMobility(EComponentMobility::Static);
    RouteLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RouteLines->SetCanEverAffectNavigation(false);
}

void ADemoScenarioManager::BeginPlay()
{
    Super::BeginPlay();
    ReloadScenario();
}
void ADemoScenarioManager::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearScenario();
    Super::EndPlay(Reason);
}
bool ADemoScenarioManager::ReloadScenario()
{
    const bool Airport=GetWorld() && GetWorld()->GetMapName().Contains(TEXT("VRAirportMap"));
    FString Scenario=FDemoSceneLibrary::ScenarioOverride(CurrentMapAsset());
    if(Scenario.IsEmpty()) Scenario=Airport?TEXT("AirportScenario.json"):TEXT("Scenario.json");
    return LoadScenario(FPaths::ProjectConfigDir()/Scenario, FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"));
}
void ADemoScenarioManager::ClearScenario()
{
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) It->Stop();
    for (ADemoUnitActor* Unit : Units) if (IsValid(Unit)) Unit->Destroy();
    Units.Reset();
    SelectionHighlightIds.Reset();
    ScenarioName.Reset();
    ScenarioConfig = FDemoScenarioConfig();
    UndoStack.Reset(); RedoStack.Reset(); ++Revision;
    if (RouteLines) RouteLines->Flush();
    ScenarioJson=TEXT("{\"scenarioName\":\"Empty\",\"units\":[]}");
    LastError.Reset();
    if (GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(100);
        GEngine->RemoveOnScreenDebugMessage(101);
    }
    UE_LOG(LogTemp, Display, TEXT("[TacticalDemo] Scene cleared"));
}

bool ADemoScenarioManager::LoadScenario(const FString& ScenarioPath, const FString& LibraryPath)
{
    return LoadScenarioInternal(ScenarioPath, LibraryPath, false);
}

bool ADemoScenarioManager::LoadChatCandidate(const FString& ScenarioPath, const FString& LibraryPath)
{
    return LoadScenarioInternal(ScenarioPath, LibraryPath, true);
}

bool ADemoScenarioManager::LoadScenarioInternal(const FString& ScenarioPath, const FString& LibraryPath, bool bRequireExactModels)
{
    FString Json;
    if(!FFileHelper::LoadFileToString(Json,*ScenarioPath)) { LastError=TEXT("Cannot read scenario: ")+ScenarioPath; return false; }
    return LoadJson(Json,LibraryPath,bRequireExactModels);
}

bool ADemoScenarioManager::LoadJson(const FString& SourceJson, const FString& LibraryPath, bool bRequireExactModels)
{
    LastError.Reset();
    auto Fail = [&](const FString& Error)
    {
        LastError = Error;
        UE_LOG(LogTemp, Warning, TEXT("[TacticalDemo] Reload rejected; previous scene preserved: %s"), *Error);
        if (GEngine) GEngine->AddOnScreenDebugMessage(100, 20.f, FColor::Orange, TEXT("Reload failed: ") + Error);
        return false;
    };
    FDemoScenarioConfig Config;
    FString Error;
    FEquipmentLibrary Library;
    if (!FScenarioConfigLoader::Parse(SourceJson, Config, Error) || !Library.Load(LibraryPath, Error)) return Fail(Error);
    if(!PrepareHighlight(Config)) return Fail(LastError);
    if(!Config.MapAsset.IsEmpty() && !CurrentMapAsset().IsEmpty() && Config.MapAsset!=CurrentMapAsset())
        return Fail(TEXT("方案属于其他地图，请从方案管理窗口加载并切换地图。"));
    UnitMaterial = LoadObject<UMaterialInterface>(nullptr, *UnitMaterialPath, nullptr, LOAD_NoWarn);
    if (!UnitMaterial) return Fail(TEXT("Unit material missing; run Scripts/CreateAssets.ps1. Path: ") + UnitMaterialPath);
    TArray<UStaticMesh*> Resolved;
    TArray<FString> ResolvedModelIds;
    struct FTextureBinding { UTexture* Texture=nullptr; UMaterialInterface* Material=nullptr; int32 Slot=0; };
    TArray<FTextureBinding> TextureBindings;
    for (const FDemoUnitConfig& Group : Config.Units)
    {
        FString ActualModelId;
        UStaticMesh* Mesh = Library.Resolve(Group.ModelId, Group.Type, Group.Side, Error, bRequireExactModels,&ActualModelId);
        if (!Mesh) return Fail(Error);
        FTextureBinding Binding;
        if(!Library.ResolveTexture(Group.ModelId,Group.Type,Group.Side,Group.TextureId,Binding.Texture,Binding.Material,Binding.Slot,Error)) return Fail(Error);
        if(Binding.Texture && Binding.Slot>=Mesh->GetStaticMaterials().Num()) return Fail(TEXT("Texture material slot is outside model mesh: ")+Group.ModelId);
        Resolved.Add(Mesh);
        ResolvedModelIds.Add(ActualModelId);
        TextureBindings.Add(Binding);
    }
    // Library keeps strong references until each candidate Actor owns its mesh.
    TArray<TObjectPtr<ADemoUnitActor>> Candidates;
    for (int32 GroupIndex=0; GroupIndex<Config.Units.Num(); ++GroupIndex)
    {
        const FDemoUnitConfig& Group = Config.Units[GroupIndex];
        for (int32 Index=0; Index<Group.Count; ++Index)
        {
            const bool Independent=Group.Instances.Num()==Group.Count;
            const FVector Position = (Independent?Group.Instances[Index].PositionMeters:Group.PositionMeters + FVector(0, Index*Group.SpacingMeters, 0))*100.0;
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ADemoUnitActor* Unit = GetWorld()->SpawnActor<ADemoUnitActor>(Position, FRotator(0,Independent?Group.Instances[Index].Yaw:0,0), Params);
            if (!Unit)
            {
                for (ADemoUnitActor* Candidate : Candidates) Candidate->Destroy();
                return Fail(TEXT("Failed to spawn unit: ") + Group.Id);
            }
            Unit->SetActorHiddenInGame(true);
            const FEquipmentDefinition* Presentation=Library.Presentation(Group.ModelId,Group.Type,Group.Side);
            const FTextureBinding& Binding=TextureBindings[GroupIndex];
            Unit->Configure(Independent?Group.Instances[Index].Id:FString::Printf(TEXT("%s_%03d"), *Group.Id, Index+1), Group.Side, Resolved[GroupIndex], UnitMaterial, Library.ColorForSide(Group.Side),Presentation?Presentation->Scale:2.5,Presentation && Presentation->PreserveMaterials,Binding.Texture,Binding.Material,Binding.Slot);
            if(Independent && Group.Instances[Index].bHighlighted) Unit->Visual->SetOverlayMaterial(HighlightMaterial);
            Candidates.Add(Unit);
            Unit->ResolvedModelId=ResolvedModelIds[GroupIndex];
        }
    }
    ClearScenario();
    Units = MoveTemp(Candidates);
    ScenarioName = Config.Name;
    if(!CurrentMapAsset().IsEmpty()) Config.MapAsset=CurrentMapAsset();
    ScenarioJson = FScenarioConfigLoader::Serialize(Config);
    ScenarioConfig = MoveTemp(Config);
    for (const FDemoUnitConfig& Group : ScenarioConfig.Units)
    {
        if (Group.RouteMeters.Num() < 2) continue;
        const FLinearColor RouteColor = Library.ColorForSide(Group.Side);
        const uint32 BatchId = GetTypeHash(Group.Id);
        for (int32 Index = 1; Index < Group.RouteMeters.Num(); ++Index)
        {
            const FVector Start = Group.RouteMeters[Index - 1] * 100.0 + FVector(0, 0, 25.0);
            const FVector End = Group.RouteMeters[Index] * 100.0 + FVector(0, 0, 25.0);
            RouteLines->DrawLine(Start, End, RouteColor, SDPG_World, 4.0f, 0.0f, BatchId);
        }
        for (const FVector& Waypoint : Group.RouteMeters)
            RouteLines->DrawPoint(Waypoint * 100.0 + FVector(0, 0, 25.0), RouteColor, 18.0f, SDPG_World, 0.0f, BatchId);
    }
    for (ADemoUnitActor* Unit : Units) Unit->SetActorHiddenInGame(false);
    UE_LOG(LogTemp, Display, TEXT("[TacticalDemo] SCENE_LOADED name=%s count=%d"), *ScenarioName, Units.Num());
    if (GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(100);
        GEngine->RemoveOnScreenDebugMessage(101);
    }
    if (ADemoGameMode* Mode = GetWorld()->GetAuthGameMode<ADemoGameMode>()) Mode->FrameScene(this);
    return true;
}
