#pragma once
#include "CoreMinimal.h"

struct FHighlightCommandResult
{
    FString Reply;
    FString Scenario;
};
class FHighlightCommands
{
public:
    static bool Handle(const FString& User,const FString& Current,const FString& SelectedId,FHighlightCommandResult& Out);
};
