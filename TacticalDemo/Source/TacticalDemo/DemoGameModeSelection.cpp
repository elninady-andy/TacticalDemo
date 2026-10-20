#include "DemoGameMode.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "DemoControlWidget.h"
#include "DemoLayoutWidget.h"
#include "DemoRouteWidget.h"
#include "FlightPreview.h"
#include "ScenarioPresetStore.h"
#include "ScenarioConfigLoader.h"
#include "Components/StaticMeshComponent.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ADemoGameMode::SetupSelectionVerification(ADemoScenarioManager* Manager)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("DemoVerifySelection"))) return;
    auto Stage=MakeShared<int32>(0); auto Valid=MakeShared<bool>(true);
    auto Before=MakeShared<FString>(); auto Routed=MakeShared<FString>(); auto Start=MakeShared<FVector>();
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer,FTimerDelegate::CreateWeakLambda(this,[this,Manager,Stage,Valid,Before,Routed,Start]()
    {
        auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController()); auto* W=P?P->ControlWindow.Get():nullptr;
        if(!W || !W->LayoutWindow || *Stage>8) return;
        auto Fail=[&](){UE_LOG(LogTemp,Error,TEXT("MULTI_SELECTION_RUNTIME_FAILED stage=%d"),*Stage); FGenericPlatformMisc::RequestExit(false);};
        const auto Units=Manager->GetUnits(); if(Units.Num()!=6) {Fail(); return;}
        auto* First=Units[0].Get(); auto* Second=Units[3].Get(); auto* Editor=W->LayoutWindow.Get();
        if(*Stage==0)
        {
            *Before=Manager->SnapshotJson();
            *Valid&=P->SelectFromRay(First->Visual->Bounds.Origin+FVector(0,0,100000),FVector(0,0,-1)); P->EndUnitDrag();
            *Valid&=P->SelectFromRay(Second->Visual->Bounds.Origin+FVector(0,0,100000),FVector(0,0,-1),true);
            *Valid&=P->SelectedUnitIds().Num()==2 && First->Visual->GetOverlayMaterial() && Second->Visual->GetOverlayMaterial() && Manager->SnapshotJson()==*Before;
        }
        else if(*Stage==1) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("MultiSelection/Selected.png"),true,false);
        else if(*Stage==2)
        {
            W->ToggleLayout(); Editor->ShowUnits(); Editor->RefreshSelection();
            Editor->SelectedFormationName->SetText(FText::FromString(TEXT("演示多选编队"))); Editor->CreateFromSelection();
            const FString Assigned=Manager->SnapshotJson(); int32 Members=0;
            for(const auto& G:Manager->GetScenarioConfig().Units) for(const auto& I:G.Instances) if(I.Formation==TEXT("演示多选编队")) ++Members;
            *Valid&=Members==2;
            const FVector Original=Second->GetActorLocation()/100.;
            Editor->TransformFields[0]->SetText(FText::FromString(FString::SanitizeFloat(Original.X+20)));
            Editor->ApplyTransform();
            *Valid&=FMath::IsNearlyEqual(Second->GetActorLocation().X,(Original.X+20)*100.,.1);
            Editor->Undo(); *Valid&=Manager->SnapshotJson()==Assigned; Editor->Redo();
        }
        else if(*Stage==3) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("MultiSelection/BatchEditor.png"),true,false);
        else if(*Stage==4)
        {
            Editor->ShowRoutes(); auto* Route=Editor->RouteEditor.Get(); Route->Reload();
            const FVector Origin=Second->GetActorLocation()/100.;
            Route->Closed->SetIsChecked(false); Route->BuildRows({Origin,Origin+FVector(100,40,20),Origin+FVector(200,0,0)});
            Route->Duration->SetText(FText::FromString(TEXT("12"))); Route->Apply();
            int32 Routes=0; for(const auto& G:Manager->GetScenarioConfig().Units) for(const auto& I:G.Instances) if(!I.FlightRoute.PointsMeters.IsEmpty()) ++Routes;
            *Valid&=Routes==2; *Routed=Manager->SnapshotJson();
            const FString Dir=FPaths::ProjectSavedDir()/TEXT("MultiSelection/TransientPreset-")+FGuid::NewGuid().ToString();
            FScenarioPresetStore Store(Dir); FString Id,Error,Saved;
            *Valid&=Store.SaveNew(TEXT("多选航迹验收"),*Routed,Id,Error) && Store.Read(Id,Saved,Error);
            FDemoScenarioConfig Parsed;
            *Valid&=FScenarioConfigLoader::Parse(Saved,Parsed,Error);
            // The isolated evidence preset is outside the user's Scenarios library.
        }
        else if(*Stage==5) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("MultiSelection/RouteEditor.png"),true,false);
        else if(*Stage==6)
        {
            Editor->Close(); *Valid&=W->FlightButton->GetVisibility()==ESlateVisibility::Visible;
            W->FlightButton->OnClicked.Broadcast();
            for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It)
            { FTransform T; *Valid&=It->IsPlaying() && It->GetPreviewTransform(Second->UnitId,T); *Start=T.GetLocation(); }
        }
        else if(*Stage==7)
        {
            for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It)
            {
                FTransform T; *Valid&=It->GetPreviewTransform(Second->UnitId,T) && FVector::Distance(T.GetLocation(),*Start)>100;
                *Valid&=Manager->SnapshotJson()==*Routed;
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("MultiSelection/RouteMoving.png"),true,false);
        }
        else
        {
            W->ToggleFlightPreview(); P->SelectUnit(FString());
            *Valid&=Manager->SnapshotJson()==*Routed;
            for(ADemoUnitActor* U:Manager->GetUnits()) *Valid&=U->Visual->IsVisible() && !U->Visual->GetOverlayMaterial();
            if(*Valid) {UE_LOG(LogTemp,Display,TEXT("MULTI_SELECTION_RUNTIME_VERIFIED map=%s multi=2 formation=2 batchUndo=1 customRoutes=2 saved=1 motion=1 originalsRestored=1"),*Manager->CurrentMapAsset());}
            else {Fail();}
        }
        ++*Stage;
    }),3.f,true);
}
