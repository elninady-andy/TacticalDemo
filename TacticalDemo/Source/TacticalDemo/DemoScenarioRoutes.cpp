#include "DemoScenarioManager.h"
#include "ScenarioConfigLoader.h"

bool FDemoFlightRoute::Validate(FString& Error)
{
    auto Fail=[&](const TCHAR* Message){ Error=Message; return false; };
    if(!FMath::IsFinite(PeriodSeconds) || PeriodSeconds<=0 || PeriodSeconds>86400) return Fail(TEXT("航迹时长须大于0且不超过86400秒。"));
    if(PointsMeters.Num()<2 || PointsMeters.Num()>512) return Fail(TEXT("航迹需要2至512个航点。"));
    TArray<FVector> Unique;
    for(int32 Index=0;Index<PointsMeters.Num();++Index)
    {
        const FVector& P=PointsMeters[Index];
        if(P.ContainsNaN() || !FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || !FMath::IsFinite(P.Z) || P.GetAbsMax()>1000000) return Fail(TEXT("航点须为正负100万米内的有限坐标。"));
        if(Index && P.Equals(PointsMeters[Index-1],.0001)) return Fail(TEXT("相邻航点不能重合。"));
        if(!Unique.ContainsByPredicate([&](const FVector& Other){return P.Equals(Other,.0001);})) Unique.Add(P);
    }
    if(bClosed)
    {
        if(Unique.Num()<3) return Fail(TEXT("闭合航迹至少需要三个不同航点。"));
        if(!PointsMeters[0].Equals(PointsMeters.Last(),.0001))
        {
            if(PointsMeters.Num()==512) return Fail(TEXT("闭合点超出512个航点限制。"));
            const FVector First=PointsMeters[0]; PointsMeters.Add(First);
        }
        else PointsMeters.Last()=PointsMeters[0];
    }
    Error.Reset(); return true;
}

bool ADemoScenarioManager::EditRoutes(const TArray<FString>& Ids,const TArray<FVector>& Points,double Period,bool Closed)
{
    if(Ids.IsEmpty()) { LastError=TEXT("请先选择飞机。"); return false; }
    FDemoFlightRoute Route; Route.PointsMeters=Points; Route.PeriodSeconds=Period; Route.bClosed=Closed;
    if(!Route.Validate(LastError)) return false;
    FDemoScenarioConfig Next;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Next,LastError)) return false;
    TMap<FString,FDemoUnitInstance*> Selected;
    for(const auto& Id:Ids)
    {
        FDemoUnitInstance* Found=nullptr;
        for(auto& G:Next.Units) if(G.Type==TEXT("aircraft")) for(auto& I:G.Instances) if(I.Id==Id) Found=&I;
        if(!Found || !FindUnit(Id) || Selected.Contains(Id)) { LastError=TEXT("所选飞机不存在、不是飞机或编号重复。"); return false; }
        Selected.Add(Id,Found);
    }
    const FVector Origin=Selected[Ids[0]]->PositionMeters;
    for(const auto& Id:Ids)
    {
        auto* I=Selected[Id]; I->FlightRoute=Route;
        for(auto& P:I->FlightRoute.PointsMeters) P+=I->PositionMeters-Origin;
        if(!I->FlightRoute.Validate(LastError)) return false;
    }
    return CommitEdit(Next);
}

bool ADemoScenarioManager::ResetRoutes(const TArray<FString>& Ids)
{
    if(Ids.IsEmpty()) { LastError=TEXT("请先选择飞机。"); return false; }
    FDemoScenarioConfig Next;
    if(!FScenarioConfigLoader::Parse(SnapshotJson(),Next,LastError)) return false;
    TSet<FString> Seen;
    for(const auto& Id:Ids)
    {
        FDemoUnitInstance* Found=nullptr;
        for(auto& G:Next.Units) if(G.Type==TEXT("aircraft")) for(auto& I:G.Instances) if(I.Id==Id) Found=&I;
        if(!Found || !FindUnit(Id) || Seen.Contains(Id)) { LastError=TEXT("所选飞机不存在、不是飞机或编号重复。"); return false; }
        Seen.Add(Id); Found->FlightRoute=FDemoFlightRoute();
    }
    return CommitEdit(Next);
}
