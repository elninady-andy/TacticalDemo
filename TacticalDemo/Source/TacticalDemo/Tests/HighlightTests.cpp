#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HighlightCommands.h"
#include "ScenarioConfigLoader.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "DemoControlWidget.h"
#include "ChatProtocol.h"
#include "ScenarioPresetStore.h"
#include "Components/StaticMeshComponent.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace HighlightTest
{
const FString Scene=TEXT(R"({"schemaVersion":2,"scenarioName":"Highlight","units":[{"id":"R1","modelId":"aircraft_red_basic","type":"aircraft","side":"red","count":2,"position":{"x":1,"y":2,"z":3},"spacing":10,"instances":[{"id":"R1_001","position":{"x":1,"y":2,"z":3},"yaw":90,"formation":"编队1"},{"id":"R1_002","position":{"x":1,"y":12,"z":3},"yaw":-45}]}]})");
UWorld* World()
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,CreatePackage(*(TEXT("/Temp/Highlight_")+FGuid::NewGuid().ToString(EGuidFormats::Digits))),true,ERHIFeatureLevel::Num,&Init);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftHighlightTest,"TacticalDemo.Highlight.StateAndCommands",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAircraftHighlightTest::RunTest(const FString&)
{
    auto* World=HighlightTest::World(); auto* M=World->SpawnActor<ADemoScenarioManager>();
    const FString Library=FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json");
    if(!TestTrue(TEXT("Load old scene without highlight"),M->LoadJson(HighlightTest::Scene,Library))) { World->DestroyWorld(false); return false; }
    const FString Before=M->SnapshotJson(); auto* First=M->FindUnit(TEXT("R1_001")); auto* Other=M->FindUnit(TEXT("R1_002"));
    auto* OriginalMaterial=First->Visual->GetMaterial(0); const FTransform OriginalTransform=First->GetActorTransform();
    FHighlightCommandResult R; FString Error; FDemoScenarioConfig Parsed;
    for(const TCHAR* Value:{TEXT("1"),TEXT("\"true\""),TEXT("null")})
        TestFalse(TEXT("Highlight must be boolean"),FScenarioConfigLoader::Parse(HighlightTest::Scene.Replace(TEXT("\"yaw\":90"),*(FString(TEXT("\"yaw\":90,\"highlight\":"))+Value)),Parsed,Error));
    TestTrue(TEXT("Explicit enable understood"),FHighlightCommands::Handle(TEXT("让 R1_001 发光高亮"),Before,TEXT(""),R));
    if(TestFalse(TEXT("Enable produces candidate"),R.Scenario.IsEmpty()))
    {
        const FString Candidate=R.Scenario;
        FScenarioConfigLoader::Parse(Candidate,Parsed,Error);
        TestTrue(TEXT("Round trip preserves highlight"),Parsed.Units[0].Instances[0].bHighlighted);
        TestFalse(TEXT("Only requested instance highlighted"),Parsed.Units[0].Instances[1].bHighlighted);
        auto Bad=Parsed; Bad.Units[0].Instances[0].PositionMeters.X+=10;
        TestFalse(TEXT("Cannot smuggle transform edit"),M->ApplyHighlightCandidate(FScenarioConfigLoader::Serialize(Bad)));
        Bad=Parsed; Bad.Units[0].Instances[0].Formation=TEXT("Other");
        TestFalse(TEXT("Cannot smuggle formation edit"),M->ApplyHighlightCandidate(FScenarioConfigLoader::Serialize(Bad)));
        TestFalse(TEXT("Formation candidate cannot smuggle highlight"),M->ApplyFormationCandidate(Candidate,true));
        TestEqual(TEXT("Rejected candidates atomic"),M->SnapshotJson(),Before);
        if(TestTrue(TEXT("Apply overlay"),M->ApplyHighlightCandidate(Candidate)))
        {
            TestNotNull(TEXT("Actual render overlay assigned"),First->Visual->GetOverlayMaterial());
            TestNull(TEXT("Other aircraft unaffected"),Other->Visual->GetOverlayMaterial());
            TestEqual(TEXT("Original material untouched"),First->Visual->GetMaterial(0),OriginalMaterial);
            TestTrue(TEXT("Aircraft stays still"),First->GetActorTransform().Equals(OriginalTransform));
            const FString Lit=M->SnapshotJson();
            TestTrue(TEXT("Undo highlight"),M->UndoEdit()); TestEqual(TEXT("Undo exact"),M->SnapshotJson(),Before); TestNull(TEXT("Undo removes overlay"),First->Visual->GetOverlayMaterial());
            TestTrue(TEXT("Redo highlight"),M->RedoEdit()); TestEqual(TEXT("Redo exact"),M->SnapshotJson(),Lit);
            FHighlightCommands::Handle(TEXT("取消 R1_001 高亮"),Lit,TEXT(""),R);
            TestTrue(TEXT("Disable applies"),M->ApplyHighlightCandidate(R.Scenario)); TestNull(TEXT("Disable removes overlay"),First->Visual->GetOverlayMaterial());
            M->UndoEdit();
            const FString MaterialPath=M->HighlightMaterialPath; M->HighlightMaterialPath=TEXT("/Game/Materials/MissingHighlight.MissingHighlight");
            FHighlightCommands::Handle(TEXT("让R1_002高亮"),Lit,TEXT(""),R);
            TestFalse(TEXT("Missing material rejects transaction"),M->ApplyHighlightCandidate(R.Scenario));
            TestEqual(TEXT("Missing material retains scene"),M->SnapshotJson(),Lit);
            M->UndoEdit(); M->RedoEdit();
            TestNotNull(TEXT("Failed load retains cached overlay for redo"),First->Visual->GetOverlayMaterial()); M->HighlightMaterialPath=MaterialPath;
            FDemoChatReply Reply;
            TestTrue(TEXT("Ordinary chat accepts highlighted state"),FDemoChatProtocol::Parse(TEXT(R"({"update":"aircraft","set":{"side":"blue"}})"),Reply,Error,Lit,TEXT("把飞机全部改为蓝方")));
            FScenarioConfigLoader::Parse(Reply.Scenario,Parsed,Error); TestTrue(TEXT("Ordinary chat retains per-plane highlight"),Parsed.Units[0].Instances[0].bHighlighted);
            const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests/Highlight-")+FGuid::NewGuid().ToString(); FScenarioPresetStore Store(Dir); FString Id,Saved;
            TestTrue(TEXT("Save highlighted preset"),Store.SaveNew(TEXT("高亮方案"),Lit,Id,Error));
            TestTrue(TEXT("Read highlighted preset"),Store.Read(Id,Saved,Error));
            TestTrue(TEXT("Reload highlighted preset"),M->LoadJson(Saved,Library));
            TestNotNull(TEXT("Reload restores actual overlay"),M->FindUnit(TEXT("R1_001"))->Visual->GetOverlayMaterial());
            IFileManager::Get().DeleteDirectory(*Dir,false,true);
        }
    }
    for(const TCHAR* Input:{TEXT("让未知飞机高亮"),TEXT("让红方飞机高亮"),TEXT("不要让R1_001高亮"),TEXT("让R1_001高亮并移动到X=10"),TEXT("让R1_001发红光高亮"),TEXT("让R1_001和R1_002高亮")})
    { TestTrue(TEXT("Unsupported command handled locally"),FHighlightCommands::Handle(Input,Before,TEXT(""),R)); TestTrue(*FString(Input),R.Scenario.IsEmpty()); }
    TestTrue(TEXT("Selected plane command"),FHighlightCommands::Handle(TEXT("让选中的飞机高亮"),Before,TEXT("R1_002"),R));
    if(FScenarioConfigLoader::Parse(R.Scenario,Parsed,Error)) TestTrue(TEXT("Selected identity used"),Parsed.Units[0].Instances[1].bHighlighted); else AddError(Error);
    FHighlightCommands::Handle(TEXT("让选中的飞机高亮"),Before,TEXT(""),R); TestTrue(TEXT("No selection cannot guess aircraft"),R.Scenario.IsEmpty());
    TestFalse(TEXT("Other chat untouched"),FHighlightCommands::Handle(TEXT("把红方飞机编成编队3"),Before,TEXT(""),R));
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHighlightChatWindowTest,"TacticalDemo.Highlight.ChatWindow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHighlightChatWindowTest::RunTest(const FString&)
{
    auto* World=HighlightTest::World(); auto* M=World->SpawnActor<ADemoScenarioManager>();
    M->LoadJson(HighlightTest::Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"));
    const FString Before=M->SnapshotJson(); auto* W=NewObject<UDemoControlWidget>(World); W->Initialize(); if(!W->Input) W->NativeOnInitialized();
    W->Endpoint->SetText(FText::FromString(TEXT("invalid")));
    auto Send=[&](const TCHAR* Input){ W->Input->SetText(FText::FromString(Input)); W->Send(); };
    Send(TEXT("让R1_001发光高亮"));
    TestFalse(TEXT("Local candidate generated"),W->Candidate.IsEmpty()); TestFalse(TEXT("No network request"),W->ActiveRequest.IsValid());
    TestEqual(TEXT("Manual apply required"),M->SnapshotJson(),Before); W->Apply();
    TestNotNull(TEXT("UI applies real overlay"),M->FindUnit(TEXT("R1_001"))->Visual->GetOverlayMaterial());
    Send(TEXT("取消R1_001高亮")); W->Apply(); TestEqual(TEXT("UI disable restores exact scene"),M->SnapshotJson(),Before);
    Send(TEXT("让R1_001高亮")); W->Cancel(); W->Apply(); TestEqual(TEXT("Cancel does not apply"),M->SnapshotJson(),Before);
    Send(TEXT("让R1_001高亮")); M->EditUnit(TEXT("R1_001"),FVector(20,30,40),0); const FString Edited=M->SnapshotJson(); W->Apply();
    TestEqual(TEXT("Stale highlight cannot overwrite edit"),M->SnapshotJson(),Edited); M->UndoEdit();
    W->Units->ClearOptions(); W->Units->AddOption(TEXT("R1_002")); W->Units->SetSelectedOption(TEXT("R1_002"));
    Send(TEXT("让选中的飞机高亮")); W->Apply();
    TestNotNull(TEXT("Selected aircraft overlay"),M->FindUnit(TEXT("R1_002"))->Visual->GetOverlayMaterial());
    TestNull(TEXT("Unselected aircraft stays normal"),M->FindUnit(TEXT("R1_001"))->Visual->GetOverlayMaterial());
    const FString BeforeMixed=M->SnapshotJson(); Send(TEXT("让R1_001高亮并把编队1移动到X=20米"));
    TestTrue(TEXT("Mixed highlight and formation operation is rejected"),W->Candidate.IsEmpty()); TestEqual(TEXT("Mixed request preserves scene"),M->SnapshotJson(),BeforeMixed);
    Send(TEXT("把红方飞机编成高亮编队"));
    TestTrue(TEXT("Effect word in new formation name retains formation routing"),W->bFormationCandidate && !W->bHighlightCandidate && !W->Candidate.IsEmpty());
    W->Apply(); TestEqual(TEXT("Formation named after effect can be created"),M->GetScenarioConfig().Units[0].Instances[0].Formation,FString(TEXT("高亮编队")));
    Send(TEXT("查看高亮编队成员")); TestFalse(TEXT("Formation query stays local"),W->ActiveRequest.IsValid()); TestTrue(TEXT("Query has no effect candidate"),W->Candidate.IsEmpty());
    for(const TCHAR* Query:{TEXT("查看高亮编队有哪些成员？"),TEXT("调用高亮编队！")})
    {
        FHighlightCommandResult Routed;
        TestFalse(TEXT("All established formation name boundaries retain routing"),FHighlightCommands::Handle(Query,M->SnapshotJson(),TEXT(""),Routed));
        Send(Query); TestFalse(TEXT("Punctuated formation query stays local"),W->ActiveRequest.IsValid());
    }
    Send(TEXT("把高亮编队排成V形，间距20米")); TestTrue(TEXT("Arrangement with effect word still works"),W->bFormationCandidate && !W->Candidate.IsEmpty()); W->Apply();
    Send(TEXT("把高亮编队改名为发光编队")); TestTrue(TEXT("Rename with effect words still works"),W->bFormationMembershipOnly && !W->Candidate.IsEmpty()); W->Apply();
    TestEqual(TEXT("Rename applied"),M->GetScenarioConfig().Units[0].Instances[0].Formation,FString(TEXT("发光编队")));
    TestNotNull(TEXT("Formation changes preserve overlay"),M->FindUnit(TEXT("R1_002"))->Visual->GetOverlayMaterial());
    Send(TEXT("把发光编队改名为高亮")); W->Apply();
    const FString KeywordNameScene=M->SnapshotJson(); Send(TEXT("让R1_001高亮并把高亮整体移动到X=80米"));
    TestTrue(TEXT("Formation name must not mask a real effect operation"),W->Candidate.IsEmpty()); TestEqual(TEXT("Mixed keyword-name request is atomic"),M->SnapshotJson(),KeywordNameScene);
    W->NativeDestruct(); World->DestroyWorld(false); return true;
}
#endif
