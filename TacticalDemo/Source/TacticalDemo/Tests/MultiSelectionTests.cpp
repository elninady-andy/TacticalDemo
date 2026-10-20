#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DemoGameMode.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "DemoLayoutWidget.h"
#include "FlightPreview.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultiSelectionTest,"TacticalDemo.Selection.MultiEdit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMultiSelectionTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,CreatePackage(*(TEXT("/Temp/Multi_")+FGuid::NewGuid().ToString(EGuidFormats::Digits))),true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>(); auto* P=World->SpawnActor<ADemoPlayerController>();
    const FString Scene=TEXT(R"({"schemaVersion":2,"scenarioName":"Selection","units":[{"id":"R1","modelId":"aircraft_red_basic","type":"aircraft","side":"red","count":3,"position":{"x":1,"y":2,"z":3},"spacing":10,"instances":[{"id":"R1_001","position":{"x":1,"y":2,"z":3},"yaw":90},{"id":"R1_002","position":{"x":1,"y":12,"z":3},"yaw":45},{"id":"R1_003","position":{"x":1,"y":22,"z":3},"yaw":0,"highlight":true}]}]})");
    if(TestTrue(TEXT("Load fixture"),M->LoadJson(Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"))))
    {
        const FString Original=M->SnapshotJson(); const uint64 Revision=M->GetRevision();
        auto* First=M->FindUnit(TEXT("R1_001"));
        P->SelectUnit(First->UnitId);
        TestNotNull(TEXT("Click selection lights actual mesh"),First->Visual->GetOverlayMaterial());
        TestEqual(TEXT("Selection is display only"),M->SnapshotJson(),Original);
        TestEqual(TEXT("Selection adds no edit revision"),M->GetRevision(),Revision);
        P->SelectUnit(FString());
        TestNull(TEXT("Deselection removes transient glow"),First->Visual->GetOverlayMaterial());
        TestNotNull(TEXT("Deselection retains saved glow"),M->FindUnit(TEXT("R1_003"))->Visual->GetOverlayMaterial());
        auto* Wing=M->FindUnit(TEXT("R1_002")); const FVector WingStart=Wing->GetActorLocation(),FirstStart=First->GetActorLocation();
        const FVector Down(0,0,-1),FirstRay=FirstStart+FVector(0,0,10000),WingRay=WingStart+FVector(0,0,10000);
        TestTrue(TEXT("Ray click hits first aircraft"),P->SelectFromRay(FirstRay,Down)); P->EndUnitDrag();
        TestTrue(TEXT("Ctrl ray click adds aircraft"),P->SelectFromRay(WingRay,Down,true));
        TestEqual(TEXT("Two aircraft selected"),P->SelectedUnitIds().Num(),2);
        TestEqual(TEXT("Most recent click is primary"),P->SelectedUnitIds()[0],Wing->UnitId);
        TestNotNull(TEXT("First multi member glowing"),First->Visual->GetOverlayMaterial()); TestNotNull(TEXT("Second multi member glowing"),Wing->Visual->GetOverlayMaterial());
        P->SelectFromRay(WingRay,Down,true); TestEqual(TEXT("Ctrl click toggles off"),P->SelectedUnitIds().Num(),1); P->ToggleUnitSelection(Wing->UnitId);
        TestTrue(TEXT("Batch move and heading"),M->EditUnits(P->SelectedUnitIds(),Wing->UnitId,FVector(11,22,13),90));
        TestEqual(TEXT("Relative position retained"),Wing->GetActorLocation()-First->GetActorLocation(),WingStart-FirstStart);
        TestEqual(TEXT("Common translation"),First->GetActorLocation(),FirstStart+FVector(1000,1000,1000));
        TestTrue(TEXT("Relative headings retained"),FMath::IsNearlyEqual(First->GetActorRotation().Yaw,135.,.01));
        TestTrue(TEXT("Whole batch undo"),M->UndoEdit()); TestEqual(TEXT("Batch undo exact"),M->SnapshotJson(),Original);
        TestNotNull(TEXT("Undo preserves selection glow"),First->Visual->GetOverlayMaterial());
        TestFalse(TEXT("Unknown selection rejected atomically"),M->EditUnits({First->UnitId,TEXT("missing")},First->UnitId,FVector::ZeroVector,0));
        TestEqual(TEXT("Rejected batch unchanged"),M->SnapshotJson(),Original);
        TestTrue(TEXT("Begin multi drag"),P->BeginUnitDrag(First->UnitId,FirstRay,Down));
        P->UpdateUnitDrag(FirstRay+FVector(2000,3000,0),Down);
        TestEqual(TEXT("Drag previews both aircraft"),Wing->GetActorLocation(),WingStart+FVector(2000,3000,0));
        TestEqual(TEXT("Drag preview excluded from snapshot"),M->SnapshotJson(),Original); P->CancelUnitDrag();
        TestEqual(TEXT("Cancel returns whole batch"),Wing->GetActorLocation(),WingStart);
        P->BeginUnitDrag(First->UnitId,FirstRay,Down); P->UpdateUnitDrag(FirstRay+FVector(2000,3000,0),Down); P->EndUnitDrag();
        TestTrue(TEXT("Drag commits one batch edit"),M->UndoEdit()); TestEqual(TEXT("Drag undo exact"),M->SnapshotJson(),Original);
        P->BeginUnitDrag(First->UnitId,FirstRay,Down); M->EditUnit(Wing->UnitId,FVector(20,30,40),0);
        P->UpdateUnitDrag(FirstRay+FVector(1000,1000,0),Down);
        TestEqual(TEXT("Stale drag cannot move a newer edit"),Wing->GetActorLocation(),FVector(2000,3000,4000));
        TestFalse(TEXT("Changed scenario stops drag"),P->IsDraggingUnit()); M->UndoEdit();
        TestTrue(TEXT("Direct selection formation"),M->CreateSelectedFormation(TEXT("多选编队"),P->SelectedUnitIds()));
        TestEqual(TEXT("Selected member assigned"),M->GetScenarioConfig().Units[0].Instances[1].Formation,FString(TEXT("多选编队")));
        TestTrue(TEXT("Unselected member excluded"),M->GetScenarioConfig().Units[0].Instances[2].Formation.IsEmpty());
        TestFalse(TEXT("Existing formation not silently overwritten"),M->CreateSelectedFormation(TEXT("多选编队"),{First->UnitId}));
        M->UndoEdit();
        P->SelectFromRay(FVector(100000,100000,100000),Down);
        TestEqual(TEXT("Empty ray clears multiselection"),P->SelectedUnitIds().Num(),0); TestNull(TEXT("Clear removes selection glow"),First->Visual->GetOverlayMaterial());
        P->SelectUnit(First->UnitId);
        M->EditRoutes({First->UnitId,Wing->UnitId},{FirstStart/100.,FirstStart/100.+FVector(100,0,0)},10,false);
        auto* Preview=World->SpawnActor<ADemoFlightPreview>();
        if(TestTrue(TEXT("Edited routes play outside map3"),Preview->Start(M)))
        {
            P->SelectUnit(Wing->UnitId); Preview->AdvancePreview(.1);
            FTransform FirstFlight,WingFlight; Preview->GetPreviewTransform(First->UnitId,FirstFlight); Preview->GetPreviewTransform(Wing->UnitId,WingFlight);
            TArray<UStaticMeshComponent*> Clones; Preview->GetComponents(Clones);
            for(auto* C:Clones)
            {
                if(C->GetComponentLocation().Equals(FirstFlight.GetLocation(),.01)) TestNull(TEXT("Playback deselected clone glow removed"),C->GetOverlayMaterial());
                if(C->GetComponentLocation().Equals(WingFlight.GetLocation(),.01)) TestNotNull(TEXT("Playback newly selected clone lights"),C->GetOverlayMaterial());
            }
            Preview->Stop();
        }
        M->ClearScenario(); TestTrue(TEXT("Scene clear invalidates old actor selection"),P->SelectedUnitIds().IsEmpty());
    }
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMultiSelectionWindowTest,"TacticalDemo.Selection.Window",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMultiSelectionWindowTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,CreatePackage(*(TEXT("/Temp/MultiUI_")+FGuid::NewGuid().ToString(EGuidFormats::Digits))),true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>(); auto* P=World->SpawnActor<ADemoPlayerController>();
    const FString Scene=TEXT(R"({"scenarioName":"MultiUI","units":[{"id":"R1","modelId":"aircraft_red_basic","type":"aircraft","side":"red","count":3,"position":{"x":1,"y":2,"z":3},"spacing":10}]})");
    World->AddController(P); // Isolated test worlds do not run controller BeginPlay registration.
    M->LoadJson(Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"));
    P->SelectUnit(TEXT("R1_001")); P->ToggleUnitSelection(TEXT("R1_002"));
    auto* W=NewObject<UDemoLayoutWidget>(World); W->EnsureInitialized(); W->RefreshSelection();
    TestTrue(TEXT("Window shows multiple selection"),W->SelectionInfo->GetText().ToString().Contains(TEXT("已选2")));
    W->TransformFields[0]->SetText(FText::FromString(TEXT("21"))); const FString Before=M->SnapshotJson(); W->ApplyTransform();
    TestEqual(TEXT("UI edits primary"),M->FindUnit(TEXT("R1_002"))->GetActorLocation().X,2100.);
    TestEqual(TEXT("UI edits other selected aircraft"),M->FindUnit(TEXT("R1_001"))->GetActorLocation().X,2100.);
    TestEqual(TEXT("UI excludes unselected aircraft"),M->FindUnit(TEXT("R1_003"))->GetActorLocation().X,100.);
    W->Undo(); TestEqual(TEXT("UI batch undo exact"),M->SnapshotJson(),Before);
    W->SelectedFormationName->SetText(FText::FromString(TEXT("窗口多选编队"))); W->CreateFromSelection();
    TestEqual(TEXT("UI creates selected formation"),M->GetScenarioConfig().Units[0].Instances[1].Formation,FString(TEXT("窗口多选编队")));
    W->ClearSelectedAircraft(); TestEqual(TEXT("UI clears selection"),P->SelectedUnitIds().Num(),0);
    W->SelectAllAircraft(); TestEqual(TEXT("UI selects all aircraft"),P->SelectedUnitIds().Num(),3);
    World->DestroyWorld(false); return true;
}
#endif
