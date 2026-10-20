#include "Misc/AutomationTest.h"
#include "DemoControlWidget.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "ChatProtocol.h"
#include "SceneLibrary.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportedProtocolTest,"TacticalDemo.ImportedAssets.Protocol",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImportedProtocolTest::RunTest(const FString&)
{
    FString Asset,Error; FDemoChatReply Reply;
    TestTrue(TEXT("Airport is registered and exists"),FDemoSceneLibrary::Resolve(TEXT("airport"),Asset,Error));
    TestFalse(TEXT("Unregistered map rejected"),FDemoSceneLibrary::Resolve(TEXT("unknown"),Asset,Error));
    TestTrue(TEXT("Airport request generates map candidate"),FDemoChatProtocol::Parse(TEXT("{\"levelId\":\"airport\"}"),Reply,Error,FString(),TEXT("切换到机场场景")));
    TestEqual(TEXT("Candidate id"),Reply.LevelId,FString(TEXT("airport")));
    TestFalse(TEXT("Question cannot generate map candidate"),FDemoChatProtocol::Parse(TEXT("{\"levelId\":\"airport\"}"),Reply,Error,FString(),TEXT("可以切换到机场场景吗")));
    TestTrue(TEXT("Model shorthand recognized as operation"),FDemoChatProtocol::IsSceneRequest(TEXT("把红方飞机换成J20")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMap3StartupTest,"TacticalDemo.ImportedAssets.Map3Startup",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMap3StartupTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("map3"),CreatePackage(TEXT("/Game/Maps/map3")),true,ERHIFeatureLevel::Num,&Init);
    auto* Manager=World->SpawnActor<ADemoScenarioManager>();
    TestTrue(TEXT("Map3 initial scenario loads"),Manager->ReloadScenario());
    TestEqual(TEXT("Map3 uses six imported aircraft"),Manager->GetUnits().Num(),6);
    for(const auto& Group:Manager->GetScenarioConfig().Units)
    {
        TestEqual(TEXT("Initial formation has three explicit members"),Group.Instances.Num(),3);
        for(const auto& Instance:Group.Instances) TestEqual(TEXT("Initial formation is named"),Instance.Formation,Group.Side==TEXT("red")?FString(TEXT("编队1")):FString(TEXT("编队2")));
    }
    for(ADemoUnitActor* Unit:Manager->GetUnits())
    {
        TestTrue(TEXT("Map3 aircraft above source terrain"),Unit->GetActorLocation().Z>10000);
        TestEqual(TEXT("Map3 uses imported source aircraft"),Unit->Visual->GetStaticMesh()->GetName(),Unit->Side==TEXT("red")?FString(TEXT("SM_J20_Map3")):FString(TEXT("SM_F35")));
    }
    FString Asset,Error; FDemoChatReply Reply;
    TestTrue(TEXT("Map3 can be selected"),FDemoSceneLibrary::Resolve(TEXT("map3"),Asset,Error));
    TestTrue(TEXT("Map3 instruction creates candidate"),FDemoChatProtocol::Parse(TEXT("{\"levelId\":\"map3\"}"),Reply,Error,FString(),TEXT("切换到map3场景")));
    TestEqual(TEXT("Map3 candidate id"),Reply.LevelId,FString(TEXT("map3")));
    World->DestroyWorld(false); return true;
}

class FImportedQwenRun : public IAutomationLatentCommand
{
public:
    explicit FImportedQwenRun(FAutomationTestBase* T):Test(T) {}
    bool Update() override
    {
        if(!World)
        {
            const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            Manager=World->SpawnActor<ADemoScenarioManager>();
            FString Fixture;
            FFileHelper::LoadFileToString(Fixture,*(FPaths::ProjectConfigDir()/TEXT("AirportScenario.json")));
            Fixture.ReplaceInline(TEXT("\"textureId\":\"1\","),TEXT(""));
            Fixture.ReplaceInline(TEXT("\"J20\""),TEXT("\"aircraft_red_basic\""));
            Fixture.ReplaceInline(TEXT("\"J10\""),TEXT("\"aircraft_blue_basic\""));
            const FString FixturePath=FPaths::ProjectSavedDir()/TEXT("ImportedModelFixture.json");
            FFileHelper::SaveStringToFile(Fixture,*FixturePath);
            Test->TestTrue(TEXT("Basic models fixture loaded"),Manager->LoadScenario(FixturePath,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")));
            Window.Reset(NewObject<UDemoControlWidget>(World)); Window->Initialize();
            if(!Window->Input) Window->NativeOnInitialized();
            Window->Endpoint->SetText(FText::FromString(TEXT("http://127.0.0.1:8000/v1/chat/completions")));
            Window->Model->SetText(FText::FromString(TEXT("qwen")));
            Send(); return false;
        }
        if(Window->ActiveRequest.IsValid() && FPlatformTime::Seconds()<Deadline) return false;
        Test->TestFalse(TEXT("Actual Qwen request finished"),Window->ActiveRequest.IsValid());
        if(Index<2)
        {
            Test->TestFalse(TEXT("Actual Qwen produced model candidate"),Window->Candidate.IsEmpty());
            if(!Window->Candidate.IsEmpty()) Window->ApplyButton->OnClicked.Broadcast();
            for(ADemoUnitActor* Unit:Manager->GetUnits())
            {
                const FString Expected=Unit->Side==TEXT("red")?TEXT("J20S"):(Index==0?TEXT("Cube"):TEXT("J10B"));
                Test->TestEqual(TEXT("Registered imported model reaches rendered actor"),Unit->Visual->GetStaticMesh()->GetName(),Expected);
                if(Index==0 && Unit->Side==TEXT("blue")) continue;
                Test->TestEqual(TEXT("Imported model keeps real scale"),Unit->GetActorScale3D(),FVector(1));
                Test->TestEqual(TEXT("Imported model keeps its material"),Unit->Visual->GetMaterial(0),Unit->Visual->GetStaticMesh()->GetMaterial(0));
            }
        }
        else
        {
            Test->TestEqual(TEXT("Actual Qwen selects airport scene"),Window->CandidateLevel,FString(TEXT("airport")));
            Test->TestTrue(TEXT("Map candidate awaits manual apply"),Window->ApplyButton->GetIsEnabled());
        }
        if(!Test->HasAnyErrors() && ++Index<3) { Send(); return false; }
        Window->NativeDestruct(); Window.Reset(); World->DestroyWorld(false); return true;
    }
private:
    void Send()
    {
        const TCHAR* Inputs[]={TEXT("把红方飞机换成J20"),TEXT("把蓝方飞机换成J10"),TEXT("切换到机场场景")};
        Window->Input->SetText(FText::FromString(Inputs[Index])); Window->SendButton->OnClicked.Broadcast(); Deadline=FPlatformTime::Seconds()+300;
    }
    FAutomationTestBase* Test;
    UWorld* World=nullptr;
    ADemoScenarioManager* Manager=nullptr;
    TStrongObjectPtr<UDemoControlWidget> Window;
    double Deadline=0;
    int32 Index=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImportedQwenTest,"Integration.ImportedQwen",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FImportedQwenTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FImportedQwenRun(this)); return true;
}
