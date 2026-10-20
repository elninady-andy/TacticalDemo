#include "DemoGameMode.h"
#include "FlightPreview.h"
#include "Components/CheckBox.h"
#include "Components/CanvasPanelSlot.h"
#include "DemoControlWidget.h"
#include "DemoNameplateWidget.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "GameFramework/SpectatorPawn.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Components/EditableTextBox.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "ScenarioPresetStore.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "DemoLayoutWidget.h"
#include "SceneLibrary.h"
#include "ScenarioConfigLoader.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"

ADemoScenarioManager* ADemoPlayerController::ScenarioManager() const
{ for(TActorIterator<ADemoScenarioManager> It(GetWorld());It;++It) return *It; return nullptr; }
ADemoUnitActor* ADemoPlayerController::SelectedUnit() const
{ auto* M=ScenarioManager(); auto* U=Selection.Get(); return M && U && M->FindUnit(U->UnitId)==U?U:nullptr; }
void ADemoPlayerController::SelectUnit(const FString& Id)
{
    if(SelectedUnits.Num()==1 && Selection.IsValid() && Selection->UnitId==Id && SelectedUnit()) return;
    CancelUnitDrag(); auto* M=ScenarioManager(); Selection=M?M->FindUnit(Id):nullptr;
    SelectedUnits.Reset(); if(Selection.IsValid()) SelectedUnits.Add(Selection);
    ++SelectedRevision; RefreshSelectionVisuals();
}
bool ADemoPlayerController::IntersectDragPlane(const FVector& Origin,const FVector& Direction,double Height,FVector& Point)
{
    if(Origin.ContainsNaN() || Direction.ContainsNaN() || !FMath::IsFinite(Height) || FMath::Abs(Direction.Z)<1e-6) return false;
    const double T=(Height-Origin.Z)/Direction.Z;
    if(T<0 || !FMath::IsFinite(T)) return false;
    Point=Origin+Direction*T;
    return !Point.ContainsNaN() && Point.GetAbsMax()<=100000000;
}
bool ADemoPlayerController::BeginUnitDrag(const FString& Id,const FVector& Origin,const FVector& Direction)
{
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) It->Stop();
    CancelUnitDrag(); if(!IsUnitSelected(Id)) SelectUnit(Id);
    if(auto* M=ScenarioManager()) { if(Selection.Get()!=M->FindUnit(Id)) ++SelectedRevision; Selection=M->FindUnit(Id); }
    auto* Unit=SelectedUnit(); FVector Hit;
    if(!Unit || !IntersectDragPlane(Origin,Direction,Unit->GetActorLocation().Z,Hit)) return false;
    DragUnit=Unit; DragStart=Unit->GetActorLocation(); DragYaw=Unit->GetActorRotation().Yaw;
    DragMembers.Reset(); if(auto* M=ScenarioManager())
    {
        DragRevision=M->GetRevision();
        for(const FString& Member:SelectedUnitIds()) if(auto* U=M->FindUnit(Member)) DragMembers.Add({U,U->GetActorLocation(),U->GetActorRotation().Yaw});
    }
    DragOffset=DragStart-Hit; return true;
}
void ADemoPlayerController::UpdateUnitDrag(const FVector& Origin,const FVector& Direction)
{
    auto* M=ScenarioManager();
    if(DragUnit.IsValid() && (!M || M->GetRevision()!=DragRevision)) { CancelUnitDrag(); return; }
    FVector Hit;
    if(DragUnit.IsValid() && IntersectDragPlane(Origin,Direction,DragStart.Z,Hit))
    {
        const FVector Delta=Hit+DragOffset-DragStart;
        for(const auto& Member:DragMembers) if((Member.Position+Delta).GetAbsMax()>100000000) return;
        for(const auto& Member:DragMembers) if(auto* U=Member.Actor.Get()) U->SetActorLocation(Member.Position+Delta);
    }
}
void ADemoPlayerController::CancelUnitDrag()
{
    auto* M=ScenarioManager();
    if(M && M->GetRevision()==DragRevision)
        for(const auto& Member:DragMembers) if(auto* U=Member.Actor.Get()) U->SetActorLocationAndRotation(Member.Position,FRotator(0,Member.Yaw,0));
    DragUnit.Reset(); DragMembers.Reset();
}
void ADemoPlayerController::EndUnitDrag()
{
    if(!DragUnit.IsValid()) return;
    const FString Id=DragUnit->UnitId; const FVector Position=DragUnit->GetActorLocation()/100.;
    const double Yaw=DragYaw; const TArray<FString> Ids=SelectedUnitIds();
    const bool Moved=!DragStart.Equals(Position*100.,1.); auto* M=ScenarioManager();
    const bool Current=M && M->GetRevision()==DragRevision; CancelUnitDrag();
    if(!Current) return;
    if(Moved) M->EditUnits(Ids,Id,Position,Yaw);
    else if(Ids.Num()>1) SelectUnit(Id);
}
void ADemoPlayerController::SelectStart()
{
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) if(It->IsPlaying()) return;
    if(!bShowMouseCursor) return;
    if(ControlWindow && ControlWindow->BlocksScenePointer(FSlateApplication::Get().GetCursorPos())) return;
    FVector Origin,Direction;
    if(!DeprojectMousePositionToWorld(Origin,Direction)) return;
    SelectFromRay(Origin,Direction,IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl));
}
bool ADemoPlayerController::SelectFromRay(const FVector& Origin,const FVector& Direction,bool Additive)
{
    auto* M=ScenarioManager(); if(!M || Origin.ContainsNaN() || Direction.ContainsNaN() || Direction.IsNearlyZero()) return false;
    ADemoUnitActor* Pick=nullptr; double Nearest=TNumericLimits<double>::Max();
    // Units deliberately have no physics collision. Pick their transformed mesh bounds.
    for(ADemoUnitActor* Unit:M->GetUnits())
    {
        if(!IsValid(Unit) || !Unit->Visual->GetStaticMesh()) continue;
        const FTransform Transform=Unit->Visual->GetComponentTransform();
        const FVector O=Transform.InverseTransformPosition(Origin),D=Transform.InverseTransformVector(Direction);
        const FBox Box=Unit->Visual->GetStaticMesh()->GetBoundingBox();
        double Entry=0,Exit=TNumericLimits<double>::Max(); bool Hit=true;
        for(int32 Axis=0;Axis<3;++Axis)
        {
            if(FMath::Abs(D[Axis])<1e-12) { if(O[Axis]<Box.Min[Axis] || O[Axis]>Box.Max[Axis]) Hit=false; continue; }
            double A=(Box.Min[Axis]-O[Axis])/D[Axis],B=(Box.Max[Axis]-O[Axis])/D[Axis];
            if(A>B) Swap(A,B); Entry=FMath::Max(Entry,A); Exit=FMath::Min(Exit,B);
            if(Entry>Exit) Hit=false;
        }
        if(Hit && Exit>=0 && Entry<Nearest) { Nearest=Entry; Pick=Unit; }
    }
    if(Pick) { if(Additive) ToggleUnitSelection(Pick->UnitId); else BeginUnitDrag(Pick->UnitId,Origin,Direction); return true; }
    if(!Additive) SelectUnit(FString()); return false;
}
void ADemoPlayerController::SelectEnd() { EndUnitDrag(); }
void ADemoPlayerController::Escape()
{
    CancelUnitDrag();
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) It->Stop();
}
void ADemoPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    bool Playing=false;
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) Playing|=It->IsPlaying();
    if(Playing && GetPawn())
    {
        if(!bFollowingFlight) { PreFlightView=GetPawn()->GetActorTransform(); PreFlightRotation=GetControlRotation(); }
        bFollowingFlight=true;
        if(auto* Mode=GetWorld()->GetAuthGameMode<ADemoGameMode>()) Mode->FrameScene(ScenarioManager());
        return;
    }
    if(bFollowingFlight)
    {
        if(GetPawn()) { GetPawn()->SetActorTransform(PreFlightView); SetControlRotation(PreFlightRotation); }
        bFollowingFlight=false;
    }
    if(DragUnit.IsValid())
    {
        if(!IsInputKeyDown(EKeys::LeftMouseButton)) EndUnitDrag();
        else { FVector O,D; if(DeprojectMousePositionToWorld(O,D)) UpdateUnitDrag(O,D); }
    }
    const auto Ids=SelectedUnitIds();
    if(Ids.Num()!=SelectedUnits.Num())
    {
        SelectedUnits.RemoveAll([&](const TWeakObjectPtr<ADemoUnitActor>& U){auto* M=ScenarioManager(); return !U.IsValid() || !M || M->FindUnit(U->UnitId)!=U.Get();});
        if(!SelectedUnit()) Selection=SelectedUnits.Num()?SelectedUnits[0]:TWeakObjectPtr<ADemoUnitActor>();
        ++SelectedRevision; RefreshSelectionVisuals();
    }
    for(const FString& Id:Ids) if(auto* Unit=ScenarioManager()->FindUnit(Id))
    {
        const FBox Bounds=Unit->GetComponentsBoundingBox(true);
        DrawDebugBox(GetWorld(),Bounds.GetCenter(),Bounds.GetExtent()+FVector(15),FColor::Yellow,false,0,0,4);
        DrawDebugDirectionalArrow(GetWorld(),Unit->GetActorLocation()+FVector(0,0,100),Unit->GetActorLocation()+FVector(0,0,100)+Unit->GetActorForwardVector()*1200,150,FColor::Yellow,false,0,0,4);
    }
}

void ADemoPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ADemoPlayerController::LookStart);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ADemoPlayerController::LookEnd);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&ADemoPlayerController::SelectStart);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&ADemoPlayerController::SelectEnd);
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&ADemoPlayerController::Escape);
}
void ADemoPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if(IsLocalController())
    {
        NameplateOverlay=CreateWidget<UDemoNameplateWidget>(this,UDemoNameplateWidget::StaticClass());
        if(NameplateOverlay) NameplateOverlay->AddToViewport(-10);
        ControlWindow=CreateWidget<UDemoControlWidget>(this,UDemoControlWidget::StaticClass());
        if(ControlWindow) ControlWindow->AddToViewport();
        LookEnd();
    }
}
void ADemoPlayerController::LookStart()
{
    CancelUnitDrag();
    bShowMouseCursor=false;
    ResetIgnoreLookInput(); ResetIgnoreMoveInput();
    SetInputMode(FInputModeGameOnly());
}
void ADemoPlayerController::LookEnd()
{
    bShowMouseCursor=true;
    ResetIgnoreLookInput(); ResetIgnoreMoveInput();
    SetIgnoreLookInput(true); SetIgnoreMoveInput(true);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}
void ADemoPlayerController::Reload()
{
    for (TActorIterator<ADemoScenarioManager> It(GetWorld()); It; ++It) It->ReloadScenario();
}
void ADemoPlayerController::Frame()
{
    if (ADemoGameMode* Mode = GetWorld()->GetAuthGameMode<ADemoGameMode>())
        for (TActorIterator<ADemoScenarioManager> It(GetWorld()); It; ++It) Mode->FrameScene(*It);
}
ADemoGameMode::ADemoGameMode()
{
    DefaultPawnClass = ASpectatorPawn::StaticClass();
    PlayerControllerClass = ADemoPlayerController::StaticClass();
}
void ADemoGameMode::BeginPlay()
{
    Super::BeginPlay();
    ADemoScenarioManager* Manager = GetWorld()->SpawnActor<ADemoScenarioManager>();
    GetWorld()->SpawnActor<ADemoFlightPreview>();
    SetupHighlightVerification(Manager);
    SetupSelectionVerification(Manager);
    SetupNameplateVerification(Manager);
    const FString PresetId=UGameplayStatics::ParseOption(OptionsString,TEXT("LayoutPreset"));
    if(!PresetId.IsEmpty())
    {
        FString Json,Error;
        if(!FScenarioPresetStore().Read(PresetId,Json,Error) || !Manager->LoadJson(Json,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")))
        { UE_LOG(LogTemp,Warning,TEXT("LAYOUT_PRESET_RESTORE_FAILED %s %s"),*Error,*Manager->GetLastError()); }
        else { UE_LOG(LogTemp,Display,TEXT("LAYOUT_PRESET_RESTORED %s"),*PresetId); }
    }
    // Next tick ensures the default spectator has been possessed before framing.
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Manager]() { FrameScene(Manager); }));
    const bool VerifyLayout=FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyLayout"));
    if(FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyFormationLanguage")))
    {
        auto Stage=MakeShared<int32>(0);
        auto Before=MakeShared<FString>();
        auto Assigned=MakeShared<FString>();
        auto Valid=MakeShared<bool>(true);
        FTimerHandle Check;
        GetWorldTimerManager().SetTimer(Check,FTimerDelegate::CreateWeakLambda(this,[this,Manager,Stage,Before,Assigned,Valid]()
        {
            auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController());
            auto* W=P?P->ControlWindow.Get():nullptr;
            if(!W || !W->Input || *Stage>4) return;
            if(*Stage==0)
            {
                *Before=Manager->SnapshotJson();
                W->Input->SetText(FText::FromString(TEXT("把红方飞机编成编队3"))); W->Send();
                *Valid&=Manager->GetUnits().Num()==6 && Manager->SnapshotJson()==*Before && !W->Candidate.IsEmpty()
                    && W->bFormationMembershipOnly && !W->ActiveRequest.IsValid();
            }
            else if(*Stage==1)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("FormationLanguage/CreationCandidate.png"),true,false);
            }
            else if(*Stage==2)
            {
                W->Apply(); *Assigned=Manager->SnapshotJson();
                int32 Members=0;
                for(const auto& G:Manager->GetScenarioConfig().Units) for(const auto& I:G.Instances)
                    if(I.Formation==TEXT("编队3")) { ++Members; *Valid&=G.Side==TEXT("red"); }
                *Valid&=Members==3 && Manager->GetUnits().Num()==6;
                *Valid&=Manager->UndoEdit() && Manager->SnapshotJson()==*Before;
                *Valid&=Manager->RedoEdit() && Manager->SnapshotJson()==*Assigned;
                W->Input->SetText(FText::FromString(TEXT("把编队3排成V形，间距50米"))); W->Send();
                *Valid&=!W->Candidate.IsEmpty() && !W->bFormationMembershipOnly && Manager->SnapshotJson()==*Assigned;
            }
            else if(*Stage==3)
            {
                W->Apply(); const FString Arranged=Manager->SnapshotJson();
                *Valid&=Arranged!=*Assigned && Manager->UndoEdit() && Manager->SnapshotJson()==*Assigned;
                *Valid&=Manager->RedoEdit() && Manager->SnapshotJson()==Arranged;
                W->Input->SetText(FText::FromString(TEXT("查看编队列表"))); W->Send(); FrameScene(Manager);
                if(*Valid) { UE_LOG(LogTemp,Display,TEXT("FORMATION_LANGUAGE_RUNTIME_VERIFIED map=%s redMembers=3 aircraft=6 manualApply=1 undoRedo=1 local=1"),*Manager->CurrentMapAsset()); }
                else { UE_LOG(LogTemp,Error,TEXT("FORMATION_LANGUAGE_RUNTIME_FAILED")); }
            }
            else FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("FormationLanguage/Arranged.png"),true,false);
            ++*Stage;
        }),3.f,true);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyFlightPreview")))
    {
        auto Stage=MakeShared<int32>(0);
        auto Before=MakeShared<FString>();
        auto StartCenter=MakeShared<FVector>(FVector::ZeroVector);
        auto Revision=MakeShared<uint64>(0);
        FTimerHandle VerifyTimer;
        GetWorldTimerManager().SetTimer(VerifyTimer,FTimerDelegate::CreateWeakLambda(this,[this,Manager,Stage,Before,StartCenter,Revision]()
        {
            auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController());
            auto* W=P?P->ControlWindow.Get():nullptr;
            ADemoFlightPreview* Preview=nullptr;
            for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) { Preview=*It; break; }
            if(!W || !Preview || !W->FlightButton) return;
            if(*Stage==0)
            {
                *Before=Manager->SnapshotJson(); *Revision=Manager->GetRevision();
                bool Valid=Manager->CurrentMapAsset()==TEXT("/Game/Maps/map3") && Manager->GetUnits().Num()==6 && !Preview->IsPlaying();
                for(ADemoUnitActor* Unit:Manager->GetUnits()) Valid&=Unit->Visual->GetStaticMesh() && Unit->Visual->GetStaticMesh()->GetName()==(Unit->Side==TEXT("red")?TEXT("SM_J20_Map3"):TEXT("SM_F35"));
                W->FlightButton->OnClicked.Broadcast();
                Valid&=Preview->IsPlaying(); *StartCenter=Preview->GetPreviewBounds().GetCenter();
                if(!Valid) { UE_LOG(LogTemp,Error,TEXT("FLIGHT_RUNTIME_FAILED start")); FGenericPlatformMisc::RequestExit(false); }
            }
            if(*Stage==2)
            {
                const double Travel=FVector::Distance(*StartCenter,Preview->GetPreviewBounds().GetCenter());
                const bool Valid=Preview->IsPlaying() && Travel>1000 && *Before==Manager->SnapshotJson() && *Revision==Manager->GetRevision();
                if(Valid) { UE_LOG(LogTemp,Display,TEXT("FLIGHT_RUNTIME_MOVING distanceCm=%.2f snapshotUnchanged=1"),Travel); }
                else { UE_LOG(LogTemp,Error,TEXT("FLIGHT_RUNTIME_FAILED movement or snapshot")); }
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("MoveImport/FlightMoving.png"),true,false);
            }
            if(*Stage==3)
            {
                W->Units->SetSelectedOption(TEXT("BLUE01_1"));
                // Slate's programmatic selection notification may be deferred; exercise the UI delegate explicitly.
                W->Units->OnSelectionChanged.Broadcast(TEXT("BLUE01_1"),ESelectInfo::Direct);
                if(!P->SelectedUnit() || P->SelectedUnit()->UnitId!=TEXT("BLUE01_1"))
                { UE_LOG(LogTemp,Error,TEXT("FLIGHT_RUNTIME_FAILED follow selection")); }
                FrameScene(Manager);
            }
            if(*Stage==4)
            {
                if(!P->SelectedUnit() || P->SelectedUnit()->UnitId!=TEXT("BLUE01_1"))
                { UE_LOG(LogTemp,Error,TEXT("FLIGHT_RUNTIME_FAILED follow selection did not persist")); }
                else { UE_LOG(LogTemp,Display,TEXT("FLIGHT_FOLLOW_VERIFIED BLUE01_1")); }
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("MoveImport/FlightF35.png"),true,false);
            }
            if(*Stage==5)
            {
                W->FlightButton->OnClicked.Broadcast();
                bool Valid=!Preview->IsPlaying() && *Before==Manager->SnapshotJson() && *Revision==Manager->GetRevision();
                for(ADemoUnitActor* Unit:Manager->GetUnits()) Valid&=Unit->Visual->IsVisible();
                if(Valid) { UE_LOG(LogTemp,Display,TEXT("FLIGHT_RUNTIME_VERIFIED stop restores originals; snapshot and revision unchanged")); }
                else { UE_LOG(LogTemp,Error,TEXT("FLIGHT_RUNTIME_FAILED stop")); }
            }
            ++*Stage;
        }),3.f,true);
    }
    const bool RestoreLayout=FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyLayoutRestore"));
    if(VerifyLayout || RestoreLayout)
    {
        FTimerHandle OpenTimer;
        GetWorldTimerManager().SetTimer(OpenTimer,FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController());
            if(P && P->ControlWindow && P->ControlWindow->LayoutWindow->GetVisibility()==ESlateVisibility::Collapsed) P->ControlWindow->ToggleLayout();
        }),1.f,false);
        FTimerHandle LayoutTimer;
        GetWorldTimerManager().SetTimer(LayoutTimer,FTimerDelegate::CreateWeakLambda(this,[this,Manager,RestoreLayout]()
        {
            auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController());
            auto* W=P?P->ControlWindow.Get():nullptr;
            const FString Evidence=FPaths::ProjectSavedDir()/TEXT("Evidence/LayoutAcceptance.json");
            auto Fail=[](const FString& Why) { UE_LOG(LogTemp,Error,TEXT("LAYOUT_RUNTIME_FAILED %s"),*Why); FGenericPlatformMisc::RequestExit(false); };
            if(!W || !W->LayoutWindow) { Fail(TEXT("missing editor window")); return; }
            auto* Editor=W->LayoutWindow.Get();
            const auto& PanelGeometry=Editor->GetCachedGeometry();
            const FVector2D PanelCenter=PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize()*.5f);
            if(PanelGeometry.GetLocalSize().IsNearlyZero() || !W->BlocksScenePointer(PanelCenter)) { Fail(TEXT("editor panel has not been arranged or does not block scene input")); return; }
            if(RestoreLayout)
            {
                FString Text; TSharedPtr<FJsonObject> Record;
                if(!FFileHelper::LoadFileToString(Text,*Evidence) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Record)) { Fail(TEXT("missing acceptance record")); return; }
                const FString Id=Record->GetStringField(TEXT("presetId"));
                if(!Manager->CurrentMapAsset().Contains(TEXT("VRAirportMap")))
                {
                    Editor->RefreshPresets();
                    for(int32 I=0;I<Editor->Presets.Num();++I) if(Editor->Presets[I].Id==Id)
                    { Editor->PresetPicker->SetSelectedIndex(I); Editor->LoadPreset(); return; }
                    Fail(TEXT("saved preset missing")); return;
                }
                FString Json; FJsonSerializer::Serialize(Record->GetObjectField(TEXT("expected")).ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
                FDemoScenarioConfig Expected; FString Error;
                if(!FScenarioConfigLoader::Parse(Json,Expected,Error)) { Fail(Error); return; }
                int32 Count=0;
                for(const auto& Group:Expected.Units) for(const auto& Instance:Group.Instances)
                {
                    auto* Unit=Manager->FindUnit(Instance.Id); ++Count;
                    if(!Unit || !Unit->GetActorLocation().Equals(Instance.PositionMeters*100.,.01) || FMath::Abs(FRotator::NormalizeAxis(Unit->GetActorRotation().Yaw-Instance.Yaw))>.001)
                    { Fail(TEXT("restart transform mismatch")); return; }
                }
                if(Count!=6 || Manager->GetUnits().Num()!=6 || Manager->GetScenarioName()!=Expected.Name) { Fail(TEXT("restart count or name mismatch")); return; }
                for(const auto& Group:Manager->GetScenarioConfig().Units)
                    if(Group.TextureId!=TEXT("1") || (Group.Side==TEXT("red")?Group.ModelId!=TEXT("J20"):Group.ModelId!=TEXT("J10"))) { Fail(TEXT("restart assets mismatch")); return; }
                UE_LOG(LogTemp,Display,TEXT("LAYOUT_RESTART_VERIFIED map travel restored six aircraft, transforms and assets")); return;
            }
            if(Manager->GetUnits().Num()!=6) { Fail(TEXT("expected six imported aircraft")); return; }
            auto* Unit=Manager->GetUnits()[0].Get(); const FString Id=Unit->UnitId;
            TArray<FVector> Others; for(int32 I=1;I<6;++I) Others.Add(Manager->GetUnits()[I]->GetActorLocation());
            const FVector Before=Unit->GetActorLocation()/100.;
            Editor->UnitPicker->SetSelectedOption(Id); Editor->RefreshSelection();
            const double Values[]={Before.X+8,Before.Y+4,Before.Z,45};
            for(int32 I=0;I<4;++I) Editor->TransformFields[I]->SetText(FText::FromString(FString::Printf(TEXT("%.6f"),Values[I])));
            Editor->ApplyTransform();
            if(!Unit->GetActorLocation().Equals(FVector(Values[0],Values[1],Values[2])*100.,.01) || FMath::Abs(Unit->GetActorRotation().Yaw-45)>.001) { Fail(TEXT("editor transform mismatch")); return; }
            Editor->UndoButton->OnClicked.Broadcast(); Editor->RedoButton->OnClicked.Broadcast();
            Editor->Close();
            const FVector Start=Unit->GetActorLocation(),Origin=Start+FVector(0,0,10000),Direction(0,0,-1);
            if(!P->BeginUnitDrag(Id,Origin,Direction)) { Fail(TEXT("drag start rejected")); return; }
            P->UpdateUnitDrag(Origin+FVector(500,600,0),Direction); P->CancelUnitDrag();
            if(!Unit->GetActorLocation().Equals(Start,.01)) { Fail(TEXT("cancel did not restore preview")); return; }
            P->BeginUnitDrag(Id,Origin,Direction); P->UpdateUnitDrag(Origin+FVector(500,600,0),Direction); P->EndUnitDrag();
            if(!Unit->GetActorLocation().Equals(Start+FVector(500,600,0),.01)) { Fail(TEXT("drag commit mismatch")); return; }
            for(int32 I=1;I<6;++I) if(!Manager->GetUnits()[I]->GetActorLocation().Equals(Others[I-1],.01)) { Fail(TEXT("other aircraft moved")); return; }
            W->ToggleLayout(); Editor->RefreshSelection();
            if(FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyFormation")))
            {
                Editor->ShowFormations();
                if(Editor->MemberChecks.Num()!=6) { Fail(TEXT("formation member list missing")); return; }
                for(int32 I=0;I<6;++I) Editor->MemberChecks[I]->SetIsChecked(I==0 || I==3 || I==4);
                Editor->SaveFormation();
                Editor->PatternPicker->SetSelectedIndex(2);
                const TCHAR* Settings[]={TEXT("550"),TEXT("-515"),TEXT("35"),TEXT("0")};
                for(int32 I=0;I<4;++I) Editor->FormationFields[I]->SetText(FText::FromString(Settings[I]));
                Editor->ApplyFormation();
                if(!Manager->GetUnits()[3]->GetActorLocation().Equals(FVector(51500,-55000,Others[2].Z),.01)
                    || !Manager->GetUnits()[4]->GetActorLocation().Equals(FVector(51500,-48000,Others[3].Z),.01))
                { Fail(TEXT("formation wedge mismatch")); return; }
                for(int32 I:{1,2,5}) if(!Manager->GetUnits()[I]->GetActorLocation().Equals(Others[I-1],.01)) { Fail(TEXT("formation moved nonmembers")); return; }
                if(!Manager->SetFormationMembers(TEXT("编队2"),{Manager->GetUnits()[1]->UnitId,Manager->GetUnits()[2]->UnitId,Manager->GetUnits()[5]->UnitId}))
                { Fail(TEXT("second formation assignment failed")); return; }
                Editor->RefreshSelection();
                UE_LOG(LogTemp,Display,TEXT("FORMATION_RUNTIME_VERIFIED mixed-side wedge, preserved heights and other aircraft, independent second formation"));
            }
            Editor->PresetName->SetText(FText::FromString(TEXT("阶段1验证方案-")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
            Editor->SavePreset(); const int32 Index=Editor->PresetPicker->GetSelectedIndex();
            if(!Editor->Presets.IsValidIndex(Index)) { Fail(TEXT("save failed")); return; }
            const FString PresetId=Editor->Presets[Index].Id;
            Editor->Undo(); Editor->LoadPreset();
            auto Record=MakeShared<FJsonObject>(); Record->SetStringField(TEXT("presetId"),PresetId);
            TSharedPtr<FJsonObject> Snapshot; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Manager->SnapshotJson()),Snapshot);
            Record->SetObjectField(TEXT("expected"),Snapshot); FString Text;
            FJsonSerializer::Serialize(Record,TJsonWriterFactory<>::Create(&Text));
            if(!FFileHelper::SaveStringToFile(Text,*Evidence,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { Fail(TEXT("evidence save failed")); return; }
            UE_LOG(LogTemp,Display,TEXT("LAYOUT_RUNTIME_VERIFIED edit, undo, redo, drag cancel/commit, save and load on airport"));
        }),6.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyFloating")))
    {
        FTimerHandle Open;
        GetWorldTimerManager().SetTimer(Open,FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController());
            if(P && P->ControlWindow) P->ControlWindow->ToggleLayout();
        }),1.f,false);
        FTimerHandle Check;
        GetWorldTimerManager().SetTimer(Check,FTimerDelegate::CreateWeakLambda(this,[this,Manager]()
        {
            auto* P=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController()); auto* Main=P?P->ControlWindow.Get():nullptr;
            auto* Editor=Main?Main->LayoutWindow.Get():nullptr;
            auto Fail=[](const TCHAR* Why){UE_LOG(LogTemp,Error,TEXT("FLOATING_RUNTIME_FAILED %s"),Why); FGenericPlatformMisc::RequestExit(false);};
            if(!Editor || !Editor->GetParent()) { Fail(TEXT("window missing")); return; }
            const FString Scene=Manager->SnapshotJson(); const uint64 Revision=Manager->GetRevision();
            const TSet<FKey> Pressed={EKeys::LeftMouseButton}; const TSet<FKey> Released;
            auto Event=[&](const FVector2D& Point,bool Down) { return FPointerEvent(0,Point,Point,Down?Pressed:Released,EKeys::LeftMouseButton,0,FModifierKeysState()); };
            const auto G=Editor->GetCachedGeometry();
            const FVector2D ClosePoint=Editor->HeaderCloseButton->GetCachedGeometry().LocalToAbsolute(Editor->HeaderCloseButton->GetCachedGeometry().GetLocalSize()*.5f);
            Editor->NativeOnMouseButtonDown(G,Event(ClosePoint,true));
            if(Editor->bDraggingWindow) { Fail(TEXT("close button starts window drag")); return; }
            const FVector2D InputPoint=Editor->TransformFields[0]->GetCachedGeometry().LocalToAbsolute(Editor->TransformFields[0]->GetCachedGeometry().GetLocalSize()*.5f);
            Editor->NativeOnMouseButtonDown(G,Event(InputPoint,true));
            if(Editor->bDraggingWindow) { Fail(TEXT("input field starts window drag")); return; }
            const auto Parent=Editor->GetParent()->GetCachedGeometry();
            const FVector2D Start=Editor->DragTitle->GetCachedGeometry().LocalToAbsolute(Editor->DragTitle->GetCachedGeometry().GetLocalSize()*FVector2D(.3,.5));
            if(!Editor->NativeOnMouseButtonDown(G,Event(Start,true)).IsEventHandled() || !Editor->bDraggingWindow) { Fail(TEXT("title does not start dragging")); return; }
            const FVector2D End=Parent.LocalToAbsolute(Parent.AbsoluteToLocal(Start)+FVector2D(-120,-40));
            Editor->NativeOnMouseMove(G,Event(End,true)); Editor->NativeOnMouseButtonUp(G,Event(End,false));
            auto* PanelSlot=Cast<UCanvasPanelSlot>(Editor->Slot);
            if(!PanelSlot || !PanelSlot->GetPosition().Equals(FVector2D(-120,-40),.01) || Editor->bDraggingWindow) { Fail(TEXT("title drag displacement or release mismatch")); return; }
            Editor->Close(); Editor->SetVisibility(ESlateVisibility::Visible);
            FTimerHandle Arranged;
            GetWorldTimerManager().SetTimer(Arranged,FTimerDelegate::CreateWeakLambda(this,[Main,Editor,Manager,Scene,Revision]()
            {
                const FVector2D Center=Editor->GetCachedGeometry().LocalToAbsolute(Editor->GetCachedGeometry().GetLocalSize()*.5f);
                if(!Main->BlocksScenePointer(Center) || Manager->SnapshotJson()!=Scene || Manager->GetRevision()!=Revision)
                { UE_LOG(LogTemp,Error,TEXT("FLOATING_RUNTIME_FAILED moved window does not block scene input or scene changed")); return; }
                UE_LOG(LogTemp,Display,TEXT("FLOATING_RUNTIME_VERIFIED title drag, close/input exclusion, release, reopen position and scene input blocking"));
            }),1.f,false);
        }),4.f,false);
    }
    // Opt-in runtime acceptance: real widget -> local Qwen -> Apply -> map travel.
    const bool VerifySwitch=FParse::Param(FCommandLine::Get(),TEXT("DemoVerifySceneSwitch"));
    FString TargetScene=TEXT("airport"),TargetAsset,TargetError;
    if(VerifySwitch)
    {
        FParse::Value(FCommandLine::Get(),TEXT("DemoSceneTarget="),TargetScene);
        if(!FDemoSceneLibrary::Resolve(TargetScene,TargetAsset,TargetError))
        { UE_LOG(LogTemp,Error,TEXT("SCENE_SWITCH_VERIFY_FAILED %s"),*TargetError); FGenericPlatformMisc::RequestExit(false); return; }
    }
    if(VerifySwitch && Manager->CurrentMapAsset()!=TargetAsset)
    {
        const double Deadline=FPlatformTime::Seconds()+300;
        auto Sent=MakeShared<bool>(false);
        const FString Before=Manager->SnapshotJson();
        GetWorldTimerManager().SetTimer(SceneVerifyTimer,FTimerDelegate::CreateWeakLambda(this,[this,Manager,Sent,Deadline,TargetScene,Before]()
        {
            auto* Player=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController());
            auto* Window=Player?Player->ControlWindow.Get():nullptr;
            if(!Window || FPlatformTime::Seconds()>Deadline)
            { UE_LOG(LogTemp,Error,TEXT("SCENE_SWITCH_VERIFY_FAILED timeout or missing UI")); FGenericPlatformMisc::RequestExit(false); return; }
            if(!*Sent)
            { Window->Input->SetText(FText::FromString(TEXT("切换到")+(TargetScene==TEXT("airport")?FString(TEXT("机场")):TargetScene)+TEXT("场景"))); Window->Send(); *Sent=true; return; }
            if(Window->ActiveRequest.IsValid()) return;
            if(Window->CandidateLevel!=TargetScene || Manager->SnapshotJson()!=Before)
            { UE_LOG(LogTemp,Error,TEXT("SCENE_SWITCH_VERIFY_FAILED invalid candidate or scene changed before Apply")); FGenericPlatformMisc::RequestExit(false); return; }
            UE_LOG(LogTemp,Display,TEXT("SCENE_SWITCH_CANDIDATE_VERIFIED %s awaits Apply; scene unchanged"),*TargetScene);
            GetWorldTimerManager().ClearTimer(SceneVerifyTimer);
            Window->Apply();
        }),1.f,true);
        return;
    }
    if(VerifySwitch)
    {
        bool Valid=Manager->GetUnits().Num()>0;
        for(ADemoUnitActor* Unit:Manager->GetUnits())
            Valid &= Unit->Visual->GetStaticMesh()!=nullptr;
        if(TargetScene==TEXT("airport"))
        {
            Valid &= Manager->GetUnits().Num()==6;
            for(ADemoUnitActor* Unit:Manager->GetUnits())
                Valid &= Unit->Visual->GetStaticMesh() && Unit->Visual->GetStaticMesh()->GetName()==(Unit->Side==TEXT("red")?TEXT("J20S"):TEXT("J10B"));
        }
        if(Valid) { UE_LOG(LogTemp,Display,TEXT("SCENE_SWITCH_VERIFIED %s units=%d map=%s"),*TargetScene,Manager->GetUnits().Num(),*Manager->CurrentMapAsset()); }
        else { UE_LOG(LogTemp,Error,TEXT("SCENE_SWITCH_VERIFY_FAILED target units incorrect")); }
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("DemoVerifyTextures")))
    {
        auto Stage=MakeShared<int32>(0);
        auto Waiting=MakeShared<bool>(false);
        const double Deadline=FPlatformTime::Seconds()+300;
        GetWorldTimerManager().SetTimer(SceneVerifyTimer,FTimerDelegate::CreateWeakLambda(this,[this,Manager,Stage,Waiting,Deadline]()
        {
            auto* Player=Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController());
            auto* Window=Player?Player->ControlWindow.Get():nullptr;
            if(!Window || FPlatformTime::Seconds()>Deadline)
            { UE_LOG(LogTemp,Error,TEXT("TEXTURE_RUNTIME_VERIFY_FAILED timeout or missing UI")); FGenericPlatformMisc::RequestExit(false); return; }
            if(!*Waiting)
            { Window->Input->SetText(FText::FromString(*Stage==0?TEXT("把红方飞机的贴图换成第二款"):TEXT("把蓝方飞机的贴图换成第二款"))); Window->Send(); *Waiting=true; return; }
            if(Window->ActiveRequest.IsValid()) return;
            if(Window->Candidate.IsEmpty())
            { UE_LOG(LogTemp,Error,TEXT("TEXTURE_RUNTIME_VERIFY_FAILED no texture candidate")); FGenericPlatformMisc::RequestExit(false); return; }
            Window->Apply();
            if(++*Stage<2){*Waiting=false;return;}
            bool Valid=Manager->GetUnits().Num()==6;
            for(ADemoUnitActor* Unit:Manager->GetUnits())
            {
                auto* Material=Cast<UMaterialInstanceDynamic>(Unit->Visual->GetMaterial(0));
                UTexture* Texture=Material?Material->K2_GetTextureParameterValue(TEXT("AircraftDiffuse")):nullptr;
                Valid &= Texture && Texture->GetName()==(Unit->Side==TEXT("red")?TEXT("mat_J20_Airframe_Diffuse2"):TEXT("j10b_d2"));
            }
            if(Valid){UE_LOG(LogTemp,Display,TEXT("TEXTURE_RUNTIME_VERIFIED both aircraft models use second texture"));}
            else{UE_LOG(LogTemp,Error,TEXT("TEXTURE_RUNTIME_VERIFY_FAILED wrong material binding"));}
            Window->Input->SetText(FText::FromString(TEXT("把红方飞机的贴图换成第一款")));
            GetWorldTimerManager().ClearTimer(SceneVerifyTimer);
        }),1.f,true);
    }
    FString Screenshot;
    if (FParse::Value(FCommandLine::Get(), TEXT("DemoScreenshot="), Screenshot))
    {
        float Delay=8.f;
        FParse::Value(FCommandLine::Get(),TEXT("DemoScreenshotDelay="),Delay);
        FTimerHandle Timer;
        GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Screenshot]()
        {
            FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
        }), FMath::Clamp(Delay,1.f,300.f), false);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("DemoAutoExit")))
    {
        float Delay=14.f;
        FParse::Value(FCommandLine::Get(),TEXT("DemoExitDelay="),Delay);
        FTimerHandle Timer;
        GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([]() { FGenericPlatformMisc::RequestExit(false); }), FMath::Clamp(Delay,2.f,360.f), false);
    }
}
void ADemoGameMode::FrameScene(ADemoScenarioManager* Manager)
{
    APlayerController* Player = GetWorld()->GetFirstPlayerController();
    if (!Manager || !Player || !Player->GetPawn()) return;
    FBox Bounds(ForceInit);
    // Demo units intentionally have no collision; include their visible mesh bounds.
    for (ADemoUnitActor* Unit : Manager->GetUnits()) if (IsValid(Unit)) Bounds += Unit->GetComponentsBoundingBox(true);
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) if(It->IsPlaying())
    {
        const auto* Controller=Cast<ADemoPlayerController>(Player);
        const auto* Selected=Controller?Controller->SelectedUnit():nullptr;
        Bounds=It->GetPreviewBounds(Selected?Selected->UnitId:FString());
        break;
    }
    const FVector Center = Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
    const double Radius = Bounds.IsValid ? FMath::Max(700.0, Bounds.GetExtent().Size()) : 700.0;
    const FVector Location = Center + FVector(-1.6, -1.6, 1.8)*Radius;
    Player->GetPawn()->SetActorLocation(Location);
    Player->SetControlRotation((Center-Location).Rotation());
}
