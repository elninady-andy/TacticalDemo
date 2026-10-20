#include "DemoGameMode.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"

TArray<FString> ADemoPlayerController::SelectedUnitIds() const
{
    TArray<FString> Ids; auto* M=ScenarioManager(); if(!M) return Ids;
    if(auto* Main=SelectedUnit()) Ids.Add(Main->UnitId);
    for(const auto& W:SelectedUnits) if(auto* U=W.Get()) if(M->FindUnit(U->UnitId)==U) Ids.AddUnique(U->UnitId);
    return Ids;
}
bool ADemoPlayerController::IsUnitSelected(const FString& Id) const { return SelectedUnitIds().Contains(Id); }
void ADemoPlayerController::RefreshSelectionVisuals()
{ if(auto* M=ScenarioManager()) M->SetSelectionHighlights(SelectedUnitIds()); }
void ADemoPlayerController::ToggleUnitSelection(const FString& Id)
{
    CancelUnitDrag(); auto* M=ScenarioManager(); auto* Unit=M?M->FindUnit(Id):nullptr; if(!Unit) return;
    if(IsUnitSelected(Id))
    {
        SelectedUnits.RemoveAll([&](const TWeakObjectPtr<ADemoUnitActor>& U){return !U.IsValid() || U.Get()==Unit;});
        if(Selection.Get()==Unit) Selection=SelectedUnits.Num()?SelectedUnits.Last():TWeakObjectPtr<ADemoUnitActor>();
    }
    else { SelectedUnits.Add(Unit); Selection=Unit; }
    ++SelectedRevision; RefreshSelectionVisuals();
}
