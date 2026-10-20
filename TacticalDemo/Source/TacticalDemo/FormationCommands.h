#pragma once
#include "CoreMinimal.h"
#include "ScenarioConfig.h"

struct FFormationCommandResult
{
    FString Reply;
    FString SelectName;
    FString Scenario;
    bool bMembershipOnly=false;
};
class FFormationCommands
{
public:
    static bool Handle(const FString& User,const FString& Current,FFormationCommandResult& Out);
    static bool IsNameMention(const FString& Text,int32 Offset,const FString& Name);
    static bool Arrange(FDemoScenarioConfig& Config,const FString& Name,int32 Pattern,double Spacing,double Heading,const FVector2D& Anchor,FString& Error);
private:
    static bool HandleMembership(const FString& User,FDemoScenarioConfig Config,FFormationCommandResult& Out);
};
