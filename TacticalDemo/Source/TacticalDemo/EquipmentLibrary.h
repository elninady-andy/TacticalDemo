#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"

struct FEquipmentDefinition
{
    FString Type;
    FString Asset;
    double Scale = 2.5;
    bool PreserveMaterials = false;
    FString TextureMaterial;
    int32 TextureSlot = 0;
    TMap<FString,FString> Textures;
};

class FEquipmentLibrary
{
public:
    static bool IsModelRegistered(const FString& Path, const FString& ModelId, const FString& Type, FString& OutError);
    static bool ValidateModelChanges(const FString& CurrentScenario, const FString& CandidateScenario, const FString& LibraryPath, FString& OutError);
    static FString ModelCatalog(const FString& Path);
    static FString TextureCatalog(const FString& Path);
    bool Load(const FString& Path, FString& OutError);
    UStaticMesh* Resolve(const FString& ModelId, const FString& Type, const FString& Side, FString& OutError, bool bRequireExactModel = false,FString* ResolvedModelId=nullptr);
    const FEquipmentDefinition* Presentation(const FString& ModelId, const FString& Type, const FString& Side) const;
    bool ResolveTexture(const FString& ModelId,const FString& Type,const FString& Side,const FString& TextureId,UTexture*& Texture,UMaterialInterface*& Material,int32& Slot,FString& Error);
    FLinearColor ColorForSide(const FString& Side) const;
    FString EffectiveModelId(const FString& ModelId,const FString& Type,const FString& Side) const;
    bool TextureModelSelector(const FString& User,FString& ModelId,FString& Error) const;
private:
    UStaticMesh* LoadMesh(const FString& Id);
    TMap<FString, FEquipmentDefinition> Models;
    TMap<FString, FString> Defaults;
    TMap<FString, FString> SideDefaults;
    TMap<FString, FLinearColor> SideColors;
    TMap<FString, TStrongObjectPtr<UStaticMesh>> Meshes;
    TMap<FString, TStrongObjectPtr<UTexture>> LoadedTextures;
    TMap<FString, TStrongObjectPtr<UMaterialInterface>> TextureMaterials;
};
