#pragma once
#include "CoreMinimal.h"

struct TACTICALDEMO_API FDemoFlightRoute
{
    TArray<FVector> PointsMeters;
    double PeriodSeconds=40;
    bool bClosed=false;
    bool Validate(FString& Error);
};

struct FDemoUnitInstance
{
    FString Id;
    FVector PositionMeters = FVector::ZeroVector;
    double Yaw = 0;
    FString Formation;
    bool bHighlighted=false;
    FDemoFlightRoute FlightRoute;
};

struct FDemoUnitConfig
{
    FString Id;
    FString ModelId;
    FString TextureId;
    FString Type;
    FString Side;
    int32 Count = 0;
    FVector PositionMeters = FVector::ZeroVector;
    double SpacingMeters = 10.0;
    TArray<FVector> RouteMeters;
    TArray<FDemoUnitInstance> Instances;
};

struct FDemoScenarioConfig
{
    FString Name;
    FString MapAsset;
    int32 SchemaVersion = 1;
    TArray<FDemoUnitConfig> Units;
};
