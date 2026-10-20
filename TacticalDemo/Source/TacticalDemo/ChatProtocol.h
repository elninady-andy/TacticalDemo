#pragma once
#include "CoreMinimal.h"

struct FDemoChatReply
{
    FString Reply;
    FString Scenario;
    FString LevelId;
};

class FDemoChatProtocol
{
public:
    static bool Parse(const FString& Content, FDemoChatReply& Out, FString& Error, const FString& CurrentScenario = FString(), const FString& UserInstruction = FString());
    static bool IsSceneRequest(const FString& User);
    static FString SystemPrompt(const FString& User = FString(), const FString& ModelCatalog = FString(),const FString& TextureCatalog = FString());
};
