#include "Misc/AutomationTest.h"
#include "ChatProtocol.h"
#include "ScenarioConfigLoader.h"
#include "DemoControlWidget.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "EquipmentLibrary.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/LineBatchComponent.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChatProtocolTest, "TacticalDemo.Chat.Protocol", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FChatProtocolTest::RunTest(const FString&)
{
    FDemoChatReply Reply;
    FString Error;
    TestTrue(TEXT("Movement instruction enters scene command path"),FDemoChatProtocol::IsSceneRequest(TEXT("把蓝方车辆移动到X=20米、Y=10米")));
    TestTrue(TEXT("Move-to instruction enters scene command path"),FDemoChatProtocol::IsSceneRequest(TEXT("将飞机移到坐标X=12米Y=30米")));
    TestTrue(TEXT("Model replacement enters scene command path"),FDemoChatProtocol::IsSceneRequest(TEXT("把红方飞机模型换成 aircraft_blue_basic")));
    TestTrue(TEXT("Route instruction enters scene command path"),FDemoChatProtocol::IsSceneRequest(TEXT("给红方飞机设置航迹，经过X=0米Y=0米和X=20米Y=10米")));
    TestFalse(TEXT("Asking whether model replacement exists remains plain chat"),FDemoChatProtocol::IsSceneRequest(TEXT("可以替换模型吗？")));
    TestFalse(TEXT("Chat prompts omit color editing capability"),FDemoChatProtocol::SystemPrompt(TEXT("创建两架飞机")).Contains(TEXT("颜色")));
    TestFalse(TEXT("Chat prompts omit color editing fields"),FDemoChatProtocol::SystemPrompt(TEXT("设置航迹")).Contains(TEXT("颜色")));
    TestTrue(TEXT("Plain chat"), FDemoChatProtocol::Parse(TEXT("你好，我可以帮助修改场景。"), Reply, Error));
    TestTrue(TEXT("Plain chat never produces scenario"), Reply.Scenario.IsEmpty());
    TestTrue(TEXT("JSON envelope"), FDemoChatProtocol::Parse(TEXT("{\"reply\":\"已准备\",\"scenario\":{\"scenarioName\":\"Empty\",\"units\":[]}}"), Reply, Error));
    TestFalse(TEXT("Has candidate"), Reply.Scenario.IsEmpty());
    TestFalse(TEXT("Invalid candidate rejected"), FDemoChatProtocol::Parse(TEXT("{\"reply\":\"bad\",\"scenario\":{\"units\":123}}"), Reply, Error));
    TestTrue(TEXT("Failure clears previous candidate"), Reply.Scenario.IsEmpty());
    TestFalse(TEXT("Wrong reply type"), FDemoChatProtocol::Parse(TEXT("{\"reply\":123}"), Reply, Error));
    TestTrue(TEXT("Fenced JSON"), FDemoChatProtocol::Parse(TEXT("```json\n{\"reply\":\"你好\",\"scenario\":null}\n```"), Reply, Error));
    TestTrue(TEXT("No candidate"), Reply.Scenario.IsEmpty());
    TestFalse(TEXT("Unsupported motion rejected"), FDemoChatProtocol::Parse(TEXT("{\"reply\":\"bad\",\"scenario\":{\"scenarioName\":\"X\",\"units\":[{\"speed\":12}]}}"), Reply, Error));
    TestFalse(TEXT("Malformed JSON"), FDemoChatProtocol::Parse(TEXT("{\"reply\":"), Reply, Error));
    TestTrue(TEXT("Validated bare scenario can omit reply"), FDemoChatProtocol::Parse(TEXT("{\"scenarioName\":\"Empty\",\"units\":[]}"), Reply, Error));
    TestFalse(TEXT("Bare scenario has visible apply guidance"), Reply.Reply.IsEmpty());
    TestTrue(TEXT("Valid wrapped scenario can omit explanation"), FDemoChatProtocol::Parse(TEXT("{\"scenario\":{\"scenarioName\":\"Empty\",\"units\":[]}}"), Reply, Error));
    TestFalse(TEXT("Flattened unit fields must not silently clear scene"), FDemoChatProtocol::Parse(TEXT("{\"reply\":\"ready\",\"scenario\":{\"scenarioName\":\"Demo\",\"units\":[],\"count\":3,\"position\":{\"x\":0,\"y\":0,\"z\":20}}}"), Reply, Error));
    TestFalse(TEXT("Empty reply with no scene is not a reply"), FDemoChatProtocol::Parse(TEXT("{\"reply\":\"  \",\"scenario\":null}"), Reply, Error));
    const FString Compact=TEXT("{\"create\":[{\"type\":\"aircraft\",\"side\":\"red\",\"count\":3,\"x\":2,\"y\":4,\"height\":20,\"spacing\":6},{\"type\":\"vehicle\",\"side\":\"blue\",\"count\":2}]}");
    const bool Created=FDemoChatProtocol::Parse(Compact,Reply,Error);
    TestTrue(TEXT("Compact create generates validated full scenario"),Created);
    const FString Current=Reply.Scenario;
    FDemoScenarioConfig Config;
    if(Created)
    {
        TestTrue(TEXT("Generated config loads"),FScenarioConfigLoader::Parse(Current,Config,Error));
        TestEqual(TEXT("Two generated groups"),Config.Units.Num(),2);
        TestEqual(TEXT("Coordinates map to meters"),Config.Units[0].PositionMeters,FVector(2,4,20));
        TestTrue(TEXT("Update targets aircraft"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"count\":5,\"side\":\"blue\",\"height\":30}}"),Reply,Error,Current));
        if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
        {
            TestEqual(TEXT("Count updated"),Config.Units[0].Count,5);
            TestEqual(TEXT("Side updated"),Config.Units[0].Side,FString(TEXT("blue")));
            TestEqual(TEXT("Height updated without moving XY"),Config.Units[0].PositionMeters,FVector(2,4,30));
            TestEqual(TEXT("Spacing preserved"),Config.Units[0].SpacingMeters,6.0);
            TestEqual(TEXT("Other type preserved"),Config.Units[1].Count,2);
        }
    const FString VerboseMove=TEXT("{\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"x\":20,\"y\":10,\"z\":5,\"height\":5,\"spacing\":5,\"modelId\":\"aircraft_red_basic\"}}");
    TestTrue(TEXT("Unrequested Qwen fields do not block a position-only move"),FDemoChatProtocol::Parse(VerboseMove,Reply,Error,Current,TEXT("把红方飞机整体移动到X=20米、Y=10米，保持高度和间距不变。")));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    {
        TestEqual(TEXT("Position-only move preserves height"),Config.Units[0].PositionMeters,FVector(20,10,20));
        TestEqual(TEXT("Position-only move preserves spacing"),Config.Units[0].SpacingMeters,6.0);
        TestEqual(TEXT("Position-only move preserves model id"),Config.Units[0].ModelId,FString(TEXT("aircraft_default")));
    }
    TestTrue(TEXT("Move command changes X/Y coordinates"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"vehicle\",\"targetSide\":\"blue\",\"set\":{\"x\":20,\"y\":10}}"),Reply,Error,Current));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("Move updates X/Y while preserving height"),Config.Units[1].PositionMeters,FVector(20,10,0)); }
    TestTrue(TEXT("Singleton array set used by Qwen is accepted"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":[{\"x\":20,\"y\":10}]}"),Reply,Error,Current));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("Array-form move updates aircraft coordinates"),Config.Units[0].PositionMeters,FVector(20,10,20)); }
    TestTrue(TEXT("Nested selector wrapper used by Qwen is normalized"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"targetSide\":\"red\",\"set\":{\"x\":20,\"y\":10}}}"),Reply,Error,Current));
    }
    const FString Mixed=TEXT("{\"scenarioName\":\"Mixed\",\"units\":[{\"id\":\"R\",\"modelId\":\"aircraft_default\",\"type\":\"aircraft\",\"side\":\"red\",\"count\":2,\"position\":{\"x\":0,\"y\":0,\"z\":0},\"spacing\":6},{\"id\":\"B\",\"modelId\":\"aircraft_default\",\"type\":\"aircraft\",\"side\":\"blue\",\"count\":3,\"position\":{\"x\":0,\"y\":0,\"z\":0},\"spacing\":6}]}");
    TestTrue(TEXT("Side selector updates only matching group"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"count\":5}}"),Reply,Error,Mixed));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("Red group selected"),Config.Units[0].Count,5); TestEqual(TEXT("Blue group preserved"),Config.Units[1].Count,3); }
    TestTrue(TEXT("User source side repairs omitted Qwen selector"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"count\":5}}"),Reply,Error,Mixed,TEXT("只把红方飞机数量改为5架。")));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("Omitted selector still changes red count"),Config.Units[0].Count,5); TestEqual(TEXT("Omitted selector preserves blue count"),Config.Units[1].Count,3); }
    TestTrue(TEXT("Height request drops unrequested destination side"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"height\":30,\"side\":\"blue\"}}"),Reply,Error,Mixed,TEXT("把红方飞机高度改为30米。")));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("Height request preserves red side"),Config.Units[0].Side,FString(TEXT("red"))); TestEqual(TEXT("Height request preserves blue height"),Config.Units[1].PositionMeters.Z,0.0); }
    TestTrue(TEXT("All-aircraft side request accepts destination repeated as targetSide"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"targetSide\":\"blue\",\"set\":{\"side\":\"blue\"}}"),Reply,Error,Mixed,TEXT("把当前场景中的飞机全部改为蓝方。")));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("All aircraft first group changes to blue"),Config.Units[0].Side,FString(TEXT("blue"))); TestEqual(TEXT("All aircraft second group remains blue"),Config.Units[1].Side,FString(TEXT("blue"))); }
    TestTrue(TEXT("All-aircraft side request recovers Qwen's invalid count-all echo"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"side\":\"blue\",\"targetSide\":\"blue\",\"set\":{\"count\":all}}}"),Reply,Error,Mixed,TEXT("把当前场景中的飞机全部改为蓝方。")));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("Recovered all-aircraft request changes first group to blue"),Config.Units[0].Side,FString(TEXT("blue"))); TestEqual(TEXT("Recovered all-aircraft request changes second group to blue"),Config.Units[1].Side,FString(TEXT("blue"))); }
    TestTrue(TEXT("Direct invalid count-all echo is also recovered"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"side\":\"blue\",\"targetSide\":\"blue\",\"count\": all}}"),Reply,Error,Mixed,TEXT("把当前场景中的飞机全部改为蓝方。")));
    TestFalse(TEXT("Invalid count token outside an all-scope instruction remains rejected"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"count\":all}}"),Reply,Error,Mixed,TEXT("把飞机数量改为3架。")));
    TestFalse(TEXT("Update needs current snapshot"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"count\":5}}"),Reply,Error));
    TestFalse(TEXT("Unknown target does not clear scene"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"radar\",\"set\":{\"count\":5}}"),Reply,Error,Current));
    TestFalse(TEXT("Unknown update field rejected"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"speed\":5}}"),Reply,Error,Current));
    TestTrue(TEXT("Model replacement is merged as a candidate"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"modelId\":\"aircraft_blue_basic\"}}"),Reply,Error,Current));
    TestTrue(TEXT("Registered matching model passes library validation"),FEquipmentLibrary::ValidateModelChanges(Current,Reply.Scenario,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"),Error));
    const FString WrongTypeCandidate=Reply.Scenario.Replace(TEXT("aircraft_blue_basic"),TEXT("vehicle_default"));
    TestFalse(TEXT("Registered model of wrong type is rejected"),FEquipmentLibrary::ValidateModelChanges(Current,WrongTypeCandidate,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"),Error));
    const FString UnknownModelCandidate=Reply.Scenario.Replace(TEXT("aircraft_blue_basic"),TEXT("unregistered_aircraft"));
    TestFalse(TEXT("Unregistered model id is rejected"),FEquipmentLibrary::ValidateModelChanges(Current,UnknownModelCandidate,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"),Error));
    TestTrue(TEXT("Model catalog lists registered ids"),FEquipmentLibrary::ModelCatalog(FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")).Contains(TEXT("aircraft_blue_basic(aircraft)")));
    TestTrue(TEXT("Static route is merged as a candidate"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"route\":[{\"x\":0,\"y\":0,\"z\":5},{\"x\":20,\"y\":10,\"z\":5}]}}"),Reply,Error,Current));
    TestFalse(TEXT("One point route is rejected"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"route\":[{\"x\":0,\"y\":0,\"z\":0}]}}"),Reply,Error,Current));
    TestFalse(TEXT("Color update is not an accepted chat operation"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"color\":\"red\"}}"),Reply,Error,Current));
    TestTrue(TEXT("Single repeated set wrapper is normalized"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"height\":30,\"set\":{\"side\":\"blue\"}}}"),Reply,Error,Current));
    TestTrue(TEXT("Malformed default count is preserved when nested side is present"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"count\":0,\"height\":30,\"set\":{\"side\":\"blue\"}}}"),Reply,Error,Current));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("Malformed default count remains unchanged"),Config.Units[0].Count,3); TestEqual(TEXT("Nested side is applied"),Config.Units[0].Side,FString(TEXT("blue"))); }
    TestFalse(TEXT("Numeric strings rejected in compact create"),FDemoChatProtocol::Parse(TEXT("{\"create\":[{\"type\":\"aircraft\",\"side\":\"red\",\"count\":\"3\"}]}"),Reply,Error));
    TestFalse(TEXT("Out of range update rejected"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"aircraft\",\"set\":{\"height\":1e20}}"),Reply,Error,Current));
    TestTrue(TEXT("Across-type selector respects source side"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"all\",\"targetSide\":\"red\",\"set\":{\"height\":30}}"),Reply,Error,Mixed));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==2)
    { TestEqual(TEXT("All-type red group updated"),Config.Units[0].PositionMeters.Z,30.0); TestEqual(TEXT("All-type blue group untouched"),Config.Units[1].PositionMeters.Z,0.0); }
    TestFalse(TEXT("Missing selected side must not broaden a move"),FDemoChatProtocol::Parse(TEXT("{\"update\":\"vehicle\",\"targetSide\":\"red\",\"set\":{\"x\":20}}"),Reply,Error,Current));
    TestTrue(TEXT("Creation ignores unrequested copied optional values"),FDemoChatProtocol::Parse(TEXT("{\"create\":[{\"type\":\"target\",\"side\":\"red\",\"count\":2,\"height\":50,\"spacing\":6,\"route\":[[0,0,5],[20,10,5]]}]}"),Reply,Error,FString(),TEXT("创建一个新场景，只放置2个红方目标。")));
    if(FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==1)
    { TestEqual(TEXT("Unrequested create height defaults to zero"),Config.Units[0].PositionMeters.Z,0.0); TestEqual(TEXT("Unrequested create spacing defaults to 10"),Config.Units[0].SpacingMeters,10.0); TestEqual(TEXT("Unrequested create route absent"),Config.Units[0].RouteMeters.Num(),0); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChatWindowTest, "TacticalDemo.Chat.WindowControls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FChatWindowTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>();
    auto* W=NewObject<UDemoControlWidget>(World);
    W->Initialize();
    // Native standalone widgets have no player context in this isolated test world.
    if(!W->Input) W->NativeOnInitialized();
    TestNotNull(TEXT("Input exists"),W->Input.Get());
    TestNotNull(TEXT("Chat history exists"),W->HistoryBox.Get());
    TestFalse(TEXT("Apply initially disabled"),W->ApplyButton->GetIsEnabled());
    TestFalse(TEXT("Initial status omits pending configuration label"),W->CandidateInfo->GetText().ToString().Contains(TEXT("待应用配置")));
    const auto& InputStyle=W->Input->WidgetStyle;
    auto Contrast=[](const FLinearColor& Ink,const FLinearColor& Paper)
    {
        auto Luminance=[](const FLinearColor& Color) { return .2126f*Color.R+.7152f*Color.G+.0722f*Color.B; };
        return (Luminance(Paper)+.05f)/(Luminance(Ink)+.05f);
    };
    TestTrue(TEXT("Entered text has strong contrast against normal input background"),Contrast(InputStyle.TextStyle.ColorAndOpacity.GetSpecifiedColor(),InputStyle.BackgroundImageNormal.TintColor.GetSpecifiedColor())>=7.f);
    TestTrue(TEXT("Focused input keeps strong contrast while typing"),Contrast(InputStyle.FocusedForegroundColor.GetSpecifiedColor(),InputStyle.BackgroundImageFocused.TintColor.GetSpecifiedColor())>=7.f);
    TestEqual(TEXT("Four resource source inputs exist"),W->ResourcePaths.Num(),4);
    TestEqual(TEXT("Resource window initially closed"),W->ResourceWindow->GetVisibility(),ESlateVisibility::Collapsed);
    W->ToggleResources();
    TestEqual(TEXT("Resource window opens"),W->ResourceWindow->GetVisibility(),ESlateVisibility::Visible);
    const FString SourcesPath=FPaths::ProjectSavedDir()/TEXT("ResourceSources.json");
    FString PreviousSources;
    const bool bHadSources=FFileHelper::LoadFileToString(PreviousSources,*SourcesPath);
    W->ResourcePaths[1]->SetText(FText::FromString(TEXT("E:/FutureAssets/Liveries")));
    W->SaveResources();
    W->ResourcePaths[1]->SetText(FText::GetEmpty());
    W->LoadResources();
    TestEqual(TEXT("Resource configuration survives reload"),W->ResourcePaths[1]->GetText().ToString(),FString(TEXT("E:/FutureAssets/Liveries")));
    if(bHadSources) FFileHelper::SaveStringToFile(PreviousSources,*SourcesPath);
    else IFileManager::Get().Delete(*SourcesPath);
    W->ToggleResources();
    W->Reload();
    const int32 Count=M->GetUnits().Num();
    TestTrue(TEXT("Load button restores units"),Count>0);
    W->ResourcePaths[0]->SetText(FText::FromString(FPaths::ProjectConfigDir()/TEXT("Scenario.json")));
    W->LoadResourceScene();
    TestEqual(TEXT("Resource window loads valid scene JSON"),M->GetUnits().Num(),Count);
    W->ResourcePaths[0]->SetText(FText::FromString(TEXT("E:/missing-scene.json")));
    W->LoadResourceScene();
    TestEqual(TEXT("Invalid resource path preserves scene"),M->GetUnits().Num(),Count);
    W->Endpoint->SetText(FText::GetEmpty());
    W->Input->SetText(FText::FromString(TEXT("你好")));
    W->SendButton->OnClicked.Broadcast();
    TestFalse(TEXT("Unconfigured endpoint starts no request"),W->ActiveRequest.IsValid());
    TestEqual(TEXT("Rejected send keeps scene"),M->GetUnits().Num(),Count);
    W->Candidate=TEXT("{\"scenarioName\":\"ChatEmpty\",\"units\":[]}");
    W->ApplyButton->SetIsEnabled(true);
    W->ApplyButton->OnClicked.Broadcast();
    TestEqual(TEXT("Apply delegates to manager"),M->GetUnits().Num(),0);
    TestTrue(TEXT("Applied candidate consumed"),W->Candidate.IsEmpty());
    TestEqual(TEXT("Applied status is concise"),W->CandidateInfo->GetText().ToString(),FString(TEXT("已应用到场景。")));
    W->Candidate=TEXT("stale"); W->ApplyButton->SetIsEnabled(true); W->Cancel();
    TestTrue(TEXT("Cancel drops candidate"),W->Candidate.IsEmpty());
    TestFalse(TEXT("Cancel disables apply"),W->ApplyButton->GetIsEnabled());
    TestTrue(TEXT("Context follows applied scene"),M->GetScenarioJson().Contains(TEXT("ChatEmpty")));
    W->Reload();
    TestEqual(TEXT("Original file preserved"),M->GetUnits().Num(),Count);
    W->Candidate=TEXT("stale"); W->Clear();
    TestTrue(TEXT("Clear drops stale candidate"),W->Candidate.IsEmpty());
    TestEqual(TEXT("Clear removes actors"),M->GetUnits().Num(),0);
    W->NativeDestruct();
    World->DestroyWorld(false);
    return true;
}

// Explicit integration fixture; never substitutes for verification of a real Qwen model.
class FChatHttpCommand : public IAutomationLatentCommand
{
public:
    explicit FChatHttpCommand(FAutomationTestBase* InTest):Test(InTest) {}
    virtual bool Update() override
    {
        if(!Window.IsValid())
        {
            FString Endpoint;
            if(!FParse::Value(FCommandLine::Get(),TEXT("DemoChatTestEndpoint="),Endpoint))
            { Test->AddError(TEXT("Run Scripts/TestChatHttp.ps1 to start the local fixture.")); return true; }
            const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            Manager=World->SpawnActor<ADemoScenarioManager>(); Manager->ReloadScenario();
            OriginalCount=Manager->GetUnits().Num();
            Window.Reset(NewObject<UDemoControlWidget>(World)); Window->Initialize();
            if(!Window->Input) Window->NativeOnInitialized();
            Window->Endpoint->SetText(FText::FromString(Endpoint)); Window->Model->SetText(FText::FromString(TEXT("test-fixture")));
            Window->Input->SetText(FText::FromString(TEXT("创建2架蓝方飞机"))); Window->SendButton->OnClicked.Broadcast();
            Deadline=FPlatformTime::Seconds()+15;
            return false;
        }
        if(Window->ActiveRequest.IsValid() && FPlatformTime::Seconds()<Deadline) return false;
        if(Window->ActiveRequest.IsValid()) Test->AddError(TEXT("Local HTTP fixture timed out"));
        if(Stage==0)
        {
            Test->TestFalse(TEXT("HTTP reply creates candidate"),Window->Candidate.IsEmpty());
            Test->TestEqual(TEXT("Complete conversation pair"),Window->Messages.Num(),2);
            Test->TestEqual(TEXT("HTTP proposal keeps scene unchanged until manual apply"),Manager->GetUnits().Num(),OriginalCount);
            Test->TestEqual(TEXT("Pending status is concise"),Window->CandidateInfo->GetText().ToString(),FString(TEXT("修改已就绪，点击应用。")));
            for(UWidget* Entry:Window->HistoryBox->GetAllChildren())
                if(const auto* Message=Cast<UTextBlock>(Entry))
                {
                    const FString VisibleText=Message->GetText().ToString();
                    Test->TestFalse(TEXT("Chat history omits raw candidate JSON"),VisibleText.Contains(TEXT("\"scenarioName\"")) || VisibleText.Contains(TEXT("\"units\"")));
                    Test->TestFalse(TEXT("Chat history omits pending configuration label"),VisibleText.Contains(TEXT("待应用配置")));
                }
            Window->ApplyButton->OnClicked.Broadcast();
            Test->TestEqual(TEXT("HTTP candidate applied"),Manager->GetUnits().Num(),2);
            Test->TestEqual(TEXT("Scenario title from response"),Manager->GetScenarioName(),FString(TEXT("HttpFixture")));
            Window->Input->SetText(FText::FromString(TEXT("无效响应测试"))); Window->SendButton->OnClicked.Broadcast();
            Deadline=FPlatformTime::Seconds()+15; Stage=1;
            return false;
        }
        Test->TestTrue(TEXT("Malformed HTTP candidate discarded"),Window->Candidate.IsEmpty());
        Test->TestEqual(TEXT("Malformed response preserves scene"),Manager->GetUnits().Num(),2);
        Test->TestTrue(TEXT("Send recovers after error"),Window->SendButton->GetIsEnabled());
        Test->TestEqual(TEXT("Failed turn removed from context"),Window->Messages.Num(),2);
        Window->NativeDestruct(); Window.Reset(); World->DestroyWorld(false);
        return true;
    }
private:
    FAutomationTestBase* Test;
    UWorld* World=nullptr;
    ADemoScenarioManager* Manager=nullptr;
    TStrongObjectPtr<UDemoControlWidget> Window;
    double Deadline=0;
    int32 OriginalCount=0;
    int32 Stage=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChatHttpTest,"Integration.LocalChatHTTP",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FChatHttpTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FChatHttpCommand(this));
    return true;
}

class FRealQwenCommand : public IAutomationLatentCommand
{
public:
    explicit FRealQwenCommand(FAutomationTestBase* InTest):Test(InTest) {}
    virtual bool Update() override
    {
        if(!Window.IsValid())
        {
            const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            Manager=World->SpawnActor<ADemoScenarioManager>(); Manager->ReloadScenario();
            OriginalCount=Manager->GetUnits().Num();
            Window.Reset(NewObject<UDemoControlWidget>(World)); Window->Initialize();
            if(!Window->Input) Window->NativeOnInitialized();
            Window->Endpoint->SetText(FText::FromString(TEXT("http://127.0.0.1:8000/v1/chat/completions")));
            Window->Model->SetText(FText::FromString(TEXT("qwen")));
            Window->Input->SetText(FText::FromString(TEXT("创建一个新场景，只放置3架红方飞机。位置为X=0米、Y=0米，高度20米，单位间距6米。")));
            Window->SendButton->OnClicked.Broadcast();
            Deadline=FPlatformTime::Seconds()+300;
            return false;
        }
        if(Window->ActiveRequest.IsValid() && FPlatformTime::Seconds()<Deadline) return false;
        Test->TestFalse(TEXT("Real Qwen request completed before timeout"),Window->ActiveRequest.IsValid());
        Test->TestEqual(TEXT("Real Qwen produced accepted conversation pairs"),Window->Messages.Num(),2*(Stage+1));
        Test->TestFalse(TEXT("Real Qwen produced a validated scenario"),Window->Candidate.IsEmpty());
        if(Stage==0) Test->TestEqual(TEXT("Proposal does not change scene before apply"),Manager->GetUnits().Num(),OriginalCount);
        Window->ApplyButton->OnClicked.Broadcast();
        const int32 ExpectedCount=Stage==0?3:5;
        Test->TestEqual(TEXT("Applied requested count"),Manager->GetUnits().Num(),ExpectedCount);
        if(Stage==0 && Manager->GetScenarioConfig().Units.Num()==1)
            AppliedSpacingMeters=Manager->GetScenarioConfig().Units[0].SpacingMeters;
        for(int32 Index=0;Index<Manager->GetUnits().Num();++Index)
        {
            auto* Unit=Manager->GetUnits()[Index].Get();
            Test->TestEqual(TEXT("Applied requested side"),Unit->Side,FString(Stage>=6?TEXT("blue"):TEXT("red")));
            const FVector ExpectedPosition(Stage>=2?2000:0,(Stage>=2?1000:0)+Index*AppliedSpacingMeters*100,2000);
            Test->TestEqual(TEXT("Applied requested position and preserved height/spacing"),Unit->GetActorLocation(),ExpectedPosition);
        }
        if(Stage>=1 && Manager->GetScenarioConfig().Units.Num()==1)
            Test->TestEqual(TEXT("Updates preserve the created group spacing"),Manager->GetScenarioConfig().Units[0].SpacingMeters,AppliedSpacingMeters);
        if(Stage==3 && Manager->GetUnits().Num())
            Test->TestEqual(TEXT("Registered model replacement reaches rendered mesh"),Manager->GetUnits()[0]->Visual->GetStaticMesh()->GetName(),FString(TEXT("Cube")));
        if(Stage==4)
        {
            Test->TestEqual(TEXT("Route candidate retains three waypoints"),Manager->GetScenarioConfig().Units[0].RouteMeters.Num(),3);
            const auto* Lines=Cast<ULineBatchComponent>(Manager->GetRootComponent());
            Test->TestEqual(TEXT("Three route points render as two static segments"),Lines?Lines->BatchedLines.Num():-1,2);
            Test->TestEqual(TEXT("Route does not move units"),Manager->GetUnits()[0]->GetActorLocation(),FVector(2000,1000,2000));
        }
        if(Stage==5)
        {
            Test->TestEqual(TEXT("Empty route clears configured waypoints"),Manager->GetScenarioConfig().Units[0].RouteMeters.Num(),0);
            const auto* Lines=Cast<ULineBatchComponent>(Manager->GetRootComponent());
            Test->TestEqual(TEXT("Cleared route leaves no drawn segments"),Lines?Lines->BatchedLines.Num():-1,0);
        }
        if(Stage<6 && !HasFailed())
        {
            ++Stage;
            const TCHAR* Commands[]={TEXT(""),TEXT("把当前场景中的飞机数量改为5架，其他参数保持不变。"),TEXT("把红方飞机整体移动到X=20米，Y=10米，保持高度和间距不变。"),TEXT("把红方飞机的模型替换为设备库中的 aircraft_blue_basic，其他配置保持不变。"),TEXT("给红方飞机设置静态航迹，按顺序经过坐标(0,0,20)、(10,10,25)、(20,0,20)，只保存和显示航点，不移动飞机。"),TEXT("清除红方飞机的航迹，保持飞机位置不变。"),TEXT("把当前场景中的飞机全部改为蓝方。")};
            Window->Input->SetText(FText::FromString(Commands[Stage]));
            Window->SendButton->OnClicked.Broadcast(); Deadline=FPlatformTime::Seconds()+300;
            return false;
        }
        Window->NativeDestruct(); Window.Reset(); World->DestroyWorld(false);
        return true;
    }
private:
    bool HasFailed() const { return Test->HasAnyErrors(); }
    FAutomationTestBase* Test;
    UWorld* World=nullptr;
    ADemoScenarioManager* Manager=nullptr;
    TStrongObjectPtr<UDemoControlWidget> Window;
    double Deadline=0;
    double AppliedSpacingMeters=10.0;
    int32 OriginalCount=0;
    int32 Stage=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealQwenTest,"Integration.RealQwen",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRealQwenTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FRealQwenCommand(this));
    return true;
}
