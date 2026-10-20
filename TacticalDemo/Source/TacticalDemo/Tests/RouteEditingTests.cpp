#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ScenarioConfigLoader.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "FlightPreview.h"
#include "DemoRouteWidget.h"
#include "DemoGameMode.h"
#include "Components/EditableTextBox.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteEditingTest,"TacticalDemo.Routes.AtomicEditingAndPreview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRouteEditingTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,CreatePackage(*(TEXT("/Temp/RouteEdit_")+FGuid::NewGuid().ToString(EGuidFormats::Digits))),true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>(); auto* Preview=World->SpawnActor<ADemoFlightPreview>();
    const FString Scene=TEXT(R"({"scenarioName":"Routes","units":[{"id":"R","modelId":"aircraft_red_basic","type":"aircraft","side":"red","count":3,"position":{"x":10,"y":20,"z":30},"spacing":10}]})");
    if(!TestTrue(TEXT("Load old schema without custom route"),M->LoadJson(Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")))) { World->DestroyWorld(false); return false; }
    const FString Before=M->SnapshotJson(); const TArray<FString> Ids={TEXT("R_001"),TEXT("R_002")};
    const TArray<FVector> Points={FVector(100,20,30),FVector(200,20,30)};
    TestFalse(TEXT("Unknown member rejects whole transaction"),M->EditRoutes({TEXT("R_001"),TEXT("missing")},Points,10,false));
    TestFalse(TEXT("Duplicate selection rejected"),M->EditRoutes({TEXT("R_001"),TEXT("R_001")},Points,10,false));
    TestFalse(TEXT("Degenerate route rejected"),M->EditRoutes(Ids,{Points[0],Points[0]},10,false));
    TestFalse(TEXT("Two point closed route rejected"),M->EditRoutes(Ids,Points,10,true));
    TestFalse(TEXT("Bad duration rejected"),M->EditRoutes(Ids,Points,0,false));
    TestEqual(TEXT("All rejection paths atomic"),M->SnapshotJson(),Before);
    TestTrue(TEXT("Apply selected routes"),M->EditRoutes(Ids,Points,10,false));
    FDemoScenarioConfig Config; FString Error; FScenarioConfigLoader::Parse(M->SnapshotJson(),Config,Error);
    TestTrue(TEXT("Primary world route retained"),Config.Units[0].Instances[0].FlightRoute.PointsMeters[0].Equals(Points[0]));
    TestTrue(TEXT("Wing route translates relative offset"),Config.Units[0].Instances[1].FlightRoute.PointsMeters[0].Equals(Points[0]+FVector(0,10,0)));
    TestTrue(TEXT("Unselected route remains absent"),Config.Units[0].Instances[2].FlightRoute.PointsMeters.IsEmpty());
    const FString Applied=M->SnapshotJson(); const uint64 Revision=M->GetRevision();
    TSharedPtr<FJsonObject> InvalidRoot; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Applied),InvalidRoot);
    InvalidRoot->GetArrayField(TEXT("units"))[0]->AsObject()->GetArrayField(TEXT("instances"))[0]->AsObject()->GetObjectField(TEXT("flightRoute"))->SetStringField(TEXT("closed"),TEXT("false"));
    FString InvalidJson; FJsonSerializer::Serialize(InvalidRoot.ToSharedRef(),TJsonWriterFactory<>::Create(&InvalidJson));
    FDemoScenarioConfig InvalidParsed; TestFalse(TEXT("Closed flag requires real JSON boolean"),FScenarioConfigLoader::Parse(InvalidJson,InvalidParsed,Error));
    TestTrue(TEXT("Custom routes preview on any map without source file"),Preview->Start(M,TEXT("not-required.json")));
    FTransform Flight; Preview->GetPreviewTransform(TEXT("R_001"),Flight);
    TestTrue(TEXT("No initial teleport"),Flight.GetLocation().Equals(FVector(1000,2000,3000),.001));
    Preview->AdvancePreview(20); Preview->GetPreviewTransform(TEXT("R_001"),Flight);
    TestTrue(TEXT("Open route ends and stays at final world waypoint"),Flight.GetLocation().Equals(FVector(20000,2000,3000),.001));
    TestEqual(TEXT("Preview keeps snapshot"),M->SnapshotJson(),Applied); TestEqual(TEXT("Preview keeps revision"),M->GetRevision(),Revision);
    TestTrue(TEXT("Source remains at edit position"),M->FindUnit(TEXT("R_001"))->GetActorLocation().Equals(FVector(1000,2000,3000)));
    Preview->Stop(); TestTrue(TEXT("One undo removes whole batch"),M->UndoEdit()); TestEqual(TEXT("Undo exact"),M->SnapshotJson(),Before);
    TestTrue(TEXT("Redo exact routes"),M->RedoEdit()); TestEqual(TEXT("Redo snapshot"),M->SnapshotJson(),Applied);
    TestTrue(TEXT("Reload preserves route data"),M->LoadJson(Applied,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")));
    TestEqual(TEXT("Round trip route snapshot"),M->SnapshotJson(),Applied);
    TestTrue(TEXT("Reset selected routes"),M->ResetRoutes(Ids)); TestEqual(TEXT("Reset restores absence"),M->SnapshotJson(),Before);
    auto Invalid=Config; Invalid.Units[0].Type=TEXT("vehicle");
    TestFalse(TEXT("Non aircraft route rejected by schema"),FScenarioConfigLoader::Parse(FScenarioConfigLoader::Serialize(Invalid),Config,Error));
    FDemoFlightRoute LongRoute; LongRoute.PeriodSeconds=40;
    for(int32 I=0;I<257;++I) LongRoute.PointsMeters.Add(FVector(I,I%7,30));
    TestTrue(TEXT("Full source sized editable route accepted"),LongRoute.Validate(Error));
    LongRoute.PointsMeters.SetNum(513); TestFalse(TEXT("Oversized route rejected"),LongRoute.Validate(Error));
    FDemoFlightPath ClosedPath;
    TestTrue(TEXT("Closed route adds missing closing point"),ClosedPath.FromRoute({FVector(10,0,0),FVector(20,0,0),FVector(20,10,0)},4,true,FVector::ZeroVector,Error));
    FVector A,B; FRotator Heading;
    ClosedPath.Evaluate(0,A,Heading); TestTrue(TEXT("Entry begins at source"),A.IsNearlyZero());
    const double EntrySeconds=ClosedPath.EntryDistanceCm/(ClosedPath.DistancesCm.Last()-ClosedPath.EntryDistanceCm)*ClosedPath.PeriodSeconds;
    ClosedPath.Evaluate(EntrySeconds+1,A,Heading); ClosedPath.Evaluate(EntrySeconds+5,B,Heading);
    TestTrue(TEXT("Closed loop has requested period after one time entry"),A.Equals(B,.001));
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteWidgetDraftTest,"TacticalDemo.Routes.WidgetDraftIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRouteWidgetDraftTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,CreatePackage(*(TEXT("/Temp/RouteWidget_")+FGuid::NewGuid().ToString(EGuidFormats::Digits))),true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>(); auto* P=World->SpawnActor<ADemoPlayerController>();
    const FString Scene=TEXT(R"({"scenarioName":"Route draft","units":[{"id":"R","modelId":"aircraft_red_basic","type":"aircraft","side":"red","count":2,"position":{"x":10,"y":20,"z":30},"spacing":10}]})");
    World->AddController(P);
    if(!TestTrue(TEXT("Load widget scene"),M->LoadJson(Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")))) { World->DestroyWorld(false); return false; }
    P->SelectUnit(TEXT("R_001")); auto* W=NewObject<UDemoRouteWidget>(World); W->EnsureInitialized();
    if(TestEqual(TEXT("Default two point editable table"),W->PointFields.Num(),6))
    {
        const FString Before=M->SnapshotJson(); W->PointFields[3]->SetText(FText::FromString(TEXT("500")));
        TestEqual(TEXT("Typing is not apply"),M->SnapshotJson(),Before);
        P->SelectUnit(TEXT("R_002")); W->RefreshSelection();
        TestEqual(TEXT("Draft survives changed selection"),W->PointFields[3]->GetText().ToString(),FString(TEXT("500")));
        W->Apply(); TestEqual(TEXT("Old draft cannot target new selection"),M->SnapshotJson(),Before);
        P->SelectUnit(TEXT("R_001")); W->Apply(); TestEqual(TEXT("Returning to same ID does not revive draft"),M->SnapshotJson(),Before);
        W->Reload(); W->PointFields[3]->SetText(FText::FromString(TEXT("600")));
        M->EditUnit(TEXT("R_002"),FVector(11,30,30),0); const FString Edited=M->SnapshotJson(); W->Apply();
        TestEqual(TEXT("Scene revision prevents stale apply"),M->SnapshotJson(),Edited);
        W->Reload(); W->PointFields[3]->SetText(FText::FromString(TEXT("700"))); W->Apply();
        TestFalse(TEXT("Manual button commits route"),M->GetScenarioConfig().Units[0].Instances[0].FlightRoute.PointsMeters.IsEmpty());
        TestTrue(TEXT("Other aircraft has no custom route"),M->GetScenarioConfig().Units[0].Instances[1].FlightRoute.PointsMeters.IsEmpty());
        W->AddPoint(); TestEqual(TEXT("Add row has three fields"),W->PointFields.Num(),9);
        W->RemovePoint(); TestEqual(TEXT("Remove row"),W->PointFields.Num(),6);
    }
    World->DestroyWorld(false); return true;
}
#endif
