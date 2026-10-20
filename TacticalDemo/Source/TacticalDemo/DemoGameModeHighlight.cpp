#include "DemoGameMode.h"
#include "DemoControlWidget.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "FlightPreview.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// Opt-in runtime acceptance; ordinary startup has no test side effects.
void ADemoGameMode::SetupHighlightVerification(ADemoScenarioManager* Manager)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyHighlight"))) return;
    auto Stage=MakeShared<int32>(0); auto Valid=MakeShared<bool>(true); auto Before=MakeShared<FString>();
    auto MaterialSlots=MakeShared<TArray<TArray<UMaterialInterface*>>>();
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer,FTimerDelegate::CreateWeakLambda(this,[this,Manager,Stage,Valid,Before,MaterialSlots]()
    {
        auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController()); auto* W=P?P->ControlWindow.Get():nullptr;
        if(!W || !W->Input || *Stage>6) return;
        auto Fail=[&](){UE_LOG(LogTemp,Error,TEXT("HIGHLIGHT_RUNTIME_FAILED stage=%d"),*Stage); FGenericPlatformMisc::RequestExit(false);};
        auto Aircraft=Manager->GetUnits(); if(Aircraft.Num()!=6) { Fail(); return; }
        auto* First=Aircraft[0].Get(); ADemoUnitActor* Second=nullptr;
        for(ADemoUnitActor* U:Aircraft) if(U->Side!=First->Side) { Second=U; break; }
        if(!Second) { Fail(); return; }
        auto Send=[&](const FString& Text){W->Input->SetText(FText::FromString(Text)); W->Send();};
        if(*Stage==0)
        {
            *Before=Manager->SnapshotJson();
            for(ADemoUnitActor* U:Aircraft)
            {
                TArray<UMaterialInterface*> Slots;
                for(int32 I=0;I<U->Visual->GetNumMaterials();++I) Slots.Add(U->Visual->GetMaterial(I));
                MaterialSlots->Add(Slots);
            }
            Send(TEXT("让")+First->UnitId+TEXT("发光高亮"));
            *Valid&=!W->Candidate.IsEmpty() && W->bHighlightCandidate && !W->ActiveRequest.IsValid() && Manager->SnapshotJson()==*Before;
        }
        else if(*Stage==1)
        {
            W->Apply();
            for(ADemoUnitActor* U:Aircraft) *Valid&=(U->Visual->GetOverlayMaterial()!=nullptr)==(U==First);
            const FString Lit=Manager->SnapshotJson();
            *Valid&=Manager->UndoEdit() && Manager->SnapshotJson()==*Before && !First->Visual->GetOverlayMaterial();
            *Valid&=Manager->RedoEdit() && Manager->SnapshotJson()==Lit && First->Visual->GetOverlayMaterial()!=nullptr;
        }
        else if(*Stage==2) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Highlight/Single.png"),true,false);
        else if(*Stage==3)
        {
            W->Units->SetSelectedOption(Second->UnitId); W->Units->OnSelectionChanged.Broadcast(Second->UnitId,ESelectInfo::Direct);
            Send(TEXT("让选中的飞机高亮")); W->Apply();
            *Valid&=First->Visual->GetOverlayMaterial()!=nullptr && Second->Visual->GetOverlayMaterial()!=nullptr;
            if(Manager->CurrentMapAsset()==TEXT("/Game/Maps/map3"))
                for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It)
                {
                    const FString Static=Manager->SnapshotJson(); *Valid&=It->Start(Manager);
                    TArray<UStaticMeshComponent*> Clones; It->GetComponents(Clones); int32 LitClones=0;
                    for(auto* C:Clones) if(C->GetOverlayMaterial()) ++LitClones;
                    *Valid&=LitClones==2 && Manager->SnapshotJson()==Static; It->Stop();
                }
        }
        else if(*Stage==4) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Highlight/BothModels.png"),true,false);
        else if(*Stage==5)
        {
            Send(TEXT("取消")+First->UnitId+TEXT("高亮")); W->Apply();
            Send(TEXT("取消")+Second->UnitId+TEXT("高亮")); W->Apply();
            P->SelectUnit(FString()); // Selection glow is temporary and independent of the saved effect.
            *Valid&=Manager->SnapshotJson()==*Before;
            for(int32 U=0;U<Aircraft.Num();++U)
            {
                auto* Visual=Aircraft[U]->Visual.Get(); *Valid&=Visual->GetOverlayMaterial()==nullptr && Visual->GetNumMaterials()==(*MaterialSlots)[U].Num();
                for(int32 I=0;I<(*MaterialSlots)[U].Num();++I) *Valid&=Visual->GetMaterial(I)==(*MaterialSlots)[U][I];
            }
            if(*Valid) { UE_LOG(LogTemp,Display,TEXT("HIGHLIGHT_RUNTIME_VERIFIED map=%s single=1 selection=1 undoRedo=1 materialsRestored=1"),*Manager->CurrentMapAsset()); }
            else { Fail(); }
        }
        else FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Highlight/Restored.png"),true,false);
        ++*Stage;
    }),3.f,true);
}
