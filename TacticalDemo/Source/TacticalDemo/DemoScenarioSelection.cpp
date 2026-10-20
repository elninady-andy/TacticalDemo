#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "ScenarioConfigLoader.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

void ADemoScenarioManager::SetSelectionHighlights(const TArray<FString>& Ids)
{
    SelectionHighlightIds.Reset();
    for(const auto& G:ScenarioConfig.Units) if(G.Type==TEXT("aircraft"))
        for(const auto& Id:Ids) if(FindUnit(Id) && (G.Instances.ContainsByPredicate([&](const FDemoUnitInstance& I){return I.Id==Id;}) || (G.Instances.IsEmpty() && Id.StartsWith(G.Id+TEXT("_"))))) SelectionHighlightIds.Add(Id);
    if(!SelectionHighlightIds.IsEmpty() && !HighlightMaterial)
        HighlightMaterial=LoadObject<UMaterialInterface>(nullptr,*HighlightMaterialPath,nullptr,LOAD_NoWarn);
    RefreshUnitHighlights();
}
void ADemoScenarioManager::RefreshUnitHighlights()
{
    TSet<FString> Lit=SelectionHighlightIds;
    for(const auto& G:ScenarioConfig.Units) for(const auto& I:G.Instances) if(I.bHighlighted) Lit.Add(I.Id);
    for(ADemoUnitActor* U:Units) if(IsValid(U)) U->Visual->SetOverlayMaterial(Lit.Contains(U->UnitId)?HighlightMaterial.Get():nullptr);
}
bool ADemoScenarioManager::EditUnits(const TArray<FString>& Ids,const FString& AnchorId,const FVector& Position,double Yaw)
{
    auto* Anchor=FindUnit(AnchorId);
    if(!Anchor || !Ids.Contains(AnchorId) || Ids.IsEmpty()) { LastError=TEXT("请先选中需要编辑的飞机。"); return false; }
    if(Position.ContainsNaN() || Position.GetAbsMax()>1000000 || !FMath::IsFinite(Yaw) || FMath::Abs(Yaw)>360000)
    { LastError=TEXT("位置或朝向无效。"); return false; }
    TSet<FString> Selected;
    for(const auto& Id:Ids)
    {
        if(!FindUnit(Id) || Selected.Contains(Id)) { LastError=TEXT("选择已失效或重复，请重新选择。"); return false; }
        Selected.Add(Id);
    }
    FDemoScenarioConfig Next;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Next,LastError)) return false;
    const FVector Delta=Position-Anchor->GetActorLocation()/100.;
    const double Turn=FRotator::NormalizeAxis(Yaw-Anchor->GetActorRotation().Yaw);
    for(auto& G:Next.Units) for(auto& I:G.Instances) if(Selected.Contains(I.Id))
    { I.PositionMeters+=Delta; I.Yaw=FRotator::NormalizeAxis(I.Yaw+Turn); }
    return CommitEdit(Next);
}
bool ADemoScenarioManager::CreateSelectedFormation(const FString& Name,const TArray<FString>& Ids)
{
    if(Ids.IsEmpty()) { LastError=TEXT("请先多选飞机，再建立编队。"); return false; }
    const FString Clean=Name.TrimStartAndEnd();
    for(const auto& G:ScenarioConfig.Units)
    {
        for(const auto& I:G.Instances) if(I.Formation==Clean && !Clean.IsEmpty()) { LastError=TEXT("编队名称已存在，请填写新名称；修改已有成员请到编队页。"); return false; }
        if(G.Type!=TEXT("aircraft")) for(const auto& Id:Ids)
            if(G.Instances.ContainsByPredicate([&](const FDemoUnitInstance& I){return I.Id==Id;}) || (G.Instances.IsEmpty() && Id.StartsWith(G.Id+TEXT("_")))) { LastError=TEXT("编队成员必须全部是飞机。"); return false; }
    }
    return SetFormationMembers(Clean,Ids);
}
