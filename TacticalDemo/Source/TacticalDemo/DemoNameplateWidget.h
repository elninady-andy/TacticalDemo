#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DemoNameplateWidget.generated.h"

struct FDemoNameplateEntry
{
    FString UnitId;
    FString Text;
    FVector Anchor=FVector::ZeroVector;
};

UCLASS()
class TACTICALDEMO_API UDemoNameplateWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    static TArray<FDemoNameplateEntry> Gather(UWorld* World);
    static FVector2D PlaceLabel(const FVector2D& Anchor,const FVector2D& Size,const FVector2D& View,const TArray<FSlateRect>& Occupied);
protected:
    virtual void NativeOnInitialized() override;
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& DrawElements,int32 LayerId,const FWidgetStyle& Style,bool ParentEnabled) const override;
private:
    friend class ADemoGameMode;
    mutable int32 LastPaintedCount=0;
};
