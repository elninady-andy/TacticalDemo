#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ScenarioConfigLoader.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ScenarioPresetStore.h"
#include "ChatProtocol.h"
#include "DemoGameMode.h"
#include "DemoLayoutWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "DemoControlWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/CheckBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "FormationCommands.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLayoutWindowDragTest,"TacticalDemo.Layout.FloatingWindow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLayoutWindowDragTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* Canvas=NewObject<UCanvasPanel>(World); auto* W=NewObject<UDemoLayoutWidget>(World); W->EnsureInitialized();
    auto* PanelSlot=Canvas->AddChildToCanvas(W); PanelSlot->SetAnchors(FAnchors(.5f,.5f));
    PanelSlot->SetAlignment(FVector2D(.5,.5)); PanelSlot->SetSize(FVector2D(760,660));
    const auto G=FGeometry::MakeRoot(FVector2D(1200,900),FSlateLayoutTransform(2.f,FVector2D(100,50)));
    if(TestTrue(TEXT("Floating window starts dragging"),W->BeginWindowDrag(FVector2D(400,300),G)))
    {
        W->MoveWindowDrag(FVector2D(600,400),G);
        TestTrue(TEXT("Drag respects viewport scale"),PanelSlot->GetPosition().Equals(FVector2D(100,50),.001));
        W->MoveWindowDrag(FVector2D(10000,10000),G);
        TestTrue(TEXT("Drag clamped at bottom right"),PanelSlot->GetPosition().Equals(FVector2D(220,120),.001));
        W->MoveWindowDrag(FVector2D(-10000,-10000),G);
        TestTrue(TEXT("Drag clamped at top left"),PanelSlot->GetPosition().Equals(FVector2D(-220,-120),.001));
        W->NativeOnMouseCaptureLost(FCaptureLostEvent());
        W->MoveWindowDrag(FVector2D(600,400),G);
        TestTrue(TEXT("Capture loss ends drag"),PanelSlot->GetPosition().Equals(FVector2D(-220,-120),.001));
        W->Close(); W->SetVisibility(ESlateVisibility::Visible);
        TestTrue(TEXT("Reopen retains position"),PanelSlot->GetPosition().Equals(FVector2D(-220,-120),.001));
    }
    World->DestroyWorld(false); return true;
}

namespace LayoutTest
{
const FString Scene=TEXT(R"({"schemaVersion":2,"scenarioName":"Layout","units":[{"id":"R1","modelId":"aircraft_red_basic","type":"aircraft","side":"red","count":2,"position":{"x":1,"y":2,"z":3},"spacing":10,"instances":[{"id":"R1_001","position":{"x":20,"y":30,"z":4},"yaw":90},{"id":"R1_002","position":{"x":1,"y":12,"z":3},"yaw":-45}]}]})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationChatWindowTest,"TacticalDemo.Layout.FormationChat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFormationChatWindowTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>();
    M->LoadJson(LayoutTest::Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"));
    const FString Unassigned=M->SnapshotJson();
    auto* Creation=NewObject<UDemoControlWidget>(World); Creation->Initialize(); if(!Creation->Input) Creation->NativeOnInitialized();
    Creation->Endpoint->SetText(FText::FromString(TEXT("invalid")));
    Creation->Input->SetText(FText::FromString(TEXT("把红方飞机编成编队3"))); Creation->Send();
    TestEqual(TEXT("Natural creation waits for manual apply"),M->SnapshotJson(),Unassigned);
    TestFalse(TEXT("Natural creation produces a local candidate"),Creation->Candidate.IsEmpty());
    TestFalse(TEXT("Formation creation needs no Qwen request"),Creation->ActiveRequest.IsValid());
    if(!Creation->Candidate.IsEmpty())
    {
        FDemoScenarioConfig Tampered; FString ValidationError;
        FScenarioConfigLoader::Parse(Creation->Candidate,Tampered,ValidationError);
        Tampered.Units[0].Instances[0].Yaw=180;
        TestFalse(TEXT("Membership candidate cannot smuggle transform edits"),M->ApplyFormationCandidate(FScenarioConfigLoader::Serialize(Tampered),true));
        TestFalse(TEXT("Transform candidate cannot smuggle membership edits"),M->ApplyFormationCandidate(Creation->Candidate));
        TestEqual(TEXT("Rejected candidates preserve state"),M->SnapshotJson(),Unassigned);
        Creation->Apply();
        TestEqual(TEXT("Natural creation assigns all selected aircraft"),M->GetScenarioConfig().Units[0].Instances[1].Formation,FString(TEXT("编队3")));
        TestTrue(TEXT("Natural creation is undoable"),M->UndoEdit());
        TestEqual(TEXT("Natural creation undo restores exact scene"),M->SnapshotJson(),Unassigned);
        TestTrue(TEXT("Natural creation can redo"),M->RedoEdit());
        TestEqual(TEXT("Redo restores members"),M->GetScenarioConfig().Units[0].Instances[0].Formation,FString(TEXT("编队3")));
        M->UndoEdit();
    }
    Creation->Input->SetText(FText::FromString(TEXT("把红方飞机编成编队3"))); Creation->Send(); Creation->Cancel(); Creation->Apply();
    TestEqual(TEXT("Cancelled membership never applies"),M->SnapshotJson(),Unassigned);
    Creation->Input->SetText(FText::FromString(TEXT("把红方飞机编成编队3"))); Creation->Send();
    M->EditUnit(TEXT("R1_001"),FVector(40,50,60),90); const FString Edited=M->SnapshotJson(); Creation->Apply();
    TestEqual(TEXT("Stale membership candidate cannot overwrite a newer edit"),M->SnapshotJson(),Edited);
    M->UndoEdit();
    Creation->NativeDestruct();
    auto* Highlight=NewObject<UDemoControlWidget>(World); Highlight->Initialize(); if(!Highlight->Input) Highlight->NativeOnInitialized();
    Highlight->Endpoint->SetText(FText::FromString(TEXT("invalid")));
    Highlight->Input->SetText(FText::FromString(TEXT("让R1_001发光高亮"))); Highlight->Send();
    TestFalse(TEXT("Highlight produces a local manual candidate"),Highlight->Candidate.IsEmpty());
    TestEqual(TEXT("Highlight waits for apply"),M->SnapshotJson(),Unassigned);
    TestFalse(TEXT("Highlight needs no Qwen request"),Highlight->ActiveRequest.IsValid());
    Highlight->NativeDestruct();
    M->SetFormationMembers(TEXT("编队1"),{TEXT("R1_001"),TEXT("R1_002")});
    const FString Original=M->SnapshotJson(); FFormationCommandResult R; FString Error;
    TestTrue(TEXT("Named formation recognized"),FFormationCommands::Handle(TEXT("调用编队1"),Original,R));
    TestEqual(TEXT("Existing formation selected"),R.SelectName,FString(TEXT("编队1")));
    TestTrue(TEXT("Arrange command recognized"),FFormationCommands::Handle(TEXT("把编队1排成纵列，间距10米，方向90度，起点X=100米、Y=200米"),Original,R));
    if(TestFalse(TEXT("Arrange generates candidate"),R.Scenario.IsEmpty()))
    {
        TestTrue(TEXT("Apply formation as undoable edit"),M->ApplyFormationCandidate(R.Scenario));
        TestTrue(TEXT("Requested geometry applied"),M->FindUnit(TEXT("R1_002"))->GetActorLocation().Equals(FVector(10000,19000,300),.01));
        M->UndoEdit(); TestEqual(TEXT("Chat formation undo exact"),M->SnapshotJson(),Original);
    }
    auto* W=NewObject<UDemoControlWidget>(World); W->Initialize(); if(!W->Input) W->NativeOnInitialized();
    W->Endpoint->SetText(FText::FromString(TEXT("invalid")));
    W->Input->SetText(FText::FromString(TEXT("把编队1整体移动到X=100米、Y=200米"))); W->Send();
    TestEqual(TEXT("Sending does not auto apply"),M->SnapshotJson(),Original);
    if(TestFalse(TEXT("Window exposes manual candidate"),W->Candidate.IsEmpty()))
    {
        W->Apply();
        TestEqual(TEXT("Leader moved"),M->FindUnit(TEXT("R1_001"))->GetActorLocation(),FVector(10000,20000,400));
        TestEqual(TEXT("Relative offset preserved"),M->FindUnit(TEXT("R1_002"))->GetActorLocation(),FVector(8100,18200,300));
        TestTrue(TEXT("Movement retains yaw"),FMath::IsNearlyEqual(M->FindUnit(TEXT("R1_001"))->GetActorRotation().Yaw,90.,.001));
        M->UndoEdit(); TestEqual(TEXT("Window movement undo exact"),M->SnapshotJson(),Original);
    }
    for(const FString& S:{FString(TEXT("调用编队9")),FString(TEXT("把编队1排成横排，间距-1米")),FString(TEXT("把编队1移动到X=abc、Y=20米")),FString(TEXT("把编队1排成横排，同时换成J10"))})
    {
        TestTrue(TEXT("Invalid formation instruction handled"),FFormationCommands::Handle(S,Original,R));
        TestTrue(TEXT("Invalid command generates no mutation"),R.Scenario.IsEmpty());
        TestTrue(TEXT("Invalid command explained"),!R.Reply.IsEmpty());
    }
    TestFalse(TEXT("Other chat untouched"),FFormationCommands::Handle(TEXT("把红方飞机换成J10"),Original,R));
    for(const FString& S:{FString(TEXT("把编队1排成横队")),FString(TEXT("把编队1排成纵队")),FString(TEXT("把编队1排成V形")),FString(TEXT("把编队1高度改为30米")),FString(TEXT("把编队1朝向改为120度"))})
    {
        TestTrue(TEXT("Extended formation instruction handled"),FFormationCommands::Handle(S,Original,R));
        TestFalse(TEXT("Extended operation generates candidate"),R.Scenario.IsEmpty());
        FDemoScenarioConfig Extended;
        if(FScenarioConfigLoader::Parse(R.Scenario,Extended,Error))
        {
            if(S.Contains(TEXT("高度"))) TestEqual(TEXT("Whole formation height"),Extended.Units[0].Instances[1].PositionMeters.Z,30.);
            if(S.Contains(TEXT("朝向")))
            {
                TestEqual(TEXT("Whole formation heading"),Extended.Units[0].Instances[1].Yaw,120.);
                TestEqual(TEXT("Heading does not rearrange"),Extended.Units[0].Instances[1].PositionMeters,FVector(1,12,3));
            }
        }
    }
    TestTrue(TEXT("List query handled"),FFormationCommands::Handle(TEXT("显示编队列表"),Original,R));
    TestTrue(TEXT("Query lists saved formation"),R.Reply.Contains(TEXT("编队1")));
    TestTrue(TEXT("Member query handled"),FFormationCommands::Handle(TEXT("查看编队1成员"),Original,R));
    TestTrue(TEXT("Member query contains identity"),R.Reply.Contains(TEXT("R1_002")));
    auto PrefixConfig=M->GetScenarioConfig();
    PrefixConfig.Units[0].Instances[0].Formation=TEXT("晨曦");
    PrefixConfig.Units[0].Instances[1].Formation=TEXT("晨曦队");
    TestTrue(TEXT("Multiple overlapping names handled"),FFormationCommands::Handle(TEXT("调用晨曦和晨曦队"),FScenarioConfigLoader::Serialize(PrefixConfig),R));
    TestTrue(TEXT("Multiple overlapping names rejected"),R.SelectName.IsEmpty());
    PrefixConfig.Units[0].Instances[1].Formation=TEXT("晨曦");
    const FString PrefixScene=FScenarioConfigLoader::Serialize(PrefixConfig);
    TestTrue(TEXT("Unknown extended name handled"),FFormationCommands::Handle(TEXT("把晨曦二队整体移动到X=550米、Y=-515米"),PrefixScene,R));
    TestTrue(TEXT("Unknown extended name rejected"),R.Scenario.IsEmpty());
    TestFalse(TEXT("Conversation mentioning name goes to Qwen"),FFormationCommands::Handle(TEXT("晨曦这个名字有什么含义？"),PrefixScene,R));
    TestTrue(TEXT("Malformed duplicate coordinate handled"),FFormationCommands::Handle(TEXT("把编队1移动到X=550米、X=abc、Y=-515米"),Original,R));
    TestTrue(TEXT("Malformed duplicate coordinate rejected"),R.Scenario.IsEmpty());
    for(const FString& S:{FString(TEXT("调用编队1？")),FString(TEXT("查看编队1有哪些成员？"))})
    {
        TestTrue(TEXT("Punctuated formation query handled"),FFormationCommands::Handle(S,Original,R));
        TestEqual(TEXT("Punctuated query selects existing name"),R.SelectName,FString(TEXT("编队1")));
    }
    TestTrue(TEXT("Unknown prefixed name handled"),FFormationCommands::Handle(TEXT("把新晨曦整体移动到X=550米、Y=-515米"),PrefixScene,R));
    TestTrue(TEXT("Unknown prefixed name rejected"),R.Scenario.IsEmpty());
    W->Input->SetText(FText::FromString(TEXT("把编队1高度改为30米"))); W->Send(); W->Apply();
    TestEqual(TEXT("Height candidate applies to actor"),M->FindUnit(TEXT("R1_002"))->GetActorLocation(),FVector(100,1200,3000));
    M->UndoEdit(); TestEqual(TEXT("Height undo exact"),M->SnapshotJson(),Original);
    W->Input->SetText(FText::FromString(TEXT("把编队1朝向改为120度"))); W->Send(); W->Apply();
    TestTrue(TEXT("Heading candidate applies to actor"),FMath::IsNearlyEqual(M->FindUnit(TEXT("R1_002"))->GetActorRotation().Yaw,120.,.001));
    M->UndoEdit(); TestEqual(TEXT("Heading undo exact"),M->SnapshotJson(),Original);
    for(const FString& S:{FString(TEXT("把编队1高度改为abc")),FString(TEXT("把编队1高度改为2000000米")),FString(TEXT("把编队1朝向改为120度，方向90度")),FString(TEXT("把编队1高度改为30米，朝向90度"))})
    {
        TestTrue(TEXT("Invalid extended command handled"),FFormationCommands::Handle(S,Original,R));
        TestTrue(TEXT("Invalid extended command rejected"),R.Scenario.IsEmpty());
    }
    W->NativeDestruct(); World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationLanguageTest,"TacticalDemo.Layout.FormationLanguage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFormationLanguageTest::RunTest(const FString&)
{
    FString Error; FDemoScenarioConfig Base;
    FScenarioConfigLoader::Parse(LayoutTest::Scene,Base,Error);
    auto Blue=Base.Units[0]; Blue.Id=TEXT("B1"); Blue.Side=TEXT("blue"); Blue.ModelId=TEXT("F35");
    Blue.Instances[0].Id=TEXT("B1_001"); Blue.Instances[1].Id=TEXT("B1_002"); Base.Units.Add(Blue);
    const FString Original=FScenarioConfigLoader::Serialize(Base);
    FFormationCommandResult R;
    auto Command=[&](const FString& Input,const FString& Source,FDemoScenarioConfig& Parsed)
    {
        TestTrue(*Input,FFormationCommands::Handle(Input,Source,R));
        TestFalse(*(Input+TEXT(" has candidate")),R.Scenario.IsEmpty());
        return !R.Scenario.IsEmpty() && FScenarioConfigLoader::Parse(R.Scenario,Parsed,Error);
    };
    FDemoScenarioConfig Created;
    if(Command(TEXT("把红方飞机编成编队3"),Original,Created))
    {
        TestEqual(TEXT("Red selected"),Created.Units[0].Instances[1].Formation,FString(TEXT("编队3")));
        TestTrue(TEXT("Blue excluded"),Created.Units[1].Instances[0].Formation.IsEmpty());
        TestEqual(TEXT("Creation preserves position"),Created.Units[0].Instances[1].PositionMeters,Base.Units[0].Instances[1].PositionMeters);
        const FString Assigned=FScenarioConfigLoader::Serialize(Created);
        FDemoScenarioConfig Changed;
        if(Command(TEXT("把F-35飞机加入编队3"),Assigned,Changed))
            TestEqual(TEXT("Model selector adds blue members"),Changed.Units[1].Instances[1].Formation,FString(TEXT("编队3")));
        if(Command(TEXT("把R1_001移出编队3"),Assigned,Changed))
        {
            TestTrue(TEXT("Only specified member removed"),Changed.Units[0].Instances[0].Formation.IsEmpty());
            TestEqual(TEXT("Other member retained"),Changed.Units[0].Instances[1].Formation,FString(TEXT("编队3")));
        }
        if(Command(TEXT("把R1_001从编队3中移出"),Assigned,Changed))
            TestTrue(TEXT("Natural from-formation phrasing"),Changed.Units[0].Instances[0].Formation.IsEmpty());
        if(Command(TEXT("把编队3改名为护航编队"),Assigned,Changed))
            TestEqual(TEXT("Rename all members"),Changed.Units[0].Instances[1].Formation,FString(TEXT("护航编队")));
        if(Command(TEXT("解散编队3"),Assigned,Changed))
        {
            TestTrue(TEXT("Dissolve removes membership"),Changed.Units[0].Instances[1].Formation.IsEmpty());
            TestEqual(TEXT("Dissolve retains aircraft"),Changed.Units.Num(),Base.Units.Num());
        }
        for(const TCHAR* Bad:{TEXT("把蓝方飞机编成编队3"),TEXT("把R1_001、missing加入编队3"),TEXT("把红方飞机加入编队9"),TEXT("把红方飞机编成编队4，同时排成V形"),TEXT("不要把红方飞机编成编队4"),TEXT("把部分红方飞机编成编队4"),TEXT("把红方飞机加入编队3并删除蓝方飞机"),TEXT("把蓝方飞机移出编队3"),TEXT("把编队3改名为编队4然后移动"),TEXT("解散编队3并删除飞机")})
        {
            TestTrue(TEXT("Invalid management handled locally"),FFormationCommands::Handle(Bad,Assigned,R));
            TestTrue(*FString(Bad),R.Scenario.IsEmpty());
        }
    }
    FDemoScenarioConfig Mixed;
    if(Command(TEXT("把R1_001、B1_002编成混合编队"),Original,Mixed))
    {
        TestEqual(TEXT("Explicit cross-side member"),Mixed.Units[1].Instances[1].Formation,FString(TEXT("混合编队")));
        TestTrue(TEXT("Unselected member stays free"),Mixed.Units[1].Instances[0].Formation.IsEmpty());
        if(Command(TEXT("把红方飞机编成编队4"),FScenarioConfigLoader::Serialize(Mixed),Created))
            TestTrue(TEXT("Reassignment disclosed"),R.Reply.Contains(TEXT("转入")));
    }
    Command(TEXT("创建编队5，成员是蓝方飞机"),Original,Created);
    for(const TCHAR* Name:{TEXT("和平编队"),TEXT("当前编队"),TEXT("所有编队"),TEXT("与风同行")})
    {
        auto Named=Base;
        for(auto& I:Named.Units[0].Instances) I.Formation=Name;
        Named.Units[1].Instances[0].Formation=TEXT("编队3");
        const FString Source=FScenarioConfigLoader::Serialize(Named);
        if(Command(FString(TEXT("把"))+Name+TEXT("成员加入编队3"),Source,Created))
            TestEqual(TEXT("Full custom name wins over prefix and connector words"),Created.Units[0].Instances[1].Formation,FString(TEXT("编队3")));
        if(Command(FString(TEXT("把"))+Name+TEXT("成员和B1_002加入编队3"),Source,Created))
        {
            TestEqual(TEXT("Custom name in mixed list"),Created.Units[0].Instances[1].Formation,FString(TEXT("编队3")));
            TestEqual(TEXT("Connector after full name still separates members"),Created.Units[1].Instances[1].Formation,FString(TEXT("编队3")));
        }
    }
    TestTrue(TEXT("Formation help handled locally"),FFormationCommands::Handle(TEXT("编队帮助"),Original,R));
    TestTrue(TEXT("Help has creation example"),R.Reply.Contains(TEXT("编成编队3")));
    TestTrue(TEXT("Aircraft identities discoverable"),FFormationCommands::Handle(TEXT("查看飞机编号"),Original,R));
    TestTrue(TEXT("Identity listing contains actual id"),R.Reply.Contains(TEXT("B1_002")));
    TestFalse(TEXT("Other aircraft edit still uses Qwen"),FFormationCommands::Handle(TEXT("把红方飞机换成J10"),Original,R));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationTest,"TacticalDemo.Layout.FreeFormation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFormationTest::RunTest(const FString&)
{
    FString Error; FDemoScenarioConfig Parsed;
    const FString WithFormation=LayoutTest::Scene.Replace(TEXT("\"yaw\":90"),TEXT("\"yaw\":90,\"formation\":\"编队1\""));
    TestTrue(TEXT("Formation metadata supported"),FScenarioConfigLoader::Parse(WithFormation,Parsed,Error));
    TestTrue(TEXT("Membership round trips"),FScenarioConfigLoader::Serialize(Parsed).Contains(TEXT("编队1")));
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>();
    M->LoadJson(LayoutTest::Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"));
    const FString Before=M->SnapshotJson();
    if(TestTrue(TEXT("Choose independent members"),M->SetFormationMembers(TEXT("编队1"),{TEXT("R1_001"),TEXT("R1_002")})))
    {
        const FString Assigned=M->SnapshotJson();
        TestTrue(TEXT("Assignment saved"),Assigned.Contains(TEXT("编队1")));
        TestTrue(TEXT("Assignment undo"),M->UndoEdit()); TestEqual(TEXT("Assignment undone exactly"),M->SnapshotJson(),Before); M->RedoEdit();
        TestTrue(TEXT("Row layout"),M->ArrangeFormation(TEXT("编队1"),0,35,0,FVector2D(100,200)));
        TestEqual(TEXT("Leader at anchor"),M->FindUnit(TEXT("R1_001"))->GetActorLocation(),FVector(10000,20000,400));
        TestEqual(TEXT("Row member and own height"),M->FindUnit(TEXT("R1_002"))->GetActorLocation(),FVector(10000,23500,300));
        M->UndoEdit(); TestEqual(TEXT("One undo restores whole layout"),M->SnapshotJson(),Assigned); M->RedoEdit();
        TestTrue(TEXT("Column rotated"),M->ArrangeFormation(TEXT("编队1"),1,10,90,FVector2D(100,200)));
        TestTrue(TEXT("Column behind rotated leader"),M->FindUnit(TEXT("R1_002"))->GetActorLocation().Equals(FVector(10000,19000,300),.01));
        const FString Stable=M->SnapshotJson();
        TestFalse(TEXT("Out of bounds layout rejected"),M->ArrangeFormation(TEXT("编队1"),0,100,0,FVector2D(0,1000000)));
        TestEqual(TEXT("Failure atomic"),M->SnapshotJson(),Stable);
        TestFalse(TEXT("Unknown members rejected"),M->SetFormationMembers(TEXT("编队2"),{TEXT("missing")}));
        TestFalse(TEXT("Blank name rejected"),M->SetFormationMembers(TEXT(" "),{TEXT("R1_001")}));
        TestFalse(TEXT("Duplicate members rejected"),M->SetFormationMembers(TEXT("编队2"),{TEXT("R1_001"),TEXT("R1_001")}));
        TestEqual(TEXT("Bad membership preserves state"),M->SnapshotJson(),Stable);
        TestTrue(TEXT("Move member between formations"),M->SetFormationMembers(TEXT("编队2"),{TEXT("R1_002")}));
        const FVector Other=M->FindUnit(TEXT("R1_002"))->GetActorLocation();
        TestTrue(TEXT("Only requested formation arranged"),M->ArrangeFormation(TEXT("编队1"),2,20,45,FVector2D(50,50)));
        TestEqual(TEXT("Other formation unchanged"),M->FindUnit(TEXT("R1_002"))->GetActorLocation(),Other);
        TestTrue(TEXT("Saved formation reloads"),M->LoadJson(M->SnapshotJson(),FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")));
        TestTrue(TEXT("Restored membership can arrange"),M->ArrangeFormation(TEXT("编队2"),0,20,0,FVector2D(80,80)));
    }
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationWindowTest,"TacticalDemo.Layout.FormationWindow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFormationWindowTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>(); FString Json,Error;
    FFileHelper::LoadFileToString(Json,*(FPaths::ProjectConfigDir()/TEXT("AirportScenario.json")));
    if(M->LoadJson(Json,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")) && M->GetUnits().Num()==6)
    {
        auto* W=NewObject<UDemoLayoutWidget>(World); W->EnsureInitialized(); W->ShowFormations();
        TestEqual(TEXT("All aircraft available"),W->MemberChecks.Num(),6);
        W->MemberChecks[0]->SetIsChecked(true); W->MemberChecks[3]->SetIsChecked(true); W->MemberChecks[4]->SetIsChecked(true);
        W->ShowUnits(); W->ShowFormations(); TestTrue(TEXT("Switching tabs preserves member draft"),W->MemberChecks[0]->IsChecked());
        const FString Before=M->SnapshotJson(); W->ApplyFormation();
        TestEqual(TEXT("Unsaved members not silently arranged"),M->SnapshotJson(),Before);
        W->FormationName->SetText(FText::FromString(TEXT("编队9"))); W->SaveFormation();
        TestEqual(TEXT("Custom formation selected"),W->FormationPicker->GetSelectedOption(),FString(TEXT("编队9")));
        const FString Assigned=M->SnapshotJson(); TArray<FVector> Positions;
        for(ADemoUnitActor* U:M->GetUnits()) Positions.Add(U->GetActorLocation());
        W->PatternPicker->SetSelectedIndex(2);
        const TCHAR* Values[]={TEXT("550"),TEXT("-515"),TEXT("35"),TEXT("0")};
        for(int32 I=0;I<4;++I) W->FormationFields[I]->SetText(FText::FromString(Values[I]));
        W->ApplyFormation();
        TestTrue(TEXT("Mixed-side wedge left wing"),M->GetUnits()[3]->GetActorLocation().Equals(FVector(51500,-55000,Positions[3].Z),.01));
        TestTrue(TEXT("Mixed-side wedge right wing"),M->GetUnits()[4]->GetActorLocation().Equals(FVector(51500,-48000,Positions[4].Z),.01));
        for(int32 I:{1,2,5}) TestEqual(TEXT("Unselected aircraft unchanged"),M->GetUnits()[I]->GetActorLocation(),Positions[I]);
        W->Undo(); TestEqual(TEXT("Window undo restores all members"),M->SnapshotJson(),Assigned); W->Redo();
        FDemoChatReply Reply;
        TestTrue(TEXT("Chat accepts scene with free formations"),FDemoChatProtocol::Parse(TEXT(R"({"update":"aircraft","set":{"side":"blue"}})"),Reply,Error,M->GetScenarioJson(),TEXT("把当前场景中的飞机全部改为蓝方")));
        FDemoScenarioConfig Parsed;
        if(FScenarioConfigLoader::Parse(Reply.Scenario,Parsed,Error))
        {
            int32 Members=0; for(const auto& G:Parsed.Units) for(const auto& I:G.Instances) if(I.Formation==TEXT("编队9")) ++Members;
            TestEqual(TEXT("Chat retains membership"),Members,3);
        }
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests/Formation-")+FGuid::NewGuid().ToString();
        FScenarioPresetStore Store(Dir); FString Id,Restored;
        TestTrue(TEXT("Formation preset saves"),Store.SaveNew(TEXT("自由编队"),M->SnapshotJson(),Id,Error));
        TestTrue(TEXT("Formation preset reads"),Store.Read(Id,Restored,Error));
        TestTrue(TEXT("Formation preset restores"),M->LoadJson(Restored,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")));
        TestTrue(TEXT("Restored custom formation usable"),M->ArrangeFormation(TEXT("编队9"),1,35,0,FVector2D(550,-515)));
        IFileManager::Get().DeleteDirectory(*Dir,false,true);
    }
    else AddError(TEXT("Airport fixture must load six planes"));
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLayoutSchemaTest,"TacticalDemo.Layout.InstanceSchema",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLayoutSchemaTest::RunTest(const FString&)
{
    FDemoScenarioConfig Config; FString Error;
    TestTrue(TEXT("Version 2 accepted"),FScenarioConfigLoader::Parse(LayoutTest::Scene,Config,Error));
    TestTrue(TEXT("Valid independent units do not depend on hypothetical group expansion"),FScenarioConfigLoader::Parse(LayoutTest::Scene.Replace(TEXT("\"y\":2"),TEXT("\"y\":999999")),Config,Error));
    const TArray<FString> Bad={
        LayoutTest::Scene.Replace(TEXT("\"schemaVersion\":2"),TEXT("\"schemaVersion\":99")),
        LayoutTest::Scene.Replace(TEXT("\"yaw\":90"),TEXT("\"yaw\":\"90\"")),
        LayoutTest::Scene.Replace(TEXT("\"R1_002\""),TEXT("\"R1_001\"")),
        LayoutTest::Scene.Replace(TEXT("\"count\":2"),TEXT("\"count\":3")),
        LayoutTest::Scene.Replace(TEXT("\"x\":20"),TEXT("\"x\":1e200"))};
    for(const FString& Json:Bad)
    {
        Config.Name=TEXT("Keep");
        TestFalse(TEXT("Invalid instance data rejected"),FScenarioConfigLoader::Parse(Json,Config,Error));
        TestEqual(TEXT("Failure preserves config"),Config.Name,FString(TEXT("Keep")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLayoutRestoreTest,"TacticalDemo.Layout.InstanceRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLayoutRestoreTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>();
    const FString Path=FPaths::ProjectSavedDir()/TEXT("Tests/Layout-")+FGuid::NewGuid().ToString()+TEXT(".json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    FFileHelper::SaveStringToFile(LayoutTest::Scene,*Path);
    if(TestTrue(TEXT("Saved layout loads"),M->LoadChatCandidate(Path,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"))) && M->GetUnits().Num()==2)
    {
        TestEqual(TEXT("First independent position"),M->GetUnits()[0]->GetActorLocation(),FVector(2000,3000,400));
        TestEqual(TEXT("First heading"),M->GetUnits()[0]->GetActorRotation().Yaw,90.0);
        TestEqual(TEXT("Second position unchanged"),M->GetUnits()[1]->GetActorLocation(),FVector(100,1200,300));
        TestEqual(TEXT("Second heading"),M->GetUnits()[1]->GetActorRotation().Yaw,-45.0);
    }
    World->DestroyWorld(false); IFileManager::Get().Delete(*Path); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLayoutEditTest,"TacticalDemo.Layout.EditUndoSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLayoutEditTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>();
    const FString Library=FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json");
    if(TestTrue(TEXT("Load in-memory layout"),M->LoadJson(LayoutTest::Scene,Library)))
    {
        auto* First=M->FindUnit(TEXT("R1_001")); auto* Other=M->FindUnit(TEXT("R1_002"));
        const FVector Original=First->GetActorLocation(),Untouched=Other->GetActorLocation();
        const uint64 Revision=M->GetRevision();
        TestTrue(TEXT("Edit one aircraft"),M->EditUnit(First->UnitId,FVector(50,60,4),135));
        TestTrue(TEXT("Revision increases"),M->GetRevision()>Revision);
        TestEqual(TEXT("Only selected aircraft moves"),Other->GetActorLocation(),Untouched);
        TestEqual(TEXT("Heading changes"),First->GetActorRotation().Yaw,135.0);
        TestTrue(TEXT("Undo supported"),M->UndoEdit()); TestEqual(TEXT("Undo position"),First->GetActorLocation(),Original);
        TestTrue(TEXT("Redo supported"),M->RedoEdit()); TestEqual(TEXT("Redo position"),First->GetActorLocation(),FVector(5000,6000,400));
        const FString Snapshot=M->SnapshotJson();
        TestFalse(TEXT("Invalid transform rejected"),M->EditUnit(First->UnitId,FVector(1e200,0,0),0));
        TestEqual(TEXT("Invalid transform preserves snapshot"),M->SnapshotJson(),Snapshot);
        FDemoChatReply Reply; FString Error;
        TestTrue(TEXT("Chat can update side after manual edit"),FDemoChatProtocol::Parse(TEXT(R"({"update":"aircraft","set":{"side":"blue"}})"),Reply,Error,Snapshot,TEXT("把当前场景中的飞机全部改为蓝方")));
        FDemoScenarioConfig Parsed;
        if(FScenarioConfigLoader::Parse(Reply.Scenario,Parsed,Error) && Parsed.Units.Num())
        { TestEqual(TEXT("Side update keeps independent position"),Parsed.Units[0].Instances[0].PositionMeters,FVector(50,60,4)); }
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests/LayoutStore-")+FGuid::NewGuid().ToString();
        FScenarioPresetStore Store(Dir); FString Id;
        TestTrue(TEXT("Save named sample"),Store.SaveNew(TEXT("机场方案 A"),Snapshot,Id,Error));
        TestEqual(TEXT("Saved sample listed"),Store.List().Num(),1);
        FString Loaded;
        TestTrue(TEXT("Read sample"),Store.Read(Id,Loaded,Error));
        TestTrue(TEXT("Reload snapshot"),M->LoadJson(Loaded,Library));
        TestEqual(TEXT("Restored independent position"),M->FindUnit(TEXT("R1_001"))->GetActorLocation(),FVector(5000,6000,400));
        TestEqual(TEXT("Restored independent yaw"),M->FindUnit(TEXT("R1_001"))->GetActorRotation().Yaw,135.0);
        TestFalse(TEXT("History reset on load"),M->CanUndo());
        TestTrue(TEXT("Chat continues after loading a saved sample"),FDemoChatProtocol::Parse(TEXT(R"({"update":"aircraft","set":{"side":"blue"}})"),Reply,Error,M->GetScenarioJson(),TEXT("把当前场景中的飞机全部改为蓝方")));
        const uint64 UnchangedRevision=M->GetRevision();
        TestTrue(TEXT("Click without movement accepted"),M->EditUnit(TEXT("R1_001"),FVector(50,60,4),135));
        TestEqual(TEXT("Selection without movement does not invalidate chat"),M->GetRevision(),UnchangedRevision);
        FString DuplicateId;
        TestFalse(TEXT("Same name cannot silently overwrite"),Store.SaveNew(TEXT("机场方案 A"),Snapshot,DuplicateId,Error));
        TestFalse(TEXT("Path traversal id rejected"),Store.Read(TEXT("../outside"),Loaded,Error));
        const FString Export=Dir/TEXT("Export.json");
        TestTrue(TEXT("Export file"),Store.Export(Id,Export,Error));
        TestFalse(TEXT("Export cannot overwrite file"),Store.Export(Id,Export,Error));
        TestTrue(TEXT("Import into library"),Store.Import(Export,TEXT("导入方案"),DuplicateId,Error));
        TestEqual(TEXT("Imported sample listed"),Store.List().Num(),2);
        TSharedPtr<FJsonObject> BadRoot; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Snapshot),BadRoot);
        BadRoot->SetStringField(TEXT("mapAsset"),TEXT("/Game/Missing/NoMap"));
        FString BadMap; FJsonSerializer::Serialize(BadRoot.ToSharedRef(),TJsonWriterFactory<>::Create(&BadMap));
        FString Map;
        TestFalse(TEXT("Missing map rejected before travel"),Store.ValidateResources(BadMap,Map,Error));
        TestFalse(TEXT("Existing mesh is not a map"),Store.ValidateResources(BadMap.Replace(TEXT("/Game/Missing/NoMap"),TEXT("/Game/plane/J20/mesh/J20S")),Map,Error));
        IFileManager::Get().DeleteDirectory(*Dir,false,true);
    }
    World->DestroyWorld(false); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLayoutDragTest,"TacticalDemo.Layout.DragCommitCancel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLayoutDragTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>(); auto* P=World->SpawnActor<ADemoPlayerController>();
    if(M->LoadJson(LayoutTest::Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")))
    {
        const FString Id=TEXT("R1_001"); auto* Unit=M->FindUnit(Id); const FVector Before=Unit->GetActorLocation();
        P->SelectUnit(Id); TestEqual(TEXT("Single selection"),P->SelectedUnit(),Unit);
        const FVector Origin=Before+FVector(0,0,1000),Direction(0,0,-1);
        if(TestTrue(TEXT("Begin drag"),P->BeginUnitDrag(Id,Origin,Direction)))
        {
            const FString Snapshot=M->SnapshotJson();
            P->UpdateUnitDrag(Origin+FVector(1000,2000,0),Direction);
            TestEqual(TEXT("Preview keeps height"),Unit->GetActorLocation(),Before+FVector(1000,2000,0));
            TestEqual(TEXT("Unconfirmed preview is not saved"),M->SnapshotJson(),Snapshot);
            P->CancelUnitDrag(); TestEqual(TEXT("Cancel restores transform"),Unit->GetActorLocation(),Before);
            TestFalse(TEXT("Cancelled drag does not create undo"),M->CanUndo());
            P->BeginUnitDrag(Id,Origin,Direction); P->UpdateUnitDrag(Origin+FVector(1000,2000,0),Direction); P->EndUnitDrag();
            TestEqual(TEXT("Release commits position"),Unit->GetActorLocation(),Before+FVector(1000,2000,0));
            TestTrue(TEXT("Whole gesture has undo"),M->UndoEdit()); TestEqual(TEXT("Undo restores start"),Unit->GetActorLocation(),Before);
        }
        FVector Hit;
        TestFalse(TEXT("Parallel ray rejected"),P->IntersectDragPlane(Origin,FVector(1,0,0),Before.Z,Hit));
        TestFalse(TEXT("Ray pointing away rejected"),P->IntersectDragPlane(Origin,FVector(0,0,1),Before.Z,Hit));
    }
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLayoutWindowTest,"TacticalDemo.Layout.WindowWorkflow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLayoutWindowTest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto* M=World->SpawnActor<ADemoScenarioManager>(); M->LoadJson(LayoutTest::Scene,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"));
    auto* W=NewObject<UDemoLayoutWidget>(World); W->Initialize(); if(!W->UnitPicker) W->NativeOnInitialized();
    TArray<UWidget*> Widgets; W->WidgetTree->GetAllWidgets(Widgets);
    UWidgetSwitcher* Pages=nullptr; for(UWidget* Widget:Widgets) if(auto* S=Cast<UWidgetSwitcher>(Widget)) Pages=S;
    if(TestNotNull(TEXT("Editor uses separate pages"),Pages)) TestEqual(TEXT("Unit, formation, preset and route pages"),Pages->GetChildrenCount(),4);
    if(Pages)
    {
        W->ShowPresets(); TestEqual(TEXT("Preset tab opens"),Pages->GetActiveWidgetIndex(),2);
        W->ShowFormations(); TestEqual(TEXT("Formation tab opens"),Pages->GetActiveWidgetIndex(),1);
        W->ShowUnits(); TestEqual(TEXT("Unit tab opens"),Pages->GetActiveWidgetIndex(),0);
    }
    if(TestNotNull(TEXT("Unit editor exists"),W->UnitPicker.Get()) && TestEqual(TEXT("Four transform fields"),W->TransformFields.Num(),4))
    {
        W->RefreshSelection(); W->UnitPicker->SetSelectedOption(TEXT("R1_001"));
        const FVector Other=M->FindUnit(TEXT("R1_002"))->GetActorLocation();
        const TCHAR* Values[]={TEXT("80"),TEXT("90"),TEXT("4"),TEXT("120")};
        for(int32 I=0;I<4;++I) W->TransformFields[I]->SetText(FText::FromString(Values[I]));
        W->ApplyTransform();
        TestEqual(TEXT("UI applies meters"),M->FindUnit(TEXT("R1_001"))->GetActorLocation(),FVector(8000,9000,400));
        TestEqual(TEXT("UI applies heading"),M->FindUnit(TEXT("R1_001"))->GetActorRotation().Yaw,120.0);
        TestEqual(TEXT("UI edit preserves other aircraft"),M->FindUnit(TEXT("R1_002"))->GetActorLocation(),Other);
        W->Undo(); TestEqual(TEXT("UI undo"),M->FindUnit(TEXT("R1_001"))->GetActorLocation(),FVector(2000,3000,400));
        W->Redo();
        W->TransformFields[0]->SetText(FText::FromString(TEXT("abc"))); W->ApplyTransform();
        TestEqual(TEXT("Invalid input keeps transform"),M->FindUnit(TEXT("R1_001"))->GetActorLocation(),FVector(8000,9000,400));
        auto* Chat=NewObject<UDemoControlWidget>(World); Chat->Initialize(); if(!Chat->Input) Chat->NativeOnInitialized();
        const FString Base=M->GetScenarioJson();
        Chat->Candidate=TEXT(R"({"scenarioName":"Stale","units":[]})"); Chat->CandidateBaseScenario=Base;
        Chat->CandidateRevision=M->GetRevision(); Chat->ObservedRevision=M->GetRevision(); Chat->ApplyButton->SetIsEnabled(true);
        M->EditUnit(TEXT("R1_001"),FVector(85,95,4),120); M->UndoEdit();
        TestEqual(TEXT("Undo can return to identical scene JSON"),M->GetScenarioJson(),Base);
        Chat->Apply();
        TestEqual(TEXT("Old candidate rejected even after undo returns same JSON"),M->GetUnits().Num(),2);
        TestTrue(TEXT("Stale candidate consumed"),Chat->Candidate.IsEmpty());
        Chat->CandidateLevel=TEXT("airport"); Chat->ObservedRevision=M->GetRevision();
        M->EditUnit(TEXT("R1_001"),FVector(85,95,4),120);
        Chat->NativeTick(FGeometry(),0);
        TestTrue(TEXT("Manual edit cancels stale map candidate"),Chat->CandidateLevel.IsEmpty());
        Chat->NativeDestruct();
    }
    World->DestroyWorld(false); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLayoutChatMergeTest,"TacticalDemo.Layout.ChatPreservesEdits",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLayoutChatMergeTest::RunTest(const FString&)
{
    FString Error; FDemoScenarioConfig Config; FScenarioConfigLoader::Parse(LayoutTest::Scene,Config,Error);
    Config.Units[0].PositionMeters=Config.Units[0].Instances[0].PositionMeters;
    const FString Current=FScenarioConfigLoader::Serialize(Config); FDemoChatReply Reply;
    auto Parse=[&](const FString& Body,const FString& User)
    {
        if(!TestTrue(TEXT("Chat update accepted"),FDemoChatProtocol::Parse(Body,Reply,Error,Current,User))) return false;
        return FScenarioConfigLoader::Parse(Reply.Scenario,Config,Error) && Config.Units.Num()==1;
    };
    if(Parse(TEXT(R"({"update":"aircraft","set":{"x":100,"y":200}})"),TEXT("把红方飞机整体移动到X=100米、Y=200米")))
    {
        TestEqual(TEXT("Group anchor moves to requested location"),Config.Units[0].Instances[0].PositionMeters,FVector(100,200,4));
        TestEqual(TEXT("Relative manual offset retained"),Config.Units[0].Instances[1].PositionMeters,FVector(81,182,3));
        TestEqual(TEXT("Movement retains yaw"),Config.Units[0].Instances[0].Yaw,90.0);
    }
    if(Parse(TEXT(R"({"update":"aircraft","set":{"count":3}})"),TEXT("把红方飞机数量改为3架")))
    {
        TestEqual(TEXT("Existing first position retained"),Config.Units[0].Instances[0].PositionMeters,FVector(20,30,4));
        TestEqual(TEXT("Existing second heading retained"),Config.Units[0].Instances[1].Yaw,-45.0);
        TestEqual(TEXT("New third position generated"),Config.Units[0].Instances[2].PositionMeters,FVector(20,50,4));
    }
    if(Parse(TEXT(R"({"update":"aircraft","set":{"spacing":8}})"),TEXT("把红方飞机间距改为8米")))
    {
        TestEqual(TEXT("Spacing applies to Y relative to anchor"),Config.Units[0].Instances[1].PositionMeters,FVector(1,38,3));
        TestEqual(TEXT("Spacing retains heading"),Config.Units[0].Instances[1].Yaw,-45.0);
    }
    return true;
}
class FLayoutQwenRun : public IAutomationLatentCommand
{
public:
    explicit FLayoutQwenRun(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(!World)
        {
            const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            Manager=World->SpawnActor<ADemoScenarioManager>(); FString Json,Error;
            if(!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectConfigDir()/TEXT("AirportScenario.json"))) || !Manager->LoadJson(Json,Library())) { Test->AddError(TEXT("Airport scene missing")); return Finish(); }
            auto* Unit=Manager->GetUnits()[0].Get(); Manager->EditUnit(Unit->UnitId,Unit->GetActorLocation()/100.+FVector(7,9,0),60);
            if(!Manager->SetFormationMembers(TEXT("编队1"),{Manager->GetUnits()[0]->UnitId,Manager->GetUnits()[3]->UnitId,Manager->GetUnits()[4]->UnitId})
                || !Manager->SetFormationMembers(TEXT("编队2"),{Manager->GetUnits()[1]->UnitId,Manager->GetUnits()[2]->UnitId,Manager->GetUnits()[5]->UnitId}))
            { Test->AddError(TEXT("Formation setup failed")); return Finish(); }
            Directory=FPaths::ProjectSavedDir()/TEXT("Tests/LayoutQwen-")+FGuid::NewGuid().ToString(); FScenarioPresetStore Store(Directory); FString Id;
            if(!Store.SaveNew(TEXT("人工编辑后保存"),Manager->SnapshotJson(),Id,Error) || !Store.Read(Id,Json,Error) || !Manager->LoadJson(Json,Library())) { Test->AddError(Error); return Finish(); }
            for(ADemoUnitActor* U:Manager->GetUnits()) { Positions.Add(U->UnitId,U->GetActorLocation()); Yaws.Add(U->UnitId,U->GetActorRotation().Yaw); }
            for(const auto& G:Manager->GetScenarioConfig().Units) for(const auto& I:G.Instances) Membership.Add(I.Id,I.Formation);
            Window.Reset(NewObject<UDemoControlWidget>(World)); Window->Initialize(); if(!Window->Input) Window->NativeOnInitialized();
            Send(); return false;
        }
        if(FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Actual local Qwen timeout")); return Finish(); }
        if(Window->ActiveRequest.IsValid()) return false;
        if(!Test->TestFalse(TEXT("Actual Qwen returns an applicable candidate after manual edit and reload"),Window->Candidate.IsEmpty())) return Finish();
        Window->ApplyButton->OnClicked.Broadcast();
        Test->TestEqual(TEXT("All six aircraft retained"),Manager->GetUnits().Num(),6);
        for(ADemoUnitActor* U:Manager->GetUnits())
        {
            Test->TestTrue(TEXT("Qwen retains independent position"),U->GetActorLocation().Equals(Positions[U->UnitId],.01));
            Test->TestTrue(TEXT("Qwen retains independent yaw"),FMath::Abs(FRotator::NormalizeAxis(U->GetActorRotation().Yaw-Yaws[U->UnitId]))<.001);
            if(Index==2) Test->TestEqual(TEXT("All aircraft change to blue"),U->Side,FString(TEXT("blue")));
        }
        const auto& Groups=Manager->GetScenarioConfig().Units;
        for(const auto& G:Groups) for(const auto& I:G.Instances) Test->TestEqual(TEXT("Actual Qwen retains free formation membership"),I.Formation,Membership.FindRef(I.Id));
        if(Groups.Num()==2)
        {
            if(Index==0) Test->TestEqual(TEXT("Texture request applied"),Groups[0].TextureId,FString(TEXT("2")));
            if(Index==1) Test->TestEqual(TEXT("Model replacement applied"),Groups[0].ModelId,FString(TEXT("J10")));
        }
        if(Test->HasAnyErrors() || ++Index==3) return Finish(); Send(); return false;
    }
private:
    FString Library() const { return FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"); }
    void Send()
    {
        const TCHAR* Commands[]={TEXT("把红方飞机的贴图换成第二款"),TEXT("把红方飞机换成J10"),TEXT("把当前场景中的飞机全部改为蓝方")};
        Window->Input->SetText(FText::FromString(Commands[Index])); Window->SendButton->OnClicked.Broadcast(); Deadline=FPlatformTime::Seconds()+300;
    }
    bool Finish()
    {
        if(Window.IsValid()) Window->NativeDestruct(); Window.Reset();
        if(World) World->DestroyWorld(false);
        if(!Directory.IsEmpty()) IFileManager::Get().DeleteDirectory(*Directory,false,true);
        return true;
    }
    FAutomationTestBase* Test; UWorld* World=nullptr; ADemoScenarioManager* Manager=nullptr;
    TStrongObjectPtr<UDemoControlWidget> Window; TMap<FString,FVector> Positions; TMap<FString,double> Yaws;
    TMap<FString,FString> Membership;
    FString Directory; int32 Index=0; double Deadline=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLayoutQwenTest,"Integration.LayoutQwen",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLayoutQwenTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FLayoutQwenRun(this)); return true; }
#endif
