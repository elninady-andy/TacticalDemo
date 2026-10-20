#include "DemoNameplateWidget.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "DemoGameMode.h"
#include "DemoControlWidget.h"
#include "Components/Button.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// Only used by the explicit runtime acceptance switch.
void ADemoGameMode::SetupNameplateVerification(ADemoScenarioManager* Manager)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyNameplates"))) return;
    auto Stage=MakeShared<int32>(0); auto Valid=MakeShared<bool>(true); auto Before=MakeShared<FString>();
    auto Idle=MakeShared<TArray<FDemoNameplateEntry>>();
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer,FTimerDelegate::CreateWeakLambda(this,[this,Manager,Stage,Valid,Before,Idle]()
    {
        auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController()); auto* W=P?P->ControlWindow.Get():nullptr;
        if(!W || !P->NameplateOverlay || *Stage>5) return;
        auto Fail=[&](){UE_LOG(LogTemp,Error,TEXT("NAMEPLATES_RUNTIME_FAILED stage=%d"),*Stage); FGenericPlatformMisc::RequestExit(false);};
        const auto Labels=UDemoNameplateWidget::Gather(GetWorld());
        if(*Stage==0)
        {
            *Idle=Labels; *Valid&=Labels.Num()==6 && P->NameplateOverlay->IsInViewport() && P->NameplateOverlay->LastPaintedCount==6;
            for(int32 I=0;I<Labels.Num();++I)
                *Valid&=Labels[I].Text==(Manager->CurrentMapAsset()==TEXT("/Game/Maps/map3")?(I<3?TEXT("J-20"):TEXT("F-35")):(I<3?TEXT("J-20"):TEXT("J-10")));
        }
        else if(*Stage==1) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Nameplates/Standing.png"),true,false);
        else if(*Stage==2)
        {
            TArray<FString> Ids; for(const auto& L:Labels) Ids.Add(L.UnitId);
            if(Manager->GetUnits().IsEmpty()) {Fail(); return;}
            const FVector Start=Manager->GetUnits()[0]->GetActorLocation()/100.;
            *Valid&=Manager->EditRoutes(Ids,{Start,Start+FVector(200,0,0)},12,false);
            *Before=Manager->SnapshotJson(); W->FlightButton->OnClicked.Broadcast();
        }
        else if(*Stage==3)
        {
            *Valid&=Labels.Num()==6 && P->NameplateOverlay->LastPaintedCount==6 && Manager->SnapshotJson()==*Before;
            if(Labels.Num()==Idle->Num() && Labels.Num()) *Valid&=FVector::Distance(Labels[0].Anchor,(*Idle)[0].Anchor)>100;
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Nameplates/Moving.png"),true,false);
        }
        else if(*Stage==4)
        {
            W->FlightButton->OnClicked.Broadcast(); const auto Restored=UDemoNameplateWidget::Gather(GetWorld());
            *Valid&=Restored.Num()==Idle->Num() && Manager->SnapshotJson()==*Before;
            for(int32 I=0;I<FMath::Min(Restored.Num(),Idle->Num());++I) *Valid&=Restored[I].Anchor.Equals((*Idle)[I].Anchor,.01);
        }
        else
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Nameplates/Restored.png"),true,false);
            if(*Valid) {UE_LOG(LogTemp,Display,TEXT("NAMEPLATES_RUNTIME_VERIFIED map=%s planes=6 painted=6 follows=1 restore=1"),*Manager->CurrentMapAsset());}
            else {Fail();}
        }
        ++*Stage;
    }),3.f,true);
}
