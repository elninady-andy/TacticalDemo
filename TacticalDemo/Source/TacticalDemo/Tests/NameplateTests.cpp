#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DemoNameplateWidget.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "FlightPreview.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftNameplateTest,"TacticalDemo.Nameplates.IdentityAndTracking",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAircraftNameplateTest::RunTest(const FString&)
{
    const FVector2D LabelSize(80,32),Viewport(800,600);
    for(const double AnchorY:{5.,595.})
    {
        TArray<FSlateRect> Occupied;
        for(int32 I=0;I<3;++I)
        {
            const FVector2D P=UDemoNameplateWidget::PlaceLabel(FVector2D(400,AnchorY),LabelSize,Viewport,Occupied);
            const FSlateRect Rect(P.X,P.Y,P.X+LabelSize.X,P.Y+LabelSize.Y);
            TestTrue(TEXT("Edge labels remain inside viewport"),P.X>=0 && P.Y>=0 && Rect.Right<=Viewport.X && Rect.Bottom<=Viewport.Y);
            TestFalse(TEXT("Crowded edge labels do not overlap"),Occupied.ContainsByPredicate([&](const FSlateRect& Other){return FSlateRect::DoRectanglesIntersect(Rect,Other);}));
            Occupied.Add(Rect);
        }
    }
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,CreatePackage(*(TEXT("/Temp/Nameplate_")+FGuid::NewGuid().ToString(EGuidFormats::Digits))),true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>(); auto* Preview=World->SpawnActor<ADemoFlightPreview>();
    const FString Scene=TEXT(R"({"scenarioName":"Labels","units":[{"id":"R","modelId":"J20","type":"aircraft","side":"red","count":2,"position":{"x":10,"y":20,"z":30},"spacing":35},{"id":"B","modelId":"F35","type":"aircraft","side":"blue","count":1,"position":{"x":10,"y":100,"z":30},"spacing":35},{"id":"V","modelId":"vehicle_default","type":"vehicle","side":"blue","count":1,"position":{"x":10,"y":200,"z":30},"spacing":35}]})");
    const FString Library=FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json");
    if(!TestTrue(TEXT("Load mixed scene"),M->LoadJson(Scene,Library))) { World->DestroyWorld(false); return false; }
    const FString Before=M->SnapshotJson(); const uint64 Revision=M->GetRevision();
    auto Entries=UDemoNameplateWidget::Gather(World);
    TestEqual(TEXT("Every aircraft, no ground vehicle"),Entries.Num(),3);
    if(Entries.Num()==3)
    {
        TestEqual(TEXT("Actual red model displayed"),Entries[0].Text,FString(TEXT("J-20")));
        TestEqual(TEXT("Actual blue model displayed"),Entries[2].Text,FString(TEXT("F-35")));
        TestTrue(TEXT("Label above mesh top"),Entries[0].Anchor.Z>M->FindUnit(TEXT("R_001"))->Visual->Bounds.GetBox().Max.Z);
        const FVector OldAnchor=Entries[0].Anchor;
        M->EditUnit(TEXT("R_001"),FVector(30,40,50),0); Entries=UDemoNameplateWidget::Gather(World);
        TestTrue(TEXT("Label follows editing transform"),Entries[0].Anchor.Equals(OldAnchor+FVector(2000,2000,2000),.1));
        M->UndoEdit(); TestEqual(TEXT("Label collection does not mutate snapshot"),M->SnapshotJson(),Before); TestEqual(TEXT("Only actual edit and undo advance revision"),M->GetRevision(),Revision+2);
        M->EditRoutes({TEXT("R_001")},{FVector(10,20,30),FVector(110,20,30)},10,false);
        const FString Routed=M->SnapshotJson();
        if(TestTrue(TEXT("Start custom preview"),Preview->Start(M)))
        {
            Entries=UDemoNameplateWidget::Gather(World); TestEqual(TEXT("No duplicate nameplates during preview"),Entries.Num(),3);
            const FVector Start=Entries[0].Anchor; Preview->AdvancePreview(5); Entries=UDemoNameplateWidget::Gather(World);
            TestTrue(TEXT("Label follows moving copy"),Entries[0].Anchor.Equals(Start+FVector(5000,0,0),.1));
            TestEqual(TEXT("Tracking does not save preview position"),M->SnapshotJson(),Routed);
            Preview->Stop(); Entries=UDemoNameplateWidget::Gather(World); TestTrue(TEXT("Stop returns nameplate"),Entries[0].Anchor.Equals(Start,.1));
        }
    }
    M->LoadJson(Scene.Replace(TEXT("J20"),TEXT("J10")),Library); Entries=UDemoNameplateWidget::Gather(World);
    if(Entries.Num()) TestEqual(TEXT("Model replacement updates identity"),Entries[0].Text,FString(TEXT("J-10")));
    M->LoadJson(Scene.Replace(TEXT("J20"),TEXT("missing_model")),Library,false); Entries=UDemoNameplateWidget::Gather(World);
    if(TestFalse(TEXT("Fallback aircraft still labelled"),Entries.IsEmpty())) TestEqual(TEXT("Fallback never claims missing requested model"),Entries[0].Text,FString(TEXT("飞机")));
    M->ClearScenario(); TestTrue(TEXT("Clear removes all labels"),UDemoNameplateWidget::Gather(World).IsEmpty());
    World->DestroyWorld(false); return true;
}
#endif
