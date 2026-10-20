#pragma once
#include "ScenarioConfig.h"

class FScenarioConfigLoader
{
public:
    static bool Parse(const FString& Text, FDemoScenarioConfig& OutConfig, FString& OutError);
    static bool Load(const FString& Path, FDemoScenarioConfig& OutConfig, FString& OutError);
    static bool IsSupportedType(const FString& Type);
    static FString Serialize(const FDemoScenarioConfig& Config);
};
