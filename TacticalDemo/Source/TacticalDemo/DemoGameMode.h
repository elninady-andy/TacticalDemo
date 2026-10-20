#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "DemoGameMode.generated.h"

class ADemoScenarioManager;
class ADemoUnitActor;

UCLASS()
class TACTICALDEMO_API ADemoPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ADemoScenarioManager* ScenarioManager() const;
    ADemoUnitActor* SelectedUnit() const;
    void SelectUnit(const FString& Id);
    void ToggleUnitSelection(const FString& Id);
    TArray<FString> SelectedUnitIds() const;
    uint64 SelectionRevision() const { return SelectedRevision; }
    bool IsUnitSelected(const FString& Id) const;
    bool SelectFromRay(const FVector& Origin,const FVector& Direction,bool Additive=false);
    bool BeginUnitDrag(const FString& Id,const FVector& RayOrigin,const FVector& RayDirection);
    void UpdateUnitDrag(const FVector& RayOrigin,const FVector& RayDirection);
    void EndUnitDrag();
    void CancelUnitDrag();
    static bool IntersectDragPlane(const FVector& Origin,const FVector& Direction,double Height,FVector& Point);
    bool IsDraggingUnit() const { return DragUnit.IsValid(); }
protected:
    virtual void PlayerTick(float DeltaTime) override;
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
private:
    friend class ADemoGameMode;
    void LookStart();
    void LookEnd();
    UPROPERTY() TObjectPtr<class UDemoControlWidget> ControlWindow;
    UPROPERTY() TObjectPtr<class UDemoNameplateWidget> NameplateOverlay;
    void Reload();
    void Frame();
    void SelectStart();
    void SelectEnd();
    void Escape();
    TWeakObjectPtr<ADemoUnitActor> Selection,DragUnit;
    TArray<TWeakObjectPtr<ADemoUnitActor>> SelectedUnits;
    struct FDragMember { TWeakObjectPtr<ADemoUnitActor> Actor; FVector Position; double Yaw=0; };
    TArray<FDragMember> DragMembers;
    uint64 SelectedRevision=0,DragRevision=0;
    void RefreshSelectionVisuals();
    FVector DragStart,DragOffset;
    double DragYaw=0;
    bool bFollowingFlight=false;
    FTransform PreFlightView;
    FRotator PreFlightRotation;
};

UCLASS()
class TACTICALDEMO_API ADemoGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADemoGameMode();
    void FrameScene(ADemoScenarioManager* Manager);
protected:
    virtual void BeginPlay() override;
private:
    FTimerHandle SceneVerifyTimer;
    void SetupHighlightVerification(ADemoScenarioManager* Manager);
    void SetupSelectionVerification(ADemoScenarioManager* Manager);
    void SetupNameplateVerification(ADemoScenarioManager* Manager);
};
