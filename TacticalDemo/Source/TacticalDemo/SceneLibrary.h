#pragma once
#include "CoreMinimal.h"
class FDemoSceneLibrary
{
public:
    static bool IsSwitchRequest(const FString& User);
    static FString Catalog();
    static FString ScenarioOverride(const FString& MapAsset);
    static bool Resolve(const FString& Id,FString& Asset,FString& Error);
};
