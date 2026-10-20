#pragma once
#include "CoreMinimal.h"

struct FScenarioPresetEntry { FString Id,Name,MapAsset,CreatedUtc; };
class FScenarioPresetStore
{
public:
    explicit FScenarioPresetStore(const FString& Directory=FString());
    TArray<FScenarioPresetEntry> List() const;
    bool SaveNew(const FString& Name,const FString& Json,FString& Id,FString& Error);
    bool Read(const FString& Id,FString& Json,FString& Error) const;
    bool Import(const FString& Path,const FString& Name,FString& Id,FString& Error);
    bool Export(const FString& Id,const FString& Path,FString& Error) const;
    static bool ValidateResources(const FString& Json,FString& MapAsset,FString& Error);
private:
    FString Directory;
    FString PathFor(const FString& Id) const;
};
