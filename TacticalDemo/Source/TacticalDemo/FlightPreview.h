#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlightPreview.generated.h"

class ADemoScenarioManager;
class ADemoUnitActor;
class UStaticMeshComponent;
class UStaticMesh;

struct TACTICALDEMO_API FDemoFlightPath
{
    FString ModelId;
    double PeriodSeconds=40;
    double YawOffsetDegrees=0;
    bool bClosed=true;
    double EntryDistanceCm=0;
    TArray<FVector> SamplesCm;
    TArray<double> DistancesCm;
    TArray<FVector> Directions;
    static bool ParseJson(const FString& Json,FString& MapAsset,TArray<FDemoFlightPath>& Paths,FString& Error);
    // Input is in world meters. Optional entry segment joins the aircraft to the first waypoint.
    bool FromRoute(const TArray<FVector>& PointsMeters,double Period,bool Closed,const FVector& EntryCm,FString& Error);
    void Evaluate(double Seconds,FVector& Position,FRotator& Heading) const;
};

// Display-only clones. Source actors and committed scenario data never move.
UCLASS()
class TACTICALDEMO_API ADemoFlightPreview : public AActor
{
    GENERATED_BODY()
public:
    ADemoFlightPreview();
    bool Start(ADemoScenarioManager* Manager,const FString& PathsFile=FString());
    void Stop();
    bool IsPlaying() const { return bPlaying; }
    const FString& GetLastError() const { return LastError; }
    FBox GetPreviewBounds(const FString& FollowUnitId=FString()) const;
    FVector GetPreviewCenter() const;
    bool GetPreviewTransform(const FString& UnitId,FTransform& Transform) const;
    void AdvancePreview(double DeltaSeconds);
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FBinding
    {
        FString UnitId;
        FString GroupId;
        TWeakObjectPtr<ADemoUnitActor> Actor;
        TWeakObjectPtr<UStaticMeshComponent> Original;
        TWeakObjectPtr<UStaticMesh> Mesh;
        int32 PathIndex=INDEX_NONE;
        FVector Anchor=FVector::ZeroVector;
        FVector Offset=FVector::ZeroVector;
        double RelativeYaw=0;
        bool bWasVisible=true;
    };
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> PreviewVisuals;
    TArray<FBinding> Bindings;
    TArray<FDemoFlightPath> Paths;
    TWeakObjectPtr<ADemoScenarioManager> SourceManager;
    uint64 SourceRevision=0;
    double ElapsedSeconds=0;
    bool bPlaying=false;
    FString LastError;
};
