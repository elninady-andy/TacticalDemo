#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "DemoGameMode.h"
#include "GameFramework/SpectatorPawn.h"
#include "EquipmentLibrary.h"

// Removing replacement cleanup, conversion or validation must break these observable assertions.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoSceneTest, "TacticalDemo.Scene.ReloadAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDemoSceneTest::RunTest(const FString&)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    ADemoScenarioManager* Manager = World->SpawnActor<ADemoScenarioManager>();
    const FString Dir = FPaths::ProjectSavedDir() / TEXT("Tests") / FGuid::NewGuid().ToString();
    IFileManager::Get().MakeDirectory(*Dir, true);
    const FString Scenario = Dir / TEXT("Scenario.json");
    const FString Library = Dir / TEXT("EquipmentLibrary.json");
    FString LibraryText;
    FFileHelper::LoadFileToString(LibraryText, *(FPaths::ProjectConfigDir() / TEXT("EquipmentLibrary.json")));
    FFileHelper::SaveStringToFile(LibraryText, *Library);
    auto WriteScene = [&](int32 Count, const FString& Side, const FString& Model)
    {
        return FFileHelper::SaveStringToFile(FString::Printf(TEXT(
            "{\"scenarioName\":\"RuntimeTest\",\"units\":[{\"id\":\"R1\",\"modelId\":\"%s\",\"type\":\"aircraft\",\"side\":\"%s\",\"count\":%d,\"position\":{\"x\":2,\"y\":3,\"z\":10},\"spacing\":6}]}"),
            *Model, *Side, Count), *Scenario);
    };
    WriteScene(3, TEXT("red"), TEXT("aircraft_red_basic"));
    const bool bLoaded = Manager->LoadScenario(Scenario, Library);
    TestTrue(TEXT("Valid scene loads"), bLoaded);
    if (bLoaded && Manager->GetUnits().Num() == 3)
    {
        const TWeakObjectPtr<ADemoUnitActor> Old = Manager->GetUnits()[0];
        TestEqual(TEXT("Meters convert to centimeters"), Old->GetActorLocation(), FVector(200,300,1000));
        TestEqual(TEXT("Spacing applied"), Manager->GetUnits()[2]->GetActorLocation(), FVector(200,1500,1000));
        TestEqual(TEXT("Red state"), Old->Side, FString(TEXT("red")));
        ADemoPlayerController* Player = World->SpawnActor<ADemoPlayerController>();
        // This isolated world has not run InitializeActorsForPlay: register its controller explicitly.
        World->AddController(Player);
        ASpectatorPawn* Pawn = World->SpawnActor<ASpectatorPawn>();
        Player->Possess(Pawn);
        ADemoGameMode* Mode = World->SpawnActor<ADemoGameMode>();
        Mode->FrameScene(Manager);
        // Independent fixture center: x=2m, y=(3+15)/2m, z=10m.
        const FVector TowardUnits = (FVector(200,900,1000)-Pawn->GetActorLocation()).GetSafeNormal();
        TestEqual(TEXT("Camera test controller registered"), World->GetFirstPlayerController(), static_cast<APlayerController*>(Player));
        TestTrue(TEXT("Camera frames non-colliding units instead of origin"),
            FVector::DotProduct(Player->GetControlRotation().Vector(), TowardUnits) > 0.9999);
        WriteScene(5, TEXT("blue"), TEXT("aircraft_blue_basic"));
        TestTrue(TEXT("Reload succeeds"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("New count is five"), Manager->GetUnits().Num(), 5);
        TestTrue(TEXT("Old actor destroyed"), !Old.IsValid() || Old->IsActorBeingDestroyed());
        int32 LiveCount = 0;
        for (TActorIterator<ADemoUnitActor> It(World); It; ++It) if (!It->IsActorBeingDestroyed()) ++LiveCount;
        TestEqual(TEXT("No duplicate live actors"), LiveCount, 5);
        TestEqual(TEXT("Blue state"), Manager->GetUnits()[0]->Side, FString(TEXT("blue")));
        UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Manager->GetUnits()[0]->Visual->GetMaterial(0));
        if (TestNotNull(TEXT("Color material exists"), MID))
            TestEqual(TEXT("Blue material parameter"), MID->K2_GetVectorParameterValue(TEXT("SideColor")), FLinearColor(FColor::FromHex(TEXT("0A4DFF"))));

        ADemoUnitActor* Kept = Manager->GetUnits()[0];
        FString RoutedScene;
        FFileHelper::LoadFileToString(RoutedScene, *Scenario);
        RoutedScene.ReplaceInline(TEXT("\"spacing\":6}"), TEXT("\"spacing\":6,\"route\":[{\"x\":0,\"y\":0,\"z\":5},{\"x\":20,\"y\":10,\"z\":5}] }"));
        FFileHelper::SaveStringToFile(RoutedScene, *Scenario);
        TestTrue(TEXT("Static route scene loads"), Manager->LoadScenario(Scenario, Library));
        ULineBatchComponent* RouteLines = Cast<ULineBatchComponent>(Manager->GetRootComponent());
        TestNotNull(TEXT("Manager owns route drawing component"), RouteLines);
        if (RouteLines) TestEqual(TEXT("Two waypoints draw one static segment"), RouteLines->BatchedLines.Num(), 1);
        TestEqual(TEXT("Route does not move units"), Manager->GetUnits()[0]->GetActorLocation(), FVector(200,300,1000));
        Kept = Manager->GetUnits()[0];
        WriteScene(1, TEXT("red"), TEXT("missing_model"));
        TestFalse(TEXT("Chat candidate rejects unregistered model instead of falling back"), Manager->LoadChatCandidate(Scenario, Library));
        TestEqual(TEXT("Rejected model keeps existing actor"), Manager->GetUnits()[0].Get(), Kept);
        TestEqual(TEXT("Rejected model keeps old route"), RouteLines ? RouteLines->BatchedLines.Num() : 0, 1);
        FString ExactLibrary = LibraryText;
        const FString RegisteredCandidate = TEXT("\"custom_sphere\":{\"type\":\"aircraft\",\"kind\":\"staticMesh\",\"asset\":\"/Engine/BasicShapes/Sphere.Sphere\"},");
        ExactLibrary.ReplaceInline(TEXT("\"models\": {"), *(TEXT("\"models\": {") + RegisteredCandidate));
        FFileHelper::SaveStringToFile(ExactLibrary, *Library);
        WriteScene(1, TEXT("red"), TEXT("custom_sphere"));
        TestTrue(TEXT("Registered exact model applies to chat candidate"), Manager->LoadChatCandidate(Scenario, Library));
        TestEqual(TEXT("Exact model is used"), Manager->GetUnits()[0]->Visual->GetStaticMesh()->GetName(), FString(TEXT("Sphere")));
        TestEqual(TEXT("Replacing candidate clears previous route"), RouteLines ? RouteLines->BatchedLines.Num() : -1, 0);

        Kept = Manager->GetUnits()[0];
        FFileHelper::SaveStringToFile(TEXT("{broken"), *Scenario);
        TestFalse(TEXT("Bad JSON rejected"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Bad JSON preserves instances"), Manager->GetUnits()[0].Get(), Kept);
        TestFalse(TEXT("Error is available"), Manager->GetLastError().IsEmpty());

        WriteScene(3, TEXT("red"), TEXT("aircraft_default"));
        FString ExtremeText;
        FFileHelper::LoadFileToString(ExtremeText, *Scenario);
        ExtremeText.ReplaceInline(TEXT("\"spacing\":6"), TEXT("\"spacing\":1e200"));
        FFileHelper::SaveStringToFile(ExtremeText, *Scenario);
        TestFalse(TEXT("Extreme spacing rejected before spawn"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Extreme spacing preserves existing scene"), Manager->GetUnits()[0].Get(), Kept);

        WriteScene(1, TEXT("red"), TEXT("missing_model"));
        TestTrue(TEXT("Unknown model falls back"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Fallback cone"), Manager->GetUnits()[0]->Visual->GetStaticMesh()->GetName(), FString(TEXT("Cone")));

        // Register a new model id without changing code.
        FString NewLibrary = LibraryText;
        const FString NewModel = TEXT("\"custom_sphere\":{\"type\":\"aircraft\",\"kind\":\"staticMesh\",\"asset\":\"/Engine/BasicShapes/Sphere.Sphere\"},");
        NewLibrary.ReplaceInline(TEXT("\"models\": {"), *(TEXT("\"models\": {") + NewModel));
        FFileHelper::SaveStringToFile(NewLibrary, *Library);
        WriteScene(2, TEXT("blue"), TEXT("custom_sphere"));
        TestTrue(TEXT("New library registration loads"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Different model selected"), Manager->GetUnits()[0]->Visual->GetStaticMesh()->GetName(), FString(TEXT("Sphere")));

        NewLibrary.ReplaceInline(TEXT("/Engine/BasicShapes/Sphere.Sphere\"},"), TEXT("/Game/Missing/NoMesh.NoMesh\"},"));
        FFileHelper::SaveStringToFile(NewLibrary, *Library);
        TestTrue(TEXT("Registered but missing asset falls back"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Broken asset uses cone"), Manager->GetUnits()[0]->Visual->GetStaticMesh()->GetName(), FString(TEXT("Cone")));

        Kept = Manager->GetUnits()[0];
        const FString BadDefault = LibraryText.Replace(TEXT("/Engine/BasicShapes/Cone.Cone"), TEXT("/Game/Missing/NoDefault.NoDefault"));
        FFileHelper::SaveStringToFile(BadDefault, *Library);
        TestFalse(TEXT("Unusable default asset rejected"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Unusable default preserves scene"), Manager->GetUnits()[0].Get(), Kept);
        FFileHelper::SaveStringToFile(TEXT("{\"defaults\":{},\"models\":{}}"), *Library);
        TestFalse(TEXT("Missing defaults rejected"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Bad library preserves scene"), Manager->GetUnits()[0].Get(), Kept);
        FFileHelper::SaveStringToFile(LibraryText, *Library);
        FFileHelper::SaveStringToFile(TEXT("{\"scenarioName\":\"Empty\",\"units\":[]}"), *Scenario);
        TestTrue(TEXT("Empty scene loads"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Empty scene clears units"), Manager->GetUnits().Num(), 0);
        TestEqual(TEXT("Clearing scene removes static route lines"), RouteLines ? RouteLines->BatchedLines.Num() : -1, 0);
        WriteScene(3, TEXT("red"), TEXT("aircraft_default"));
        TestTrue(TEXT("Restores after clear"), Manager->LoadScenario(Scenario, Library));
        TestEqual(TEXT("Restored count"), Manager->GetUnits().Num(), 3);
    }
    else TestEqual(TEXT("Initial count is three"), Manager->GetUnits().Num(), 3);
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Dir, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoLibraryTypeTest, "TacticalDemo.Config.LibraryStrictStrings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDemoLibraryTypeTest::RunTest(const FString&)
{
    FString Text;
    FFileHelper::LoadFileToString(Text, *(FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")));
    // An unused entry must still satisfy the schema: JSON number is not an asset path.
    Text.ReplaceInline(TEXT("\"models\": {"), TEXT("\"models\": {\"bad\":{\"type\":\"aircraft\",\"kind\":\"staticMesh\",\"asset\":123},"));
    const FString Path = FPaths::ProjectSavedDir()/TEXT("Tests")/ (FGuid::NewGuid().ToString()+TEXT(".json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    FFileHelper::SaveStringToFile(Text, *Path);
    FEquipmentLibrary Library;
    FString Error;
    TestFalse(TEXT("Numeric asset is rejected even when unused"), Library.Load(Path, Error));
    TestFalse(TEXT("Numeric asset error available"), Error.IsEmpty());
    IFileManager::Get().Delete(*Path);
    return true;
}
#endif
