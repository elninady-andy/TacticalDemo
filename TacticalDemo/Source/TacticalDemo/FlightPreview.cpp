#include "FlightPreview.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FDemoFlightPath::ParseJson(const FString& Json,FString& MapAsset,TArray<FDemoFlightPath>& OutPaths,FString& Error)
{
    auto Fail=[&](const FString& Why){ Error=Why; return false; };
    TSharedPtr<FJsonObject> Root;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root) || !Root.IsValid()) return Fail(TEXT("轨迹文件不是有效 JSON。"));
    FString Map;
    if(!Root->TryGetStringField(TEXT("mapAsset"),Map) || !Map.StartsWith(TEXT("/Game/"))) return Fail(TEXT("轨迹文件缺少有效地图路径。"));
    const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;
    if(!Root->TryGetArrayField(TEXT("paths"),Items) || Items->IsEmpty() || Items->Num()>32) return Fail(TEXT("轨迹文件需要1至32条路径。"));
    TArray<FDemoFlightPath> Parsed; TSet<FString> Models;
    for(const auto& Item:*Items)
    {
        if(!Item.IsValid() || Item->Type!=EJson::Object) return Fail(TEXT("轨迹条目必须是对象。"));
        const auto Object=Item->AsObject(); FDemoFlightPath Path; bool Closed=false;
        if(!Object->TryGetStringField(TEXT("modelId"),Path.ModelId) || Path.ModelId.IsEmpty() || Models.Contains(Path.ModelId)) return Fail(TEXT("轨迹型号为空或重复。"));
        if(!Object->TryGetBoolField(TEXT("closed"),Closed) || !Closed) return Fail(TEXT("当前预览需要明确闭合的路径。"));
        if(!Object->TryGetNumberField(TEXT("periodSeconds"),Path.PeriodSeconds) || !FMath::IsFinite(Path.PeriodSeconds) || Path.PeriodSeconds<=0 || Path.PeriodSeconds>86400) return Fail(TEXT("演示周期必须大于0且不超过86400秒。"));
        if(Object->HasField(TEXT("yawOffsetDegrees")) && (!Object->TryGetNumberField(TEXT("yawOffsetDegrees"),Path.YawOffsetDegrees) || !FMath::IsFinite(Path.YawOffsetDegrees) || FMath::Abs(Path.YawOffsetDegrees)>360)) return Fail(TEXT("机头角度修正必须在正负360度以内。"));
        const TArray<TSharedPtr<FJsonValue>>* Samples=nullptr;
        if(!Object->TryGetArrayField(TEXT("samplesCm"),Samples) || Samples->Num()<4 || Samples->Num()>4097) return Fail(TEXT("闭合轨迹需要4至4097个厘米坐标采样点。"));
        for(const auto& Sample:*Samples)
        {
            const TArray<TSharedPtr<FJsonValue>>* Coordinates=nullptr;
            if(!Sample.IsValid() || !Sample->TryGetArray(Coordinates) || Coordinates->Num()!=3) return Fail(TEXT("轨迹采样点必须包含三个坐标。"));
            FVector Point;
            for(int32 Axis=0;Axis<3;++Axis)
                if(!(*Coordinates)[Axis].IsValid() || !(*Coordinates)[Axis]->TryGetNumber(Point[Axis]) || !FMath::IsFinite(Point[Axis]) || FMath::Abs(Point[Axis])>100000000) return Fail(TEXT("轨迹坐标无效或越界。"));
            if(!Path.SamplesCm.IsEmpty() && FVector::DistSquared(Path.SamplesCm.Last(),Point)<.0001) return Fail(TEXT("轨迹包含重复相邻采样点。"));
            Path.SamplesCm.Add(Point);
        }
        if(!Path.SamplesCm[0].Equals(Path.SamplesCm.Last(),.01)) return Fail(TEXT("轨迹末点必须回到起点。"));
        Path.SamplesCm.Last()=Path.SamplesCm[0];
        Path.DistancesCm.Add(0);
        for(int32 Index=1;Index<Path.SamplesCm.Num();++Index)
            Path.DistancesCm.Add(Path.DistancesCm.Last()+FVector::Distance(Path.SamplesCm[Index-1],Path.SamplesCm[Index]));
        const int32 Unique=Path.SamplesCm.Num()-1;
        for(int32 Index=0;Index<Unique;++Index)
        {
            const FVector Incoming=(Path.SamplesCm[Index]-Path.SamplesCm[(Index+Unique-1)%Unique]).GetSafeNormal();
            const FVector Outgoing=(Path.SamplesCm[(Index+1)%Unique]-Path.SamplesCm[Index]).GetSafeNormal();
            const FVector Smooth=(Incoming+Outgoing).GetSafeNormal();
            Path.Directions.Add(Smooth.IsNearlyZero()?Outgoing:Smooth);
        }
        // TArray rejects Add(reference-to-self) because growth can invalidate it.
        const FVector ClosingDirection=Path.Directions[0];
        Path.Directions.Add(ClosingDirection);
        Models.Add(Path.ModelId); Parsed.Add(MoveTemp(Path));
    }
    MapAsset=Map; OutPaths=MoveTemp(Parsed); Error.Reset(); return true;
}

bool FDemoFlightPath::FromRoute(const TArray<FVector>& PointsMeters,double Period,bool Closed,const FVector& EntryCm,FString& Error)
{
    FDemoFlightRoute Route; Route.PointsMeters=PointsMeters; Route.PeriodSeconds=Period; Route.bClosed=Closed;
    if(!Route.Validate(Error)) return false;
    SamplesCm.Reset(); DistancesCm.Reset(); Directions.Reset(); EntryDistanceCm=0; PeriodSeconds=Period; bClosed=Closed;
    for(const auto& P:Route.PointsMeters) SamplesCm.Add(P*100.);
    if(!EntryCm.Equals(SamplesCm[0],.01))
    { EntryDistanceCm=FVector::Distance(EntryCm,SamplesCm[0]); SamplesCm.Insert(EntryCm,0); }
    DistancesCm.Add(0);
    for(int32 I=1;I<SamplesCm.Num();++I) DistancesCm.Add(DistancesCm.Last()+FVector::Distance(SamplesCm[I-1],SamplesCm[I]));
    for(int32 I=0;I<SamplesCm.Num();++I)
    {
        const FVector Incoming=I>0?(SamplesCm[I]-SamplesCm[I-1]).GetSafeNormal():(SamplesCm[1]-SamplesCm[0]).GetSafeNormal();
        const FVector Outgoing=I+1<SamplesCm.Num()?(SamplesCm[I+1]-SamplesCm[I]).GetSafeNormal():Incoming;
        const FVector Smooth=(Incoming+Outgoing).GetSafeNormal(); Directions.Add(Smooth.IsNearlyZero()?Outgoing:Smooth);
    }
    if(bClosed)
    {
        const int32 Start=EntryDistanceCm>0?1:0;
        const FVector Incoming=(SamplesCm.Last()-SamplesCm[SamplesCm.Num()-2]).GetSafeNormal();
        const FVector Outgoing=(SamplesCm[Start+1]-SamplesCm[Start]).GetSafeNormal();
        const FVector Smooth=(Incoming+Outgoing).GetSafeNormal();
        Directions[Start]=Smooth.IsNearlyZero()?Outgoing:Smooth; Directions.Last()=Directions[Start];
    }
    return true;
}

void FDemoFlightPath::Evaluate(double Seconds,FVector& Position,FRotator& Heading) const
{
    Position=FVector::ZeroVector; Heading=FRotator::ZeroRotator;
    if(SamplesCm.Num()<2 || DistancesCm.Num()!=SamplesCm.Num() || Directions.Num()!=SamplesCm.Num() || PeriodSeconds<=0 || !FMath::IsFinite(Seconds)) return;
    double Distance=FMath::Clamp(Seconds/PeriodSeconds,0.,1.)*DistancesCm.Last();
    if(bClosed)
    {
        const double LoopLength=DistancesCm.Last()-EntryDistanceCm;
        if(LoopLength<=0) return;
        const double EntrySeconds=EntryDistanceCm/LoopLength*PeriodSeconds;
        if(Seconds<EntrySeconds) Distance=EntrySeconds>0?FMath::Clamp(Seconds/EntrySeconds,0.,1.)*EntryDistanceCm:0;
        else Distance=EntryDistanceCm+FMath::Fmod(Seconds-EntrySeconds,PeriodSeconds)/PeriodSeconds*LoopLength;
    }
    int32 Index=0;
    while(Index+2<DistancesCm.Num() && DistancesCm[Index+1]<=Distance) ++Index;
    const double Segment=DistancesCm[Index+1]-DistancesCm[Index];
    const double Alpha=Segment>0?FMath::Clamp((Distance-DistancesCm[Index])/Segment,0.,1.):0.;
    Position=FMath::Lerp(SamplesCm[Index],SamplesCm[Index+1],Alpha);
    FVector Direction=FMath::Lerp(Directions[Index],Directions[Index+1],Alpha).GetSafeNormal();
    if(Direction.IsNearlyZero()) Direction=(SamplesCm[Index+1]-SamplesCm[Index]).GetSafeNormal();
    Heading=Direction.Rotation();
}

ADemoFlightPreview::ADemoFlightPreview()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot")));
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bStartWithTickEnabled=false;
}

bool ADemoFlightPreview::Start(ADemoScenarioManager* Manager,const FString& PathsFile)
{
    if(bPlaying) return SourceManager.Get()==Manager;
    LastError.Reset();
    if(!IsValid(Manager) || Manager->GetWorld()!=GetWorld()) { LastError=TEXT("没有可预览的场景。"); return false; }
    FString Json,Map; TArray<FDemoFlightPath> Loaded;
    const FString File=PathsFile.IsEmpty()?FPaths::ProjectConfigDir()/TEXT("Map3FlightPaths.json"):PathsFile;
    FString SourceError;
    if(FFileHelper::LoadFileToString(Json,*File))
    {
        if(!FDemoFlightPath::ParseJson(Json,Map,Loaded,SourceError) || Manager->CurrentMapAsset()!=Map) Loaded.Reset();
    }
    TArray<FBinding> Pending;
    for(const auto& Group:Manager->GetScenarioConfig().Units)
    {
        const int32 DefaultPathIndex=Loaded.IndexOfByPredicate([&](const FDemoFlightPath& Path){return Path.ModelId==Group.ModelId;});
        if(Group.Type!=TEXT("aircraft") || Group.Count==0) continue;
        TArray<ADemoUnitActor*> Members;
        if(Group.Instances.Num()==Group.Count)
        {
            for(const auto& Instance:Group.Instances) Members.Add(Manager->FindUnit(Instance.Id));
        }
        else
        {
            for(int32 Index=0;Index<Group.Count;++Index) Members.Add(Manager->FindUnit(FString::Printf(TEXT("%s_%03d"),*Group.Id,Index+1)));
        }
        for(auto* Unit:Members)
            if(!IsValid(Unit) || !IsValid(Unit->Visual) || !Unit->Visual->GetStaticMesh()) { LastError=TEXT("编队单位或飞机网格已失效。"); return false; }
        const FVector Anchor=Members[0]->GetActorLocation(); const double LeadYaw=Members[0]->GetActorRotation().Yaw;
        for(int32 MemberIndex=0;MemberIndex<Members.Num();++MemberIndex)
        {
            auto* Unit=Members[MemberIndex];
            const FDemoFlightRoute* Custom=Group.Instances.IsValidIndex(MemberIndex)?&Group.Instances[MemberIndex].FlightRoute:nullptr;
            const bool bHasCustom=Custom && !Custom->PointsMeters.IsEmpty();
            const bool bWorldRoute=bHasCustom || !Group.RouteMeters.IsEmpty();
            int32 PathIndex=DefaultPathIndex;
            if(bWorldRoute)
            {
                FDemoFlightPath WorldPath;
                const auto& Points=bHasCustom?Custom->PointsMeters:Group.RouteMeters;
                if(!WorldPath.FromRoute(Points,bHasCustom?Custom->PeriodSeconds:40,bHasCustom?Custom->bClosed:false,Unit->GetActorLocation(),LastError)) return false;
                WorldPath.YawOffsetDegrees=DefaultPathIndex==INDEX_NONE?0:Loaded[DefaultPathIndex].YawOffsetDegrees;
                PathIndex=Loaded.Add(MoveTemp(WorldPath));
            }
            if(PathIndex==INDEX_NONE) continue;
            FBinding Binding; Binding.UnitId=Unit->UnitId; Binding.GroupId=Group.Id; Binding.Actor=Unit; Binding.Original=Unit->Visual; Binding.Mesh=Unit->Visual->GetStaticMesh();
            Binding.PathIndex=PathIndex; Binding.Anchor=bWorldRoute?Unit->GetActorLocation():Anchor; Binding.Offset=bWorldRoute?FVector::ZeroVector:Unit->GetActorLocation()-Anchor;
            Binding.RelativeYaw=bWorldRoute?0:FRotator::NormalizeAxis(Unit->GetActorRotation().Yaw-LeadYaw); Binding.bWasVisible=Unit->Visual->IsVisible();
            Pending.Add(MoveTemp(Binding));
        }
    }
    if(Pending.IsEmpty()) { LastError=TEXT("当前飞机没有可预览航迹，请先在航迹页应用航点。"); return false; }
    Paths=MoveTemp(Loaded); Bindings=MoveTemp(Pending); SourceManager=Manager; SourceRevision=Manager->GetRevision(); ElapsedSeconds=0;
    for(const auto& Binding:Bindings)
    {
        auto* Original=Binding.Original.Get();
        auto* Clone=NewObject<UStaticMeshComponent>(this,NAME_None,RF_Transient);
        Clone->SetupAttachment(GetRootComponent()); Clone->SetMobility(EComponentMobility::Movable);
        Clone->SetCollisionEnabled(ECollisionEnabled::NoCollision); Clone->SetCanEverAffectNavigation(false);
        Clone->SetStaticMesh(Original->GetStaticMesh());
        Clone->SetOverlayMaterial(Original->GetOverlayMaterial());
        for(int32 Index=0;Index<Original->GetNumMaterials();++Index) Clone->SetMaterial(Index,Original->GetMaterial(Index));
        Clone->SetVisibility(Binding.bWasVisible); Clone->SetHiddenInGame(Original->bHiddenInGame);
        Clone->RegisterComponent(); Clone->SetWorldTransform(Original->GetComponentTransform());
        PreviewVisuals.Add(Clone);
    }
    for(const auto& Binding:Bindings) Binding.Original->SetVisibility(false);
    bPlaying=true; SetActorTickEnabled(true); AdvancePreview(0); return bPlaying;
}

void ADemoFlightPreview::Stop()
{
    SetActorTickEnabled(false); bPlaying=false;
    for(const auto& Binding:Bindings) if(auto* Original=Binding.Original.Get()) Original->SetVisibility(Binding.bWasVisible);
    for(UStaticMeshComponent* Clone:PreviewVisuals) if(IsValid(Clone)) Clone->DestroyComponent();
    PreviewVisuals.Reset(); Bindings.Reset(); Paths.Reset(); SourceManager.Reset(); ElapsedSeconds=0;
}

void ADemoFlightPreview::AdvancePreview(double DeltaSeconds)
{
    if(!bPlaying) return;
    auto* Manager=SourceManager.Get();
    if(!Manager || Manager->GetRevision()!=SourceRevision || Bindings.Num()!=PreviewVisuals.Num()) { Stop(); return; }
    // Validate all sources before reading cloned geometry or applying any movement.
    for(int32 Index=0;Index<Bindings.Num();++Index)
    {
        const auto& Binding=Bindings[Index]; auto* Original=Binding.Original.Get(); auto* Unit=Binding.Actor.Get();
        if(!Unit || !Original || !Binding.Mesh.IsValid() || Unit->Visual!=Original || Manager->FindUnit(Binding.UnitId)!=Unit || Original->GetStaticMesh()!=Binding.Mesh.Get() || !IsValid(PreviewVisuals[Index]) || !Paths.IsValidIndex(Binding.PathIndex)) { Stop(); return; }
    }
    if(!FMath::IsFinite(DeltaSeconds) || DeltaSeconds<0) return;
    ElapsedSeconds+=DeltaSeconds;
    if(!FMath::IsFinite(ElapsedSeconds)) { Stop(); return; }
    for(int32 Index=0;Index<Bindings.Num();++Index)
    {
        const auto& Binding=Bindings[Index]; const auto& Path=Paths[Binding.PathIndex];
        if(PreviewVisuals[Index]->GetOverlayMaterial()!=Binding.Original->GetOverlayMaterial())
            PreviewVisuals[Index]->SetOverlayMaterial(Binding.Original->GetOverlayMaterial());
        FVector Position,Start; FRotator Heading,StartHeading;
        Path.Evaluate(ElapsedSeconds,Position,Heading); Path.Evaluate(0,Start,StartHeading);
        const FRotator OffsetRotation(0,FRotator::NormalizeAxis(Heading.Yaw-StartHeading.Yaw),0);
        const FVector Location=Binding.Anchor+Position-Start+OffsetRotation.RotateVector(Binding.Offset);
        Heading.Yaw+=Path.YawOffsetDegrees+Binding.RelativeYaw;
        PreviewVisuals[Index]->SetWorldLocationAndRotation(Location,Heading);
    }
}

void ADemoFlightPreview::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); AdvancePreview(DeltaSeconds); }
void ADemoFlightPreview::EndPlay(const EEndPlayReason::Type Reason) { Stop(); Super::EndPlay(Reason); }
FBox ADemoFlightPreview::GetPreviewBounds(const FString& FollowUnitId) const
{
    FBox Bounds(ForceInit);
    const FBinding* Follow=Bindings.FindByPredicate([&](const FBinding& Binding){ return Binding.UnitId==FollowUnitId; });
    if(bPlaying) for(int32 Index=0;Index<PreviewVisuals.Num();++Index)
    {
        auto* Clone=PreviewVisuals[Index].Get();
        if(Follow && Bindings[Index].GroupId!=Follow->GroupId) continue;
        if(IsValid(Clone) && Clone->IsVisible() && !Clone->bHiddenInGame) Bounds+=Clone->Bounds.GetBox();
    }
    return Bounds;
}
FVector ADemoFlightPreview::GetPreviewCenter() const
{
    const FBox Bounds=GetPreviewBounds(); return Bounds.IsValid?Bounds.GetCenter():FVector::ZeroVector;
}
bool ADemoFlightPreview::GetPreviewTransform(const FString& UnitId,FTransform& Transform) const
{
    if(!bPlaying) return false;
    for(int32 Index=0;Index<Bindings.Num();++Index) if(Bindings[Index].UnitId==UnitId && PreviewVisuals.IsValidIndex(Index) && IsValid(PreviewVisuals[Index]))
    { Transform=PreviewVisuals[Index]->GetComponentTransform(); return true; }
    return false;
}
