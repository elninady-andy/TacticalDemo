#include "Misc/AutomationTest.h"
#include "ChatProtocol.h"
#include "ScenarioConfigLoader.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "DemoControlWidget.h"
#include "EquipmentLibrary.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Components/StaticMeshComponent.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureProtocolTest,"TacticalDemo.Texture.Protocol",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTextureProtocolTest::RunTest(const FString&)
{
    const FString Current=TEXT("{\"scenarioName\":\"TextureTest\",\"units\":[{\"id\":\"R\",\"type\":\"aircraft\",\"modelId\":\"J20\",\"side\":\"red\",\"count\":1,\"position\":{\"x\":300,\"y\":-500,\"z\":3},\"spacing\":30},{\"id\":\"B\",\"type\":\"aircraft\",\"modelId\":\"J10\",\"side\":\"blue\",\"count\":1,\"position\":{\"x\":330,\"y\":-500,\"z\":3},\"spacing\":30}]}");
    FDemoChatReply Reply; FString Error;
    TestTrue(TEXT("Texture request recognized"),FDemoChatProtocol::IsSceneRequest(TEXT("把红方飞机的贴图换成第二款")));
    TestTrue(TEXT("Switch-to texture wording recognized"),FDemoChatProtocol::IsSceneRequest(TEXT("把J10飞机的贴图切换到第二款")));
    TestFalse(TEXT("Texture-only request cannot clear scene via full-scene reply"),FDemoChatProtocol::Parse(TEXT("{\"scenarioName\":\"Empty\",\"units\":[]}"),Reply,Error,Current,TEXT("把红方飞机的贴图换成第二款")));
    TestFalse(TEXT("Texture-only request cannot create instead of updating"),FDemoChatProtocol::Parse(TEXT("{\"create\":[{\"type\":\"aircraft\",\"side\":\"red\",\"count\":1}]}"),Reply,Error,Current,TEXT("把红方飞机的贴图换成第二款")));
    TestTrue(TEXT("Texture command produces validated candidate"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"textureId\":\"2\"}}"),Reply,Error,Current,TEXT("把红方飞机的贴图换成第二款")));
    TestTrue(TEXT("Candidate includes requested texture"),Reply.Scenario.Contains(TEXT("textureId")));
    FDemoScenarioConfig Parsed;
    if(!Reply.Scenario.IsEmpty())
    {
        TestTrue(TEXT("Texture scenario parses"),FScenarioConfigLoader::Parse(Reply.Scenario,Parsed,Error));
        if(Parsed.Units.Num()==2)
        {
            TestEqual(TEXT("Texture update preserves red position"),Parsed.Units[0].PositionMeters,FVector(300,-500,3));
            TestEqual(TEXT("Texture update preserves red model"),Parsed.Units[0].ModelId,FString(TEXT("J20")));
        }
    }
    TestTrue(TEXT("Model selector for texture accepts registered model id"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"targetModel\":\"J10\",\"set\":{\"textureId\":\"1\"}}"),Reply,Error,Current,TEXT("把J10飞机的贴图换成第一款")));
    TestTrue(TEXT("Omitted selector inferred from explicit model name"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"textureId\":\"2\"}}"),Reply,Error,Current,TEXT("把J10飞机的贴图换成第二款")));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Parsed,Error) && Parsed.Units.Num()==2)
    {TestTrue(TEXT("J10-only texture request keeps J20 texture"),Parsed.Units[0].TextureId.IsEmpty());TestEqual(TEXT("J10-only texture request updates J10"),Parsed.Units[1].TextureId,FString(TEXT("2")));}
    TestFalse(TEXT("Texture id must be string"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"textureId\":2}}"),Reply,Error,Current,TEXT("把飞机的贴图换成第二款")));
    TestFalse(TEXT("Texture fields cannot leak into position update"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"textureId\":\"2\"}}"),Reply,Error,Current,TEXT("把红方飞机移动到X=20米")));
    TestTrue(TEXT("Texture request strips hallucinated model and coordinates"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"textureId\":\"2\",\"modelId\":\"J10\",\"count\":20,\"x\":0}}"),Reply,Error,Current,TEXT("给红方飞机模型更换第二款贴图")));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Parsed,Error) && Parsed.Units.Num()==2)
    {TestEqual(TEXT("Texture request keeps aircraft model"),Parsed.Units[0].ModelId,FString(TEXT("J20")));TestEqual(TEXT("Texture request keeps quantity"),Parsed.Units[0].Count,1);TestEqual(TEXT("Texture request keeps aircraft position"),Parsed.Units[0].PositionMeters,FVector(300,-500,3));}
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureSceneTest,"TacticalDemo.Texture.ApplyAndRollback",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTextureSceneTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* Manager=World->SpawnActor<ADemoScenarioManager>();
    FString Fixture; FFileHelper::LoadFileToString(Fixture,*(FPaths::ProjectConfigDir()/TEXT("AirportScenario.json")));
    const FString FixturePath=FPaths::ProjectSavedDir()/TEXT("TextureFixture.json");
    const FString LibraryPath=FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json");
    for(const FString Variant:{TEXT("1"),TEXT("2")})
    {
        FDemoChatReply Reply; FString Error;
        TestTrue(TEXT("All-aircraft texture update parses"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"textureId\":\"")+Variant+TEXT("\"}}"),Reply,Error,Fixture,TEXT("把全部飞机的贴图换成第")+Variant+TEXT("款")));
        FFileHelper::SaveStringToFile(Reply.Scenario,*FixturePath);
        if(!TestTrue(TEXT("Registered texture applies to real actors"),Manager->LoadChatCandidate(FixturePath,LibraryPath))) break;
        for(ADemoUnitActor* Unit:Manager->GetUnits())
        {
            auto* Material=Cast<UMaterialInstanceDynamic>(Unit->Visual->GetMaterial(0));
            if(TestNotNull(TEXT("Only body material uses dynamic texture"),Material))
            {
                UTexture* Texture=Material->K2_GetTextureParameterValue(TEXT("AircraftDiffuse"));
                const FString Expected=Unit->Side==TEXT("red")?(Variant==TEXT("1")?TEXT("mat_J20_Airframe_Diffuse"):TEXT("mat_J20_Airframe_Diffuse2")):(Variant==TEXT("1")?TEXT("j10b_d"):TEXT("j10b_d2"));
                if(TestNotNull(TEXT("Body texture is loaded"),Texture)) TestEqual(TEXT("Selected registered texture reaches mesh material"),Texture->GetName(),Expected);
                if(Unit->Side==TEXT("blue")) TestEqual(TEXT("J10 canopy material is preserved"),Unit->Visual->GetMaterial(1),Unit->Visual->GetStaticMesh()->GetMaterial(1));
                TestEqual(TEXT("Texture switching keeps model scale"),Unit->GetActorScale3D(),FVector(1));
            }
        }
    }
    ADemoUnitActor* Kept=Manager->GetUnits().IsEmpty()?nullptr:Manager->GetUnits()[0].Get();
    FDemoChatReply Invalid; FString Error;
    TestTrue(TEXT("Unknown texture reaches resource validation"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"textureId\":\"9\"}}"),Invalid,Error,Fixture,TEXT("把全部飞机的贴图换成第9款")));
    FFileHelper::SaveStringToFile(Invalid.Scenario,*FixturePath);
    TestFalse(TEXT("Unknown texture rejected before scene mutation"),Manager->LoadChatCandidate(FixturePath,LibraryPath));
    if(Kept) TestEqual(TEXT("Invalid texture keeps current actor"),Manager->GetUnits()[0].Get(),Kept);
    Manager->ClearScenario(); World->DestroyWorld(false); return true;
}

class FTextureQwenRun:public IAutomationLatentCommand
{
public:
    explicit FTextureQwenRun(FAutomationTestBase* InTest):Test(InTest){}
    bool Update() override
    {
        if(!World)
        {
            const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            Manager=World->SpawnActor<ADemoScenarioManager>();
            Test->TestTrue(TEXT("Airport fixture loaded"),Manager->LoadScenario(FPaths::ProjectConfigDir()/TEXT("AirportScenario.json"),FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")));
            for(ADemoUnitActor* Unit:Manager->GetUnits()) Positions.Add(Unit->UnitId,Unit->GetActorLocation());
            Window.Reset(NewObject<UDemoControlWidget>(World)); Window->Initialize(); if(!Window->Input) Window->NativeOnInitialized();
            Window->Endpoint->SetText(FText::FromString(TEXT("http://127.0.0.1:8000/v1/chat/completions"))); Window->Model->SetText(FText::FromString(TEXT("qwen")));
            Send(); return false;
        }
        if(Window->ActiveRequest.IsValid() && FPlatformTime::Seconds()<Deadline) return false;
        Test->TestFalse(TEXT("Actual Qwen texture request completed"),Window->ActiveRequest.IsValid());
        Test->TestFalse(TEXT("Actual Qwen texture candidate exists"),Window->Candidate.IsEmpty());
        if(!Window->Candidate.IsEmpty()) Window->ApplyButton->OnClicked.Broadcast();
        Test->TestEqual(TEXT("Texture commands preserve all6 aircraft"),Manager->GetUnits().Num(),6);
        for(ADemoUnitActor* Unit:Manager->GetUnits())
        {
            const bool Red=Unit->Side==TEXT("red");
            const bool Second=Red?(Index<2):(Index>=1 && Index<3);
            auto* Material=Cast<UMaterialInstanceDynamic>(Unit->Visual->GetMaterial(0));
            if(Test->TestNotNull(TEXT("Real Qwen applies switchable material"),Material))
            {
                UTexture* Texture=Material->K2_GetTextureParameterValue(TEXT("AircraftDiffuse"));
                if(Test->TestNotNull(TEXT("Real Qwen applies texture resource"),Texture)) Test->TestEqual(TEXT("Requested texture reaches body material"),Texture->GetName(),FString(Red?(Second?TEXT("mat_J20_Airframe_Diffuse2"):TEXT("mat_J20_Airframe_Diffuse")):(Second?TEXT("j10b_d2"):TEXT("j10b_d"))));
            }
            Test->TestEqual(TEXT("Texture operation does not move aircraft"),Unit->GetActorLocation(),Positions[Unit->UnitId]);
            Test->TestEqual(TEXT("Texture operation does not replace model"),Unit->Visual->GetStaticMesh()->GetName(),FString(Red?TEXT("J20S"):TEXT("J10B")));
        }
        if(!Test->HasAnyErrors() && ++Index<4){Send();return false;}
        Window->NativeDestruct(); Window.Reset(); Manager->ClearScenario(); World->DestroyWorld(false); return true;
    }
private:
    void Send()
    {
        const TCHAR* Commands[]={TEXT("把红方飞机的贴图换成第二款"),TEXT("把蓝方飞机的贴图换成第二款"),TEXT("把J20飞机的贴图换成第一款"),TEXT("把J10飞机的贴图换成第一款")};
        Window->Input->SetText(FText::FromString(Commands[Index])); Window->SendButton->OnClicked.Broadcast(); Deadline=FPlatformTime::Seconds()+300;
    }
    FAutomationTestBase* Test; UWorld* World=nullptr; ADemoScenarioManager* Manager=nullptr; TStrongObjectPtr<UDemoControlWidget> Window; TMap<FString,FVector> Positions; int32 Index=0; double Deadline=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextureQwenTest,"Integration.TextureQwen",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTextureQwenTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTextureQwenRun(this));return true;}
