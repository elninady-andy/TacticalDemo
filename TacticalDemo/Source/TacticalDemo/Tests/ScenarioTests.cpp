#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ScenarioConfigLoader.h"

namespace DemoTest
{
    const FString Unit = TEXT(R"({"id":"R1","modelId":"aircraft_default","type":"aircraft","side":"red","count":3,"position":{"x":1,"y":2,"z":10}})");
    FString Wrap(const FString& Value) { return TEXT("{\"scenarioName\":\"Test\",\"units\":[") + Value + TEXT("]}"); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioValidTest, "TacticalDemo.Config.ValidAndEmpty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FScenarioValidTest::RunTest(const FString&)
{
    FDemoScenarioConfig Config;
    FString Error;
    if (!TestTrue(TEXT("Valid JSON accepted"), FScenarioConfigLoader::Parse(DemoTest::Wrap(DemoTest::Unit), Config, Error)))
        return false;
    TestEqual(TEXT("One group"), Config.Units.Num(), 1);
    TestEqual(TEXT("Count"), Config.Units[0].Count, 3);
    TestEqual(TEXT("Position remains meters"), Config.Units[0].PositionMeters, FVector(1,2,10));
    TestEqual(TEXT("Default spacing"), Config.Units[0].SpacingMeters, 10.0);
    TestTrue(TEXT("Empty scene accepted"), FScenarioConfigLoader::Parse(DemoTest::Wrap(TEXT("")), Config, Error));
    TestEqual(TEXT("Empty scene clears configuration"), Config.Units.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioInvalidTest, "TacticalDemo.Config.RejectInvalidWithoutMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FScenarioInvalidTest::RunTest(const FString&)
{
    const TArray<FString> Invalid = {
        TEXT("not json"), TEXT("{}"),
        DemoTest::Wrap(DemoTest::Unit).Replace(TEXT("\"scenarioName\":\"Test\""), TEXT("\"scenarioName\":123")),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"id\":\"R1\""), TEXT("\"id\":123"))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"modelId\":\"aircraft_default\""), TEXT("\"modelId\":true"))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"x\":1"), TEXT("\"x\":1e200"))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"count\":3"), TEXT("\"spacing\":1e200,\"count\":3"))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"count\":3"), TEXT("\"count\":3.5"))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"count\":3"), TEXT("\"count\":-1"))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"count\":3"), TEXT("\"count\":101"))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"x\":1"), TEXT("\"x\":\"1\""))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"side\":\"red\""), TEXT("\"side\":\"green\""))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"type\":\"aircraft\""), TEXT("\"type\":\"unknown\""))),
        DemoTest::Wrap(DemoTest::Unit.Replace(TEXT("\"count\":3"), TEXT("\"spacing\":0,\"count\":3"))),
        DemoTest::Wrap(DemoTest::Unit + TEXT(",") + DemoTest::Unit)
    };
    for (const FString& Input : Invalid)
    {
        FDemoScenarioConfig Config;
        Config.Name = TEXT("KeepExisting");
        FString Error;
        TestFalse(TEXT("Invalid input rejected"), FScenarioConfigLoader::Parse(Input, Config, Error));
        TestFalse(TEXT("Has useful error"), Error.IsEmpty());
        TestEqual(TEXT("Failure leaves caller output unchanged"), Config.Name, FString(TEXT("KeepExisting")));
    }
    FString Many;
    for (int32 I=0; I<6; ++I)
    {
        if (I) Many += TEXT(",");
        Many += DemoTest::Unit.Replace(TEXT("\"R1\""), *FString::Printf(TEXT("\"R%d\""), I))
            .Replace(TEXT("\"count\":3"), TEXT("\"count\":100"));
    }
    FDemoScenarioConfig LimitConfig;
    FString LimitError;
    TestFalse(TEXT("Total count limit enforced"), FScenarioConfigLoader::Parse(DemoTest::Wrap(Many), LimitConfig, LimitError));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioRouteValidationTest, "TacticalDemo.Config.RouteValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FScenarioRouteValidationTest::RunTest(const FString&)
{
    const FString Base = TEXT(R"({"scenarioName":"Route","units":[{"id":"R1","modelId":"aircraft_default","type":"aircraft","side":"red","count":1,"position":{"x":1,"y":2,"z":3},"route":[{"x":0,"y":0,"z":3},{"x":20,"y":10,"z":3}]}]})");
    FDemoScenarioConfig Config;
    FString Error;
    TestTrue(TEXT("Two waypoint static route is accepted"), FScenarioConfigLoader::Parse(Base, Config, Error));
    if (Config.Units.Num() == 1) TestEqual(TEXT("Waypoint data is retained in meters"), Config.Units[0].RouteMeters.Num(), 2);
    const FString GoodRoute = TEXT("[{\"x\":0,\"y\":0,\"z\":3},{\"x\":20,\"y\":10,\"z\":3}]");
    const FString ClearedRoute = Base.Replace(*GoodRoute, TEXT("[]"));
    TestTrue(TEXT("Empty route clears waypoints"), FScenarioConfigLoader::Parse(ClearedRoute, Config, Error));
    if (Config.Units.Num() == 1) TestEqual(TEXT("Cleared route has no waypoints"), Config.Units[0].RouteMeters.Num(), 0);
    const TArray<FString> InvalidRoutes = {
        TEXT("[{\"x\":0,\"y\":0,\"z\":0}]"),
        TEXT("[{\"x\":0,\"y\":0,\"z\":0},{\"x\":1e200,\"y\":0,\"z\":0}]"),
        TEXT("[{\"x\":0,\"y\":0},{\"x\":1,\"y\":1,\"z\":0}]"),
        TEXT("[{\"x\":\"0\",\"y\":0,\"z\":0},{\"x\":1,\"y\":1,\"z\":0}]")
    };
    for (const FString& Route : InvalidRoutes)
    {
        const FString Input = Base.Replace(*GoodRoute, *Route);
        Config.Name = TEXT("Preserve");
        TestFalse(TEXT("Malformed route rejected"), FScenarioConfigLoader::Parse(Input, Config, Error));
        TestEqual(TEXT("Rejected route leaves output untouched"), Config.Name, FString(TEXT("Preserve")));
        TestFalse(TEXT("Route error is actionable"), Error.IsEmpty());
    }
    return true;
}
#endif
