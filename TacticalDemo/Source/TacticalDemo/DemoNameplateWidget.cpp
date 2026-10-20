#include "DemoNameplateWidget.h"
#include "DemoScenarioManager.h"
#include "DemoUnitActor.h"
#include "FlightPreview.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CanvasPanel.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"

TArray<FDemoNameplateEntry> UDemoNameplateWidget::Gather(UWorld* World)
{
    TArray<FDemoNameplateEntry> Entries; if(!World) return Entries;
    ADemoFlightPreview* Preview=nullptr;
    for(TActorIterator<ADemoFlightPreview> It(World);It;++It) if(It->IsPlaying()) { Preview=*It; break; }
    for(TActorIterator<ADemoScenarioManager> It(World);It;++It)
    {
        TSet<FString> AircraftIds;
        for(const auto& G:It->GetScenarioConfig().Units) if(G.Type==TEXT("aircraft"))
            for(int32 I=0;I<G.Count;++I)
            {
                const FString Id=G.Instances.Num()==G.Count?G.Instances[I].Id:FString::Printf(TEXT("%s_%03d"),*G.Id,I+1);
                AircraftIds.Add(Id);
            }
        // Units retain scenario order; one pass avoids a linear FindUnit per label.
        for(ADemoUnitActor* Unit:It->GetUnits())
            {
                if(!IsValid(Unit) || !AircraftIds.Contains(Unit->UnitId) || Unit->IsHidden() || !Unit->Visual || !Unit->Visual->GetStaticMesh()) continue;
                const FString& Id=Unit->UnitId;
                FTransform Transform=Unit->Visual->GetComponentTransform();
                const bool Moving=Preview && Preview->GetPreviewTransform(Id,Transform);
                if(!Moving && (!Unit->Visual->IsVisible() || Unit->Visual->bHiddenInGame)) continue;
                const FBox Bounds=Unit->Visual->GetStaticMesh()->GetBoundingBox().TransformBy(Transform);
                FString Name=Unit->ResolvedModelId;
                if(Name==TEXT("J20") || Name==TEXT("J20_Map3")) Name=TEXT("J-20");
                else if(Name==TEXT("J10")) Name=TEXT("J-10");
                else if(Name==TEXT("F35")) Name=TEXT("F-35");
                else if(Name.IsEmpty() || Name==TEXT("aircraft_default") || Name==TEXT("aircraft_red_basic") || Name==TEXT("aircraft_blue_basic")) Name=TEXT("飞机");
                if(Name.Len()>24) Name=Name.Left(23)+TEXT("…");
                Entries.Add({Id,Name,FVector(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Max.Z+150.)});
            }
    }
    return Entries;
}
FVector2D UDemoNameplateWidget::PlaceLabel(const FVector2D& Anchor,const FVector2D& Size,const FVector2D& View,const TArray<FSlateRect>& Occupied)
{
    const FVector2D Limit(FMath::Max(0.,View.X-Size.X),FMath::Max(0.,View.Y-Size.Y));
    const FVector2D Preferred(FMath::Clamp(Anchor.X-Size.X/2,0.,Limit.X),FMath::Clamp(Anchor.Y-Size.Y-6,0.,Limit.Y));
    auto Available=[&](const FVector2D& P)
    {
        if(P.X<0 || P.Y<0 || P.X>Limit.X || P.Y>Limit.Y) return false;
        const FSlateRect Rect(P.X,P.Y,P.X+Size.X,P.Y+Size.Y);
        return !Occupied.ContainsByPredicate([&](const FSlateRect& Other){return FSlateRect::DoRectanglesIntersect(Rect,Other);});
    };
    if(Available(Preferred)) return Preferred;
    // Search both sides of the preferred row, then neighbouring columns.
    const int32 Rows=FMath::Min(64,FMath::CeilToInt(View.Y/(Size.Y+4))+1);
    for(int32 Column=0;Column<9;++Column)
        for(int32 Row=0;Row<Rows;++Row)
            for(int32 Sign:{-1,1})
            {
                const int32 Offset=Column==0?0:((Column+1)/2)*(Column%2?1:-1);
                const FVector2D Candidate(Preferred.X+Offset*(Size.X+4),Preferred.Y+Sign*Row*(Size.Y+4));
                if(Available(Candidate)) return Candidate;
            }
    return Preferred;
}
void UDemoNameplateWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    WidgetTree->RootWidget=WidgetTree->ConstructWidget<UCanvasPanel>();
    SetVisibility(ESlateVisibility::HitTestInvisible); ForceVolatile(true);
}
int32 UDemoNameplateWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& DrawElements,int32 LayerId,const FWidgetStyle& Style,bool ParentEnabled) const
{
    const int32 Base=Super::NativePaint(Args,Geometry,CullingRect,DrawElements,LayerId,Style,ParentEnabled);
    LastPaintedCount=0;
    auto* Player=GetOwningPlayer(); if(!Player || !FSlateApplication::IsInitialized()) return Base;
    const FVector2D View=Geometry.GetLocalSize(); if(View.X<1 || View.Y<1) return Base;
    const FSlateFontInfo Font(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),22);
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const auto* Brush=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    TArray<FSlateRect> Occupied;
    for(const auto& Entry:Gather(GetWorld()))
    {
        FVector2D Point;
        if(!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player,Entry.Anchor,Point,true)
            || Point.X<0 || Point.X>View.X || Point.Y<0 || Point.Y>View.Y) continue;
        const FVector2D TextSize=Measure->Measure(Entry.Text,Font),Size(TextSize.X+20,TextSize.Y+8);
        const FVector2D Position=PlaceLabel(Point,Size,View,Occupied);
        Occupied.Add(FSlateRect(Position.X,Position.Y,Position.X+Size.X,Position.Y+Size.Y));
        const FVector2D Bottom(Position.X+Size.X/2,Position.Y+Size.Y);
        TArray<FVector2f> Line={FVector2f(Point),FVector2f(Bottom)};
        FSlateDrawElement::MakeLines(DrawElements,Base+1,Geometry.ToPaintGeometry(),Line,ESlateDrawEffect::None,FLinearColor(.6f,.8f,1.f,.8f),true,1.f);
        FSlateDrawElement::MakeBox(DrawElements,Base+2,Geometry.ToPaintGeometry(Size,FSlateLayoutTransform(Position)),Brush,ESlateDrawEffect::None,FLinearColor(.015f,.035f,.06f,.9f));
        const FLinearColor TextColor=(Entry.Text==TEXT("J-20") || Entry.Text==TEXT("J-10"))
            ? FLinearColor(FColor(255,128,191)) : FLinearColor::White;
        FSlateDrawElement::MakeText(DrawElements,Base+3,Geometry.ToPaintGeometry(TextSize,FSlateLayoutTransform(Position+FVector2D(10,4))),Entry.Text,Font,ESlateDrawEffect::None,TextColor);
        ++LastPaintedCount;
    }
    return Base+3;
}
