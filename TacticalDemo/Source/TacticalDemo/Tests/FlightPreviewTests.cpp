#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "FlightPreview.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace FlightPreviewTest
{
    const FString Paths=TEXT(R"({"mapAsset":"/Game/Maps/map3","paths":[{"modelId":"aircraft_red_basic","closed":true,"periodSeconds":4,"yawOffsetDegrees":0,"samplesCm":[[0,0,0],[10000,0,0],[10000,10000,0],[0,10000,0],[0,0,0]]}]})");
    const FString Scene=TEXT(R"({"schemaVersion":2,"scenarioName":"Flight preview test","units":[{"id":"R1","modelId":"aircraft_red_basic","type":"aircraft","side":"red","count":2,"position":{"x":20,"y":30,"z":40},"spacing":35,"instances":[{"id":"R1_001","position":{"x":20,"y":30,"z":40},"yaw":0,"formation":"Flight"},{"id":"R1_002","position":{"x":20,"y":65,"z":40},"yaw":0,"formation":"Flight"}]}]})");
}

// Catches sample-index timing, missing wraparound, and acceptance of degenerate routes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightPathDataTest,"TacticalDemo.FlightPreview.PathData",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFlightPathDataTest::RunTest(const FString&)
{
    FString Map,Error; TArray<FDemoFlightPath> Paths;
    if(TestTrue(TEXT("Parse route"),FDemoFlightPath::ParseJson(FlightPreviewTest::Paths,Map,Paths,Error)))
    {
        FVector Position; FRotator Heading;
        Paths[0].Evaluate(.5,Position,Heading);
        TestTrue(TEXT("Half first edge uses centimeters"),Position.Equals(FVector(5000,0,0),.001));
        Paths[0].Evaluate(2,Position,Heading);
        TestTrue(TEXT("Half cycle reaches opposite corner"),Position.Equals(FVector(10000,10000,0),.001));
        Paths[0].Evaluate(4,Position,Heading);
        TestTrue(TEXT("Full cycle closes exactly"),Position.Equals(FVector::ZeroVector,.001));
        Paths[0].Evaluate(8.5,Position,Heading);
        TestTrue(TEXT("Multiple cycles wrap"),Position.Equals(FVector(5000,0,0),.001));
    }
    TestFalse(TEXT("Zero period rejected"),FDemoFlightPath::ParseJson(FlightPreviewTest::Paths.Replace(TEXT("\"periodSeconds\":4"),TEXT("\"periodSeconds\":0")),Map,Paths,Error));
    TestFalse(TEXT("Duplicate adjacent samples rejected"),FDemoFlightPath::ParseJson(FlightPreviewTest::Paths.Replace(TEXT("[10000,0,0]"),TEXT("[0,0,0]")),Map,Paths,Error));
    TestFalse(TEXT("Open route not silently looped"),FDemoFlightPath::ParseJson(FlightPreviewTest::Paths.Replace(TEXT("\"closed\":true"),TEXT("\"closed\":false")),Map,Paths,Error));
    TestFalse(TEXT("Missing closure rejected"),FDemoFlightPath::ParseJson(FlightPreviewTest::Paths.Replace(TEXT("[0,10000,0],[0,0,0]"),TEXT("[0,10000,0],[0,100,0]")),Map,Paths,Error));
    return true;
}

// Catches edits/saves of preview transforms, visibility leaks, and stale bindings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightPreviewIsolationTest,"TacticalDemo.FlightPreview.IsolationAndReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFlightPreviewIsolationTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    const FString TestMap=TEXT("/Game/Tests/FlightPreview_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,CreatePackage(*TestMap),true,ERHIFeatureLevel::Num,&Init);
    auto* Manager=World->SpawnActor<ADemoScenarioManager>();
    auto* Preview=World->SpawnActor<ADemoFlightPreview>();
    const FString Fixture=FPaths::ProjectSavedDir()/TEXT("Tests/FlightPaths-")+FGuid::NewGuid().ToString()+TEXT(".json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Fixture),true);
    FFileHelper::SaveStringToFile(FlightPreviewTest::Paths.Replace(TEXT("/Game/Maps/map3"),*TestMap),*Fixture,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    TestFalse(TEXT("Startup stays static"),Preview->IsPlaying());
    if(TestTrue(TEXT("Load editable scene"),Manager->LoadJson(FlightPreviewTest::Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"))))
    {
        auto* First=Manager->FindUnit(TEXT("R1_001")); auto* Wing=Manager->FindUnit(TEXT("R1_002"));
        Manager->EditUnit(First->UnitId,FVector(20,30,45),15);
        const FString Snapshot=Manager->SnapshotJson(); const uint64 Revision=Manager->GetRevision();
        const FTransform Original=First->GetActorTransform(),WingOriginal=Wing->GetActorTransform();
        const bool Undo=Manager->CanUndo(),Redo=Manager->CanRedo();
        if(TestTrue(TEXT("Explicit preview starts"),Preview->Start(Manager,Fixture)))
        {
            FTransform Flight;
            TestTrue(TEXT("Visible position available"),Preview->GetPreviewTransform(First->UnitId,Flight));
            TestTrue(TEXT("Start preserves lead position"),Flight.GetLocation().Equals(Original.GetLocation(),.001));
            TestFalse(TEXT("Source hidden during preview"),First->Visual->IsVisible());
            Preview->AdvancePreview(.5); Preview->GetPreviewTransform(First->UnitId,Flight);
            TestTrue(TEXT("Clone follows translated route"),Flight.GetLocation().Equals(FVector(7000,3000,4500),.001));
            TestTrue(TEXT("Original actor does not move"),First->GetActorTransform().Equals(Original,.001));
            TestTrue(TEXT("Wing actor does not move"),Wing->GetActorTransform().Equals(WingOriginal,.001));
            TestEqual(TEXT("Saving uses committed position"),Manager->SnapshotJson(),Snapshot);
            TestEqual(TEXT("Preview keeps scene revision"),Manager->GetRevision(),Revision);
            TestEqual(TEXT("Preview keeps undo"),Manager->CanUndo(),Undo);
            TestEqual(TEXT("Preview keeps redo"),Manager->CanRedo(),Redo);
            TestTrue(TEXT("Camera bounds available"),Preview->GetPreviewBounds().IsValid!=0);
            Preview->Stop();
            TestFalse(TEXT("Stop disables playback"),Preview->IsPlaying());
            TestTrue(TEXT("Stop restores visibility"),First->Visual->IsVisible());
            TestFalse(TEXT("Stop removes clone"),Preview->GetPreviewTransform(First->UnitId,Flight));
            TestTrue(TEXT("Stop keeps exact actor transform"),First->GetActorTransform().Equals(Original,.001));
            TestEqual(TEXT("Stop keeps saved state"),Manager->SnapshotJson(),Snapshot);
            Preview->Stop(); TestTrue(TEXT("Stop is idempotent"),First->Visual->IsVisible());
            TestTrue(TEXT("Restart preview"),Preview->Start(Manager,Fixture));
            Manager->EditUnit(First->UnitId,FVector(25,35,45),45); Preview->AdvancePreview(.1);
            TestFalse(TEXT("Committed edit stops playback"),Preview->IsPlaying());
            TestTrue(TEXT("Edit survives cleanup"),First->GetActorLocation().Equals(FVector(2500,3500,4500),.001));
            TestTrue(TEXT("Restart after edit"),Preview->Start(Manager,Fixture));
            First->Visual->SetStaticMesh(nullptr); Preview->AdvancePreview(.1);
            TestFalse(TEXT("Invalid source model stops safely"),Preview->IsPlaying());
            TestTrue(TEXT("Invalidation restores visibility"),First->Visual->IsVisible());
        }
    }
    Preview->Stop(); World->DestroyWorld(false); IFileManager::Get().Delete(*Fixture); return true;
}
#endif
