#include "ScenarioConfigLoader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

namespace
{
    bool ReadScenarioString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out)
    {
        const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Field);
        return Value && (*Value)->Type == EJson::String && (*Value)->TryGetString(Out) && !Out.TrimStartAndEnd().IsEmpty();
    }
    bool ReadScenarioNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double& Out)
    {
        const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Field);
        return Value && (*Value)->Type == EJson::Number && (*Value)->TryGetNumber(Out) && FMath::IsFinite(Out);
    }
}
bool FScenarioConfigLoader::IsSupportedType(const FString& Type)
{
    return Type == TEXT("aircraft") || Type == TEXT("vehicle") || Type == TEXT("radar") || Type == TEXT("target");
}

bool FScenarioConfigLoader::Parse(const FString& Text, FDemoScenarioConfig& OutConfig, FString& OutError)
{
    OutError.Reset();
    TSharedPtr<FJsonObject> Root;
    const auto Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Invalid JSON: ") + Reader->GetErrorMessage();
        return false;
    }
    FDemoScenarioConfig Candidate;
    if (Root->HasField(TEXT("schemaVersion")))
    {
        double Version;
        if(!ReadScenarioNumber(Root,TEXT("schemaVersion"),Version) || (Version!=1 && Version!=2))
        { OutError=TEXT("Unsupported schemaVersion (expected 1 or 2)"); return false; }
        Candidate.SchemaVersion=static_cast<int32>(Version);
    }
    if(Root->HasField(TEXT("mapAsset")) && !ReadScenarioString(Root,TEXT("mapAsset"),Candidate.MapAsset))
    { OutError=TEXT("mapAsset must be a nonempty string"); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
    if (!ReadScenarioString(Root, TEXT("scenarioName"), Candidate.Name) || !Root->TryGetArrayField(TEXT("units"), Groups))
    {
        OutError = TEXT("scenarioName must be nonempty; units must be an array");
        return false;
    }
    TSet<FString> Ids;
    TSet<FString> InstanceIds;
    int32 Total = 0;
    for (int32 Index = 0; Index < Groups->Num(); ++Index)
    {
        auto Fail = [&](const FString& Reason)
        {
            OutError = FString::Printf(TEXT("units[%d]: %s"), Index, *Reason);
            return false;
        };
        const TSharedPtr<FJsonValue>& Value = (*Groups)[Index];
        if (!Value.IsValid() || Value->Type != EJson::Object) return Fail(TEXT("expected object"));
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        FDemoUnitConfig Unit;
        if (!ReadScenarioString(Object, TEXT("id"), Unit.Id) || !ReadScenarioString(Object, TEXT("modelId"), Unit.ModelId)
            || !ReadScenarioString(Object, TEXT("type"), Unit.Type) || !ReadScenarioString(Object, TEXT("side"), Unit.Side))
            return Fail(TEXT("id/modelId/type/side must be nonempty strings"));
        if (Ids.Contains(Unit.Id)) return Fail(TEXT("duplicate id: ") + Unit.Id);
        if(Object->HasField(TEXT("textureId")) && !ReadScenarioString(Object,TEXT("textureId"),Unit.TextureId))
            return Fail(TEXT("textureId must be a nonempty registered texture string"));
        Ids.Add(Unit.Id);
        if (!IsSupportedType(Unit.Type)) return Fail(TEXT("type must be aircraft/vehicle/radar/target"));
        if (Unit.Side != TEXT("red") && Unit.Side != TEXT("blue")) return Fail(TEXT("side must be red/blue"));
        double Count;
        if (!ReadScenarioNumber(Object, TEXT("count"), Count) || Count < 0 || Count > 100 || FMath::FloorToDouble(Count) != Count)
            return Fail(TEXT("count must be an integer from 0 to 100"));
        Unit.Count = static_cast<int32>(Count);
        Total += Unit.Count;
        if (Total > 500) return Fail(TEXT("total count exceeds 500"));
        const TSharedPtr<FJsonObject>* Position = nullptr;
        if (!Object->TryGetObjectField(TEXT("position"), Position)
            || !ReadScenarioNumber(*Position, TEXT("x"), Unit.PositionMeters.X)
            || !ReadScenarioNumber(*Position, TEXT("y"), Unit.PositionMeters.Y)
            || !ReadScenarioNumber(*Position, TEXT("z"), Unit.PositionMeters.Z))
            return Fail(TEXT("position.x/y/z must be finite numbers (meters)"));
        if (Unit.PositionMeters.GetAbsMax() > 1000000.0)
            return Fail(TEXT("position coordinates must be within +/-1,000,000 meters"));
        if (Object->HasField(TEXT("spacing")) && (!ReadScenarioNumber(Object, TEXT("spacing"), Unit.SpacingMeters) || Unit.SpacingMeters <= 0 || Unit.SpacingMeters > 10000.0))
            return Fail(TEXT("spacing must be > 0 and <= 10,000 meters"));
        const FVector LastPosition = (Unit.PositionMeters + FVector(0, FMath::Max(0, Unit.Count-1) * Unit.SpacingMeters, 0)) * 100;
        if (!Object->HasField(TEXT("instances")) && (LastPosition.ContainsNaN() || !FMath::IsFinite(LastPosition.X) || !FMath::IsFinite(LastPosition.Y) || !FMath::IsFinite(LastPosition.Z)
            || LastPosition.GetAbsMax() > 100000000.0))
            return Fail(TEXT("generated unit positions exceed +/-1,000,000 meters"));
        if (Object->HasField(TEXT("route")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
            if (!Object->TryGetArrayField(TEXT("route"), Points)) return Fail(TEXT("route must be an array of waypoint objects"));
            if (!Points->IsEmpty() && (Points->Num() < 2 || Points->Num() > 32))
                return Fail(TEXT("route must be empty or contain 2 to 32 waypoints"));
            for (int32 PointIndex = 0; PointIndex < Points->Num(); ++PointIndex)
            {
                const TSharedPtr<FJsonValue>& PointValue = (*Points)[PointIndex];
                if (!PointValue.IsValid() || PointValue->Type != EJson::Object)
                    return Fail(FString::Printf(TEXT("route[%d] must be an object"), PointIndex));
                const TSharedPtr<FJsonObject> Point = PointValue->AsObject();
                for (const auto& Field : Point->Values)
                    if (Field.Key != TEXT("x") && Field.Key != TEXT("y") && Field.Key != TEXT("z"))
                        return Fail(FString::Printf(TEXT("route[%d] contains unsupported field: %s"), PointIndex, *Field.Key));
                FVector Waypoint;
                if (!ReadScenarioNumber(Point, TEXT("x"), Waypoint.X) || !ReadScenarioNumber(Point, TEXT("y"), Waypoint.Y) || !ReadScenarioNumber(Point, TEXT("z"), Waypoint.Z))
                    return Fail(FString::Printf(TEXT("route[%d].x/y/z must be finite numbers in meters"), PointIndex));
                if (Waypoint.GetAbsMax() > 1000000.0)
                    return Fail(FString::Printf(TEXT("route[%d] coordinates must be within +/-1,000,000 meters"), PointIndex));
                Unit.RouteMeters.Add(Waypoint);
            }
        }
        if(Object->HasField(TEXT("instances")))
        {
            const TArray<TSharedPtr<FJsonValue>>* Instances=nullptr;
            if(Candidate.SchemaVersion!=2 || !Object->TryGetArrayField(TEXT("instances"),Instances) || Instances->Num()!=Unit.Count)
                return Fail(TEXT("version 2 instances must contain exactly count entries"));
            for(const auto& InstanceValue:*Instances)
            {
                if(!InstanceValue.IsValid() || InstanceValue->Type!=EJson::Object) return Fail(TEXT("instance must be an object"));
                const auto InstanceObject=InstanceValue->AsObject();
                FDemoUnitInstance Instance;
                const TSharedPtr<FJsonObject>* InstancePosition=nullptr;
                if(!ReadScenarioString(InstanceObject,TEXT("id"),Instance.Id) || InstanceIds.Contains(Instance.Id)
                    || !InstanceObject->TryGetObjectField(TEXT("position"),InstancePosition)
                    || !ReadScenarioNumber(*InstancePosition,TEXT("x"),Instance.PositionMeters.X)
                    || !ReadScenarioNumber(*InstancePosition,TEXT("y"),Instance.PositionMeters.Y)
                    || !ReadScenarioNumber(*InstancePosition,TEXT("z"),Instance.PositionMeters.Z)
                    || Instance.PositionMeters.GetAbsMax()>1000000
                    || !ReadScenarioNumber(InstanceObject,TEXT("yaw"),Instance.Yaw) || FMath::Abs(Instance.Yaw)>360000)
                    return Fail(TEXT("invalid instance id, coordinates or yaw"));
                for(const auto& Field:InstanceObject->Values)
                    if(Field.Key!=TEXT("id") && Field.Key!=TEXT("position") && Field.Key!=TEXT("yaw") && Field.Key!=TEXT("formation") && Field.Key!=TEXT("highlight") && Field.Key!=TEXT("flightRoute")) return Fail(TEXT("unsupported instance field: ")+Field.Key);
                if(InstanceObject->HasField(TEXT("flightRoute")))
                {
                    const TSharedPtr<FJsonObject>* Route=nullptr;
                    const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
                    if(Unit.Type!=TEXT("aircraft") || !InstanceObject->TryGetObjectField(TEXT("flightRoute"),Route)
                        || !ReadScenarioNumber(*Route,TEXT("periodSeconds"),Instance.FlightRoute.PeriodSeconds)
                        || !(*Route)->HasTypedField<EJson::Boolean>(TEXT("closed"))
                        || !(*Route)->TryGetBoolField(TEXT("closed"),Instance.FlightRoute.bClosed)
                        || !(*Route)->TryGetArrayField(TEXT("points"),Points) || Points->Num()>512)
                        return Fail(TEXT("flightRoute requires aircraft, duration, boolean closed and 2..512 points"));
                    for(const auto& Field:(*Route)->Values)
                        if(Field.Key!=TEXT("periodSeconds") && Field.Key!=TEXT("closed") && Field.Key!=TEXT("points")) return Fail(TEXT("unsupported flightRoute field: ")+Field.Key);
                    for(const auto& Point:*Points)
                    {
                        if(!Point.IsValid() || Point->Type!=EJson::Object) return Fail(TEXT("flightRoute point must be an object"));
                        const auto P=Point->AsObject(); FVector V;
                        if(P->Values.Num()!=3 || !ReadScenarioNumber(P,TEXT("x"),V.X) || !ReadScenarioNumber(P,TEXT("y"),V.Y) || !ReadScenarioNumber(P,TEXT("z"),V.Z)) return Fail(TEXT("flightRoute points require only finite x/y/z"));
                        Instance.FlightRoute.PointsMeters.Add(V);
                    }
                    FString RouteError;
                    if(!Instance.FlightRoute.Validate(RouteError)) return Fail(RouteError);
                }
                if(InstanceObject->HasField(TEXT("highlight")))
                {
                    if(InstanceObject->Values[TEXT("highlight")]->Type!=EJson::Boolean || !InstanceObject->TryGetBoolField(TEXT("highlight"),Instance.bHighlighted)) return Fail(TEXT("highlight must be boolean"));
                    if(Instance.bHighlighted && Unit.Type!=TEXT("aircraft")) return Fail(TEXT("highlight only supports aircraft"));
                }
                if(InstanceObject->HasField(TEXT("formation")))
                {
                    if(!ReadScenarioString(InstanceObject,TEXT("formation"),Instance.Formation)) return Fail(TEXT("formation must be a nonempty string"));
                    Instance.Formation=Instance.Formation.TrimStartAndEnd();
                    if(Instance.Formation.Len()>32 || Instance.Formation.Contains(TEXT("\n")) || Instance.Formation.Contains(TEXT("\r"))) return Fail(TEXT("invalid formation name"));
                }
                InstanceIds.Add(Instance.Id);
                Instance.Yaw=FRotator::NormalizeAxis(Instance.Yaw);
                Unit.Instances.Add(Instance);
            }
        }
        else
        {
            for(int32 I=0;I<Unit.Count;++I)
            {
                const FString GeneratedId=FString::Printf(TEXT("%s_%03d"),*Unit.Id,I+1);
                if(InstanceIds.Contains(GeneratedId)) return Fail(TEXT("duplicate generated unit id"));
                InstanceIds.Add(GeneratedId);
            }
        }
        for (const TCHAR* Deferred : {TEXT("formation"), TEXT("speed"), TEXT("radarRange"), TEXT("altitude")})
            if (Object->HasField(Deferred)) UE_LOG(LogTemp, Warning, TEXT("units[%d].%s is not supported in phase 1; ignored"), Index, Deferred);
        Candidate.Units.Add(MoveTemp(Unit));
    }
    OutConfig = MoveTemp(Candidate);
    return true;
}

FString FScenarioConfigLoader::Serialize(const FDemoScenarioConfig& Config)
{
    auto Root=MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("schemaVersion"),2);
    Root->SetStringField(TEXT("scenarioName"),Config.Name.IsEmpty()?TEXT("Empty"):Config.Name);
    if(!Config.MapAsset.IsEmpty()) Root->SetStringField(TEXT("mapAsset"),Config.MapAsset);
    auto Position=[](const FVector& P)
    { auto O=MakeShared<FJsonObject>(); O->SetNumberField(TEXT("x"),P.X); O->SetNumberField(TEXT("y"),P.Y); O->SetNumberField(TEXT("z"),P.Z); return O; };
    TArray<TSharedPtr<FJsonValue>> Groups;
    for(const auto& Group:Config.Units)
    {
        auto O=MakeShared<FJsonObject>();
        O->SetStringField(TEXT("id"),Group.Id); O->SetStringField(TEXT("modelId"),Group.ModelId);
        O->SetStringField(TEXT("type"),Group.Type); O->SetStringField(TEXT("side"),Group.Side);
        if(!Group.TextureId.IsEmpty()) O->SetStringField(TEXT("textureId"),Group.TextureId);
        O->SetNumberField(TEXT("count"),Group.Count); O->SetNumberField(TEXT("spacing"),Group.SpacingMeters);
        O->SetObjectField(TEXT("position"),Position(Group.PositionMeters));
        TArray<TSharedPtr<FJsonValue>> Route,Instances;
        for(const FVector& P:Group.RouteMeters) Route.Add(MakeShared<FJsonValueObject>(Position(P)));
        O->SetArrayField(TEXT("route"),Route);
        for(const auto& Instance:Group.Instances)
        {
            auto I=MakeShared<FJsonObject>(); I->SetStringField(TEXT("id"),Instance.Id);
            I->SetObjectField(TEXT("position"),Position(Instance.PositionMeters)); I->SetNumberField(TEXT("yaw"),Instance.Yaw);
            if(!Instance.Formation.IsEmpty()) I->SetStringField(TEXT("formation"),Instance.Formation);
            if(Instance.bHighlighted) I->SetBoolField(TEXT("highlight"),true);
            if(!Instance.FlightRoute.PointsMeters.IsEmpty())
            {
                auto R=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Points;
                for(const auto& P:Instance.FlightRoute.PointsMeters) Points.Add(MakeShared<FJsonValueObject>(Position(P)));
                R->SetArrayField(TEXT("points"),Points); R->SetNumberField(TEXT("periodSeconds"),Instance.FlightRoute.PeriodSeconds);
                R->SetBoolField(TEXT("closed"),Instance.FlightRoute.bClosed); I->SetObjectField(TEXT("flightRoute"),R);
            }
            Instances.Add(MakeShared<FJsonValueObject>(I));
        }
        if(Group.Instances.Num()==Group.Count) O->SetArrayField(TEXT("instances"),Instances);
        Groups.Add(MakeShared<FJsonValueObject>(O));
    }
    Root->SetArrayField(TEXT("units"),Groups);
    FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json)); return Json;
}

bool FScenarioConfigLoader::Load(const FString& Path, FDemoScenarioConfig& OutConfig, FString& OutError)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        OutError = TEXT("Cannot read scenario: ") + Path;
        return false;
    }
    if (!Parse(Text, OutConfig, OutError))
    {
        OutError = Path + TEXT(": ") + OutError;
        return false;
    }
    return true;
}
