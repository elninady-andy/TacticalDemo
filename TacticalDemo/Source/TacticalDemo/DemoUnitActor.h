#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoUnitActor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

UCLASS()
class TACTICALDEMO_API ADemoUnitActor : public AActor
{
    GENERATED_BODY()
public:
    ADemoUnitActor();
    void Configure(const FString& InId, const FString& InSide, UStaticMesh* Mesh, UMaterialInterface* Material, const FLinearColor& InColor, double Scale=2.5, bool PreserveMaterials=false,class UTexture* Texture=nullptr,UMaterialInterface* TextureMaterial=nullptr,int32 TextureSlot=0);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString UnitId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Side;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString ResolvedModelId;
};
