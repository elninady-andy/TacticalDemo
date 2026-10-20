#include "DemoUnitActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
ADemoUnitActor::ADemoUnitActor()
{
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    SetRootComponent(Visual);
    Visual->SetMobility(EComponentMobility::Movable);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrimaryActorTick.bCanEverTick = false;
}
void ADemoUnitActor::Configure(const FString& InId, const FString& InSide, UStaticMesh* Mesh, UMaterialInterface* Material, const FLinearColor& InColor, double Scale, bool PreserveMaterials,UTexture* Texture,UMaterialInterface* TextureMaterial,int32 TextureSlot)
{
    UnitId = InId;
    Side = InSide;
    Visual->SetStaticMesh(Mesh);
    SetActorScale3D(FVector(Scale));
    if(Texture && TextureMaterial)
    {
        UMaterialInstanceDynamic* Instance=UMaterialInstanceDynamic::Create(TextureMaterial,this);
        Instance->SetTextureParameterValue(TEXT("AircraftDiffuse"),Texture);
        Visual->SetMaterial(TextureSlot,Instance);
        return;
    }
    if(PreserveMaterials) return;
    UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Material, this);
    Instance->SetVectorParameterValue(TEXT("SideColor"), InColor);
    for (int32 Index=0; Index < Visual->GetNumMaterials(); ++Index) Visual->SetMaterial(Index, Instance);
}
