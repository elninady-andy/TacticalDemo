#include "Misc/AutomationTest.h"
#include "DemoControlWidget.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

// Every supported operation below is sent to the actual local Qwen service.
// Each case starts from the same mixed-side scene and checks the applied result.
class FCommandCatalogRun : public IAutomationLatentCommand
{
public:
    explicit FCommandCatalogRun(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(!World)
        {
            const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
            Manager=World->SpawnActor<ADemoScenarioManager>();
            Window.Reset(NewObject<UDemoControlWidget>(World)); Window->Initialize();
            if(!Window->Input) Window->NativeOnInitialized();
            Window->Endpoint->SetText(FText::FromString(TEXT("http://127.0.0.1:8000/v1/chat/completions")));
            Window->Model->SetText(FText::FromString(TEXT("qwen")));
            SendCase(); return false;
        }
        if(Window->ActiveRequest.IsValid() && FPlatformTime::Seconds()<Deadline) return false;
        Test->AddInfo(FString::Printf(TEXT("Command category %d: %s"),Index+1,Commands()[Index]));
        Test->TestFalse(TEXT("Request completes"),Window->ActiveRequest.IsValid());
        Test->TestFalse(TEXT("Validated candidate exists"),Window->Candidate.IsEmpty());
        const bool bCaseReady=!Window->ActiveRequest.IsValid() && !Window->Candidate.IsEmpty();
        if(bCaseReady)
        {
            Window->ApplyButton->OnClicked.Broadcast();
            const auto& Groups=Manager->GetScenarioConfig().Units;
            if(Index<4)
            {
                const TCHAR* Types[]={TEXT("aircraft"),TEXT("vehicle"),TEXT("radar"),TEXT("target")};
                Test->TestEqual(TEXT("Creation yields one group"),Groups.Num(),1);
                if(Groups.Num()==1)
                { Test->TestEqual(TEXT("Created requested type"),Groups[0].Type,FString(Types[Index])); Test->TestEqual(TEXT("Created requested count"),Groups[0].Count,2); Test->TestEqual(TEXT("Created requested side"),Groups[0].Side,FString(TEXT("red"))); Test->TestEqual(TEXT("Unspecified position starts at origin"),Groups[0].PositionMeters,FVector::ZeroVector); Test->TestEqual(TEXT("Unspecified spacing defaults to 10m"),Groups[0].SpacingMeters,10.0); Test->TestEqual(TEXT("Unrequested route is absent"),Groups[0].RouteMeters.Num(),0); }
            }
            else
            {
                Test->TestEqual(TEXT("Updates preserve five groups"),Groups.Num(),5);
                if(Groups.Num()==5)
                {
                    const auto& Red=Groups[0]; const auto& Blue=Groups[1];
                    Test->TestEqual(TEXT("Other unit types are preserved"),Groups[2].Count,2);
                    Test->TestEqual(TEXT("Other aircraft group count is preserved"),Blue.Count,3);
                    Test->TestEqual(TEXT("Requested count or preserved count"),Red.Count,Index==4?5:3);
                    Test->TestEqual(TEXT("Requested height or preserved height"),Red.PositionMeters.Z,Index==7?30.0:5.0);
                    Test->TestEqual(TEXT("Requested spacing or preserved spacing"),Red.SpacingMeters,Index==8?8.0:6.0);
                    if(Index==5) { Test->TestEqual(TEXT("All red aircraft change to blue"),Red.Side,FString(TEXT("blue"))); Test->TestEqual(TEXT("Existing blue aircraft remain blue"),Blue.Side,FString(TEXT("blue"))); }
                    if(Index==6) Test->TestEqual(TEXT("Position uses meters and preserves height"),Red.PositionMeters,FVector(20,10,5));
                    if(Index==9) Test->TestEqual(TEXT("Requested registered model is applied"),Red.ModelId,FString(TEXT("aircraft_blue_basic")));
                    if(Index==10) { Test->TestEqual(TEXT("Route has three ordered waypoints"),Red.RouteMeters.Num(),3); if(Red.RouteMeters.Num()==3) Test->TestEqual(TEXT("Route middle waypoint"),Red.RouteMeters[1],FVector(10,10,25)); }
                    if(Index==11) Test->TestEqual(TEXT("Route is cleared"),Red.RouteMeters.Num(),0);
                    if(Index==10 || Index==11) Test->TestEqual(TEXT("Route operation keeps unit location"),Manager->GetUnits()[0]->GetActorLocation(),FVector(0,0,500));
                }
            }
        }
        if(++Index<12) { SendCase(); return false; }
        Window->NativeDestruct(); Window.Reset(); World->DestroyWorld(false); return true;
    }
private:
    const TCHAR* const* Commands() const
    {
        static const TCHAR* Values[]={TEXT("创建一个新场景，只放置2架红方飞机。"),TEXT("创建一个新场景，只放置2辆红方车辆。"),TEXT("创建一个新场景，只放置2台红方雷达。"),TEXT("创建一个新场景，只放置2个红方目标。"),TEXT("只把红方飞机数量改为5架。"),TEXT("把当前场景中的飞机全部改为蓝方。"),TEXT("把红方飞机整体移动到X=20米、Y=10米，保持高度和间距不变。"),TEXT("把红方飞机高度改为30米。"),TEXT("把红方飞机间距改为8米。"),TEXT("把红方飞机模型替换为 aircraft_blue_basic。"),TEXT("给红方飞机设置静态航迹，按顺序经过(0,0,20)、(10,10,25)、(20,0,20)，不移动飞机。"),TEXT("清除红方飞机的航迹，保持飞机位置不变。")};
        return Values;
    }
    void SendCase()
    {
        Manager->ReloadScenario(); Window->Messages.Reset();
        Window->Input->SetText(FText::FromString(Commands()[Index]));
        Window->SendButton->OnClicked.Broadcast(); Deadline=FPlatformTime::Seconds()+300;
    }
    FAutomationTestBase* Test;
    UWorld* World=nullptr;
    ADemoScenarioManager* Manager=nullptr;
    TStrongObjectPtr<UDemoControlWidget> Window;
    int32 Index=0;
    double Deadline=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCommandCatalogTest,"Integration.CommandCatalog",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCommandCatalogTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FCommandCatalogRun(this)); return true;
}
