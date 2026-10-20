#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ScenarioConfig.h"
#include "DemoScenarioManager.generated.h"

class ADemoUnitActor;
class UMaterialInterface;
class ULineBatchComponent;

UCLASS(Config=Game)
class TACTICALDEMO_API ADemoScenarioManager : public AActor
{
    GENERATED_BODY()
public:
    ADemoScenarioManager();
    bool LoadScenario(const FString& ScenarioPath, const FString& LibraryPath);
    bool LoadChatCandidate(const FString& ScenarioPath, const FString& LibraryPath);
    UFUNCTION(BlueprintCallable) bool ReloadScenario();
    UFUNCTION(BlueprintCallable) void ClearScenario();
    const TArray<TObjectPtr<ADemoUnitActor>>& GetUnits() const { return Units; }
    const FString& GetLastError() const { return LastError; }
    const FString& GetScenarioName() const { return ScenarioName; }
    const FString& GetScenarioJson() const { return ScenarioJson; }
    const FDemoScenarioConfig& GetScenarioConfig() const { return ScenarioConfig; }
    ADemoUnitActor* FindUnit(const FString& Id) const;
    bool EditUnit(const FString& Id, const FVector& PositionMeters, double Yaw);
    bool EditUnits(const TArray<FString>& Ids,const FString& AnchorId,const FVector& PositionMeters,double Yaw);
    bool CreateSelectedFormation(const FString& Name,const TArray<FString>& Ids);
    void SetSelectionHighlights(const TArray<FString>& Ids);
    bool SetFormationMembers(const FString& Name,const TArray<FString>& MemberIds);
    bool ArrangeFormation(const FString& Name,int32 Pattern,double Spacing,double Heading,const FVector2D& Anchor);
    bool ApplyFormationCandidate(const FString& Json,bool bMembershipOnly=false);
    bool ApplyHighlightCandidate(const FString& Json);
    bool EditRoutes(const TArray<FString>& Ids,const TArray<FVector>& Points,double Period,bool Closed);
    bool ResetRoutes(const TArray<FString>& Ids);
    bool UndoEdit();
    bool RedoEdit();
    bool CanUndo() const { return UndoStack.Num()>0; }
    bool CanRedo() const { return RedoStack.Num()>0; }
    uint64 GetRevision() const { return Revision; }
    FString CurrentMapAsset() const;
    FString SnapshotJson() const;
    bool LoadJson(const FString& Json, const FString& LibraryPath, bool bRequireExactModels=true);
    UPROPERTY(Config) FString UnitMaterialPath;
    UPROPERTY(Config) FString HighlightMaterialPath;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TArray<TObjectPtr<ADemoUnitActor>> Units;
    UPROPERTY() TObjectPtr<UMaterialInterface> UnitMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> HighlightMaterial;
    TSet<FString> SelectionHighlightIds;
    void RefreshUnitHighlights();
    UPROPERTY() TObjectPtr<ULineBatchComponent> RouteLines;
    FString LastError;
    FString ScenarioName;
    FString ScenarioJson;
    FDemoScenarioConfig ScenarioConfig;
    struct FUnitEdit { FDemoScenarioConfig Before,After; };
    TArray<FUnitEdit> UndoStack,RedoStack;
    uint64 Revision=0;
    bool CommitEdit(const FDemoScenarioConfig& Config);
    bool PrepareHighlight(const FDemoScenarioConfig& Config);
    void RestoreEdit(const FDemoScenarioConfig& Config);
    bool LoadScenarioInternal(const FString& ScenarioPath, const FString& LibraryPath, bool bRequireExactModels);
};
