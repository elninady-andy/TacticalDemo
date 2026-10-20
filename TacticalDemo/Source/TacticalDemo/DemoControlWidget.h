#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/IHttpRequest.h"
#include "Components/ComboBoxString.h"
#include "DemoControlWidget.generated.h"

class UTextBlock;
class UEditableTextBox;
class UScrollBox;
class UVerticalBox;
class UButton;
class UComboBoxString;
class UBorder;
class ADemoScenarioManager;

UCLASS()
class TACTICALDEMO_API UDemoControlWidget : public UUserWidget
{
    GENERATED_BODY()
    friend class FChatWindowTest;
    friend class FChatHttpCommand;
    friend class FRealQwenCommand;
    friend class FCommandCatalogRun;
    friend class FImportedQwenRun;
    friend class FTextureQwenRun;
    friend class FLayoutQwenRun;
    friend class ADemoGameMode;
    friend class FLayoutWindowTest;
    friend class FFormationChatWindowTest;
    friend class FHighlightChatWindowTest;
public:
    bool BlocksScenePointer(const FVector2D& AbsolutePosition) const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float Delta) override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void Send();
    UFUNCTION() void Cancel();
    UFUNCTION() void Apply();
    UFUNCTION() void Reload();
    UFUNCTION() void Clear();
    UFUNCTION() void Frame();
    UFUNCTION() void ToggleFlightPreview();
    UFUNCTION() void ToggleResources();
    UFUNCTION() void SaveResources();
    UFUNCTION() void LoadResources();
    UFUNCTION() void LoadResourceScene();
    UFUNCTION() void ToggleLayout();
    UFUNCTION() void UnitSelectionChanged(FString Id,ESelectInfo::Type Type);
    UTextBlock* Text(const FString& Value, int32 Size = 16);
    UButton* Button(UVerticalBox* Parent, const FString& Label, FName Handler);
    void AddMessage(const FString& Speaker, const FString& Value);
    ADemoScenarioManager* Manager() const;
    void FinishRequest(FHttpRequestPtr Request, FHttpResponsePtr Response, bool Success);
    UPROPERTY() TObjectPtr<UEditableTextBox> Input;
    UPROPERTY() TObjectPtr<UEditableTextBox> Endpoint;
    UPROPERTY() TObjectPtr<UEditableTextBox> Model;
    UPROPERTY() TObjectPtr<UScrollBox> HistoryBox;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UTextBlock> Details;
    UPROPERTY() TObjectPtr<UTextBlock> CandidateInfo;
    UPROPERTY() TObjectPtr<UComboBoxString> Units;
    UPROPERTY() TObjectPtr<UButton> SendButton;
    UPROPERTY() TObjectPtr<UButton> ApplyButton;
    UPROPERTY() TObjectPtr<UButton> FlightButton;
    UPROPERTY() TObjectPtr<UBorder> ResourceWindow;
    UPROPERTY() TObjectPtr<class UDemoLayoutWidget> LayoutWindow;
    UPROPERTY() TArray<TObjectPtr<class UWidget>> InputPanels;
    UPROPERTY() TArray<TObjectPtr<UEditableTextBox>> ResourcePaths;
    UPROPERTY() TObjectPtr<UTextBlock> ResourceStatus;
    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
    TArray<TPair<FString, FString>> Messages;
    FString Candidate;
    bool bFormationCandidate=false;
    bool bFormationMembershipOnly=false;
    bool bHighlightCandidate=false;
    FString CandidateLevel;
    FString PendingScenario;
    FString CandidateBaseScenario;
    bool bPendingSceneRequest = false;
    FString UnitSignature;
    bool bRefreshingUnits=false;
    uint64 ObservedRevision=MAX_uint64;
    uint64 PendingRevision=0;
    uint64 CandidateRevision=0;
};
