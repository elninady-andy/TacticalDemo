#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "ScenarioPresetStore.h"
#include "DemoLayoutWidget.generated.h"
class UEditableTextBox;
class UComboBoxString;
class UTextBlock;
class UButton;
class UWidgetSwitcher;
class UCheckBox;
class UVerticalBox;
class ADemoScenarioManager;
class ADemoPlayerController;

UCLASS()
class TACTICALDEMO_API UDemoLayoutWidget : public UUserWidget
{
    GENERATED_BODY()
    friend class FLayoutWindowTest;
    friend class FFormationWindowTest;
    friend class FLayoutWindowDragTest;
    friend class FMultiSelectionWindowTest;
    friend class ADemoGameMode;
public:
    void RefreshSelection();
    void EnsureInitialized();
    void SelectFormation(const FString& Name);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;
private:
    bool BeginWindowDrag(const FVector2D& Position,const FGeometry& ParentGeometry);
    void MoveWindowDrag(const FVector2D& Position,const FGeometry& ParentGeometry);
    UPROPERTY() TObjectPtr<UWidget> DragTitle;
    UPROPERTY() TObjectPtr<UButton> HeaderCloseButton;
    bool bDraggingWindow=false;
    FVector2D DragStartPointer,DragStartOffset;
    UFUNCTION() void ApplyTransform();
    UFUNCTION() void Undo();
    UFUNCTION() void Redo();
    UFUNCTION() void SavePreset();
    UFUNCTION() void LoadPreset();
    UFUNCTION() void ImportPreset();
    UFUNCTION() void ExportPreset();
    UFUNCTION() void RefreshPresets();
    UFUNCTION() void Close();
    UFUNCTION() void ShowUnits();
    UFUNCTION() void ShowFormations();
    UFUNCTION() void ShowPresets();
    UFUNCTION() void ShowRoutes();
    UFUNCTION() void SelectAllAircraft();
    UFUNCTION() void ClearSelectedAircraft();
    UFUNCTION() void CreateFromSelection();
    UFUNCTION() void FormationChanged(FString Name,ESelectInfo::Type Type);
    UFUNCTION() void SaveFormation();
    UFUNCTION() void ApplyFormation();
    UFUNCTION() void ToggleMembers();
    void RefreshFormations();
    void RefreshMembers();
    UFUNCTION() void SelectionChanged(FString Id,ESelectInfo::Type Type);
    ADemoScenarioManager* Manager() const;
    ADemoPlayerController* Player() const;
    void Result(const FString& Message);
    UPROPERTY() TObjectPtr<UComboBoxString> UnitPicker;
    UPROPERTY() TArray<TObjectPtr<UEditableTextBox>> TransformFields;
    UPROPERTY() TObjectPtr<UEditableTextBox> PresetName;
    UPROPERTY() TObjectPtr<UEditableTextBox> FilePath;
    UPROPERTY() TObjectPtr<UComboBoxString> PresetPicker;
    UPROPERTY() TObjectPtr<UTextBlock> Feedback;
    UPROPERTY() TObjectPtr<UTextBlock> SelectionInfo;
    UPROPERTY() TObjectPtr<UEditableTextBox> SelectedFormationName;
    UPROPERTY() TObjectPtr<class UDemoRouteWidget> RouteEditor;
    UPROPERTY() TObjectPtr<UButton> UndoButton;
    UPROPERTY() TObjectPtr<UButton> RedoButton;
    UPROPERTY() TObjectPtr<UWidgetSwitcher> Pages;
    UPROPERTY() TObjectPtr<UComboBoxString> FormationPicker;
    UPROPERTY() TObjectPtr<UComboBoxString> PatternPicker;
    UPROPERTY() TObjectPtr<UEditableTextBox> FormationName;
    UPROPERTY() TObjectPtr<UVerticalBox> MemberList;
    UPROPERTY() TObjectPtr<UVerticalBox> MemberSection;
    UPROPERTY() TArray<TObjectPtr<UCheckBox>> MemberChecks;
    UPROPERTY() TArray<TObjectPtr<UEditableTextBox>> FormationFields;
    TArray<FString> MemberIds;
    uint64 FormationRevision=MAX_uint64;
    TArray<FScenarioPresetEntry> Presets;
    FString SelectionSignature;
    FString DisplayedUnitId;
    uint64 SeenRevision=MAX_uint64;
    uint64 SeenSelectionRevision=MAX_uint64;
    bool bRefreshing=false;
};
