#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DemoRouteWidget.generated.h"
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UCheckBox;
class ADemoPlayerController;
class ADemoScenarioManager;

UCLASS()
class TACTICALDEMO_API UDemoRouteWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void EnsureInitialized();
    void RefreshSelection();
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
private:
    friend class FRouteWidgetDraftTest;
    friend class ADemoGameMode;
    ADemoPlayerController* Player() const;
    ADemoScenarioManager* Manager() const;
    UEditableTextBox* MakeField(const FString& Value);
    UTextBlock* MakeText(const FString& Value,int32 Size=14);
    void BuildRows(const TArray<FVector>& Points);
    bool ReadPoints(TArray<FVector>& Points) const;
    FString CurrentDraftText() const;
    void LoadSelection();
    bool IsCurrent() const;
    UFUNCTION() void AddPoint();
    UFUNCTION() void RemovePoint();
    UFUNCTION() void Apply();
    UFUNCTION() void Reset();
    UFUNCTION() void Reload();
    UFUNCTION() void FieldChanged(const FText& Text);
    UFUNCTION() void ClosedChanged(bool Checked);
    UPROPERTY() TObjectPtr<UVerticalBox> PointRows;
    UPROPERTY() TObjectPtr<UTextBlock> SelectionLabel;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UEditableTextBox> Duration;
    UPROPERTY() TObjectPtr<UCheckBox> Closed;
    UPROPERTY() TArray<TObjectPtr<UEditableTextBox>> PointFields;
    TArray<FString> DraftIds;
    FString LoadedDraftText;
    uint64 DraftRevision=MAX_uint64;
    uint64 DraftSelectionRevision=MAX_uint64;
    bool bDirty=false,bLoading=false;
};
