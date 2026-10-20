#include "DemoRouteWidget.h"
#include "DemoGameMode.h"
#include "DemoScenarioManager.h"
#include "ScenarioConfigLoader.h"
#include "DemoUnitActor.h"
#include "FlightPreview.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Brushes/SlateColorBrush.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ADemoPlayerController* UDemoRouteWidget::Player() const
{
    auto* P=Cast<ADemoPlayerController>(GetOwningPlayer());
    return P?P:(GetWorld()?Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController()):nullptr);
}
ADemoScenarioManager* UDemoRouteWidget::Manager() const { auto* P=Player(); return P?P->ScenarioManager():nullptr; }
void UDemoRouteWidget::EnsureInitialized() { Initialize(); if(!PointRows) NativeOnInitialized(); }
UTextBlock* UDemoRouteWidget::MakeText(const FString& Value,int32 Size)
{
    auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Value)); T->SetAutoWrapText(true);
    T->SetColorAndOpacity(FLinearColor(.06f,.12f,.2f)); T->SetFont(FSlateFontInfo(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),Size)); return T;
}
UEditableTextBox* UDemoRouteWidget::MakeField(const FString& Value)
{
    auto* E=WidgetTree->ConstructWidget<UEditableTextBox>(); auto& Style=E->WidgetStyle;
    Style.SetFont(FSlateFontInfo(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),14));
    Style.SetForegroundColor(FLinearColor(.03f,.07f,.12f)).SetFocusedForegroundColor(FLinearColor(.03f,.07f,.12f));
    Style.TextStyle.SetColorAndOpacity(FLinearColor(.03f,.07f,.12f)); Style.SetBackgroundColor(FLinearColor::White);
    Style.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor::White)); Style.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor::White));
    E->SetText(FText::FromString(Value)); E->OnTextChanged.AddDynamic(this,&UDemoRouteWidget::FieldChanged); return E;
}
void UDemoRouteWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root=WidgetTree->ConstructWidget<UVerticalBox>(); WidgetTree->RootWidget=Root;
    SelectionLabel=MakeText(TEXT("请选择飞机"),16); Root->AddChildToVerticalBox(SelectionLabel)->SetPadding(FMargin(0,8));
    Root->AddChildToVerticalBox(MakeText(TEXT("世界坐标（米）。批量应用时以首选飞机为基准，其他飞机航迹按当前位置偏移。只保存航迹，飞机保持静止。")));
    auto Button=[&](UHorizontalBox* Row,const TCHAR* Label,FName Handler)
    {
        auto* B=WidgetTree->ConstructWidget<UButton>(); B->SetBackgroundColor(FLinearColor(.08f,.34f,.57f));
        auto* T=MakeText(Label); T->SetColorAndOpacity(FLinearColor::White); B->AddChild(T);
        Row->AddChildToHorizontalBox(B)->SetPadding(FMargin(0,4,8,4)); FScriptDelegate D; D.BindUFunction(this,Handler); B->OnClicked.Add(D);
    };
    auto* Options=WidgetTree->ConstructWidget<UHorizontalBox>(); Root->AddChildToVerticalBox(Options)->SetPadding(FMargin(0,8));
    Options->AddChildToHorizontalBox(MakeText(TEXT("时长（秒） "))); Duration=MakeField(TEXT("40")); Options->AddChildToHorizontalBox(Duration);
    Closed=WidgetTree->ConstructWidget<UCheckBox>(); Options->AddChildToHorizontalBox(Closed)->SetPadding(FMargin(16,0,0,0));
    Closed->SetContent(MakeText(TEXT("闭合循环"))); Closed->OnCheckStateChanged.AddDynamic(this,&UDemoRouteWidget::ClosedChanged);
    auto* Commit=WidgetTree->ConstructWidget<UHorizontalBox>(); Root->AddChildToVerticalBox(Commit);
    Button(Commit,TEXT("应用所选飞机航迹"),GET_FUNCTION_NAME_CHECKED(UDemoRouteWidget,Apply));
    Button(Commit,TEXT("恢复所选默认航迹"),GET_FUNCTION_NAME_CHECKED(UDemoRouteWidget,Reset));
    auto* Actions=WidgetTree->ConstructWidget<UHorizontalBox>(); Root->AddChildToVerticalBox(Actions);
    Button(Actions,TEXT("添加航点"),GET_FUNCTION_NAME_CHECKED(UDemoRouteWidget,AddPoint));
    Button(Actions,TEXT("删除末点"),GET_FUNCTION_NAME_CHECKED(UDemoRouteWidget,RemovePoint));
    Button(Actions,TEXT("重新读取所选航迹"),GET_FUNCTION_NAME_CHECKED(UDemoRouteWidget,Reload));
    Root->AddChildToVerticalBox(MakeText(TEXT("序号              X                      Y                      世界 Z")));
    auto* Area=WidgetTree->ConstructWidget<USizeBox>(); Area->SetHeightOverride(180); Root->AddChildToVerticalBox(Area);
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Area->SetContent(Scroll);
    PointRows=WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(PointRows);
    Root->AddChildToVerticalBox(MakeText(TEXT("播放需手动点击主窗口“播放轨迹”。首点不在当前位置时先飞向首点。开放航迹时长包含接入，到末点停止；闭合时长指每圈，首次接入另计。恢复默认后使用原编组或 map3 源航迹。"),12));
    Status=MakeText(TEXT("")); Root->AddChildToVerticalBox(Status)->SetPadding(FMargin(0,8)); LoadSelection();
}
void UDemoRouteWidget::BuildRows(const TArray<FVector>& Points)
{
    bLoading=true; PointRows->ClearChildren(); PointFields.Reset();
    for(int32 I=0;I<Points.Num();++I)
    {
        auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>(); PointRows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,3));
        Row->AddChildToHorizontalBox(MakeText(FString::Printf(TEXT("%03d  "),I+1)));
        for(int32 Axis=0;Axis<3;++Axis)
        {
            auto* Field=MakeField(FString::Printf(TEXT("%.6f"),Points[I][Axis])); PointFields.Add(Field);
            auto* PointSlot=Row->AddChildToHorizontalBox(Field); PointSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); PointSlot->SetPadding(FMargin(0,0,6,0));
        }
    }
    bLoading=false;
}
bool UDemoRouteWidget::ReadPoints(TArray<FVector>& Points) const
{
    Points.Reset();
    if(PointFields.Num()%3) return false;
    for(int32 I=0;I<PointFields.Num();I+=3)
    {
        FVector P;
        for(int32 A=0;A<3;++A)
            if(!FDefaultValueHelper::ParseDouble(PointFields[I+A]->GetText().ToString(),P[A]) || !FMath::IsFinite(P[A]) || FMath::Abs(P[A])>1000000) return false;
        Points.Add(P);
    }
    return true;
}
bool UDemoRouteWidget::IsCurrent() const
{
    auto* P=Player(); auto* M=Manager();
    return P && M && DraftRevision==M->GetRevision() && DraftSelectionRevision==P->SelectionRevision() && DraftIds==P->SelectedUnitIds();
}
FString UDemoRouteWidget::CurrentDraftText() const
{
    // UMG SetText may update the property before a Slate widget exists, without
    // broadcasting OnTextChanged. Compare the visible draft itself as well.
    FString Signature;
    auto Append=[&](const FString& Text){ Signature+=FString::Printf(TEXT("%d:"),Text.Len())+Text; };
    Append(Duration?Duration->GetText().ToString():FString());
    Signature+=Closed && Closed->IsChecked()?TEXT("1"):TEXT("0");
    Signature+=FString::Printf(TEXT("/%d/"),PointFields.Num());
    for(const auto& Field:PointFields) Append(Field?Field->GetText().ToString():FString());
    return Signature;
}
void UDemoRouteWidget::LoadSelection()
{
    auto* P=Player(); auto* M=Manager(); if(!P || !M || !PointRows) return;
    DraftIds=P->SelectedUnitIds(); DraftSelectionRevision=P->SelectionRevision(); DraftRevision=M->GetRevision();
    TArray<FVector> Points; double Period=40; bool Loop=false; FString Source=TEXT("新建直线（尚未应用）");
    if(auto* Primary=P->SelectedUnit())
    {
        const FVector Origin=Primary->GetActorLocation()/100.;
        FDemoScenarioConfig Snapshot; FString SnapshotError;
        FScenarioConfigLoader::Parse(M->SnapshotJson(),Snapshot,SnapshotError);
        for(const auto& G:Snapshot.Units) for(const auto& I:G.Instances) if(I.Id==Primary->UnitId)
        {
            if(!I.FlightRoute.PointsMeters.IsEmpty())
            { Points=I.FlightRoute.PointsMeters; Period=I.FlightRoute.PeriodSeconds; Loop=I.FlightRoute.bClosed; Source=TEXT("已保存的单机航迹"); }
            else if(!G.RouteMeters.IsEmpty()) { Points=G.RouteMeters; Source=TEXT("原编组航迹"); }
            else
            {
                FString Json,Map,Error; TArray<FDemoFlightPath> Paths;
                if(FFileHelper::LoadFileToString(Json,*(FPaths::ProjectConfigDir()/TEXT("Map3FlightPaths.json"))) && FDemoFlightPath::ParseJson(Json,Map,Paths,Error) && M->CurrentMapAsset()==Map)
                    for(const auto& Path:Paths) if(Path.ModelId==G.ModelId)
                    { for(const auto& Sample:Path.SamplesCm) Points.Add(Origin+(Sample-Path.SamplesCm[0])/100.); Period=Path.PeriodSeconds; Loop=true; Source=TEXT("map3 源航迹副本（尚未应用）"); }
            }
        }
        if(Points.IsEmpty()) Points={Origin,Origin+FVector(100,0,0)};
    }
    if(Loop && Points.Num()>2 && Points[0].Equals(Points.Last(),.0001)) Points.Pop();
    BuildRows(Points); bLoading=true; Duration->SetText(FText::FromString(FString::Printf(TEXT("%.6f"),Period))); Closed->SetIsChecked(Loop); bLoading=false; bDirty=false;
    LoadedDraftText=CurrentDraftText();
    SelectionLabel->SetText(FText::FromString(DraftIds.IsEmpty()?TEXT("请选择飞机"):FString::Printf(TEXT("%d 架飞机 · 首选 %s"),DraftIds.Num(),*DraftIds[0])));
    Status->SetText(FText::FromString(DraftIds.IsEmpty()?TEXT("在单位列表或场景中选择飞机。"):Source));
}
void UDemoRouteWidget::RefreshSelection()
{
    if(!PointRows || IsCurrent()) return;
    bDirty=bDirty || CurrentDraftText()!=LoadedDraftText;
    if(bDirty) Status->SetText(FText::FromString(TEXT("选择或场景已改变，保留了未应用草稿。请点击“重新读取所选航迹”后再编辑，旧草稿不能应用。")));
    else LoadSelection();
}
void UDemoRouteWidget::FieldChanged(const FText&) { if(!bLoading) { bDirty=true; Status->SetText(FText::FromString(TEXT("草稿尚未应用；蓝色连线仅为草稿预览。"))); } }
void UDemoRouteWidget::ClosedChanged(bool) { FieldChanged(FText()); }
void UDemoRouteWidget::AddPoint()
{
    TArray<FVector> Points; if(!ReadPoints(Points)) { Status->SetText(FText::FromString(TEXT("请先修正现有坐标。"))); return; }
    if(Points.Num()>=(Closed->IsChecked()?511:512)) { Status->SetText(FText::FromString(TEXT("开放航迹最多512点；闭合航迹预留一个自动闭合点。"))); return; }
    Points.Add(Points.IsEmpty()?FVector::ZeroVector:Points.Last()+FVector(100,0,0)); BuildRows(Points); FieldChanged(FText());
}
void UDemoRouteWidget::RemovePoint()
{
    TArray<FVector> Points; if(!ReadPoints(Points) || Points.Num()<=2) { Status->SetText(FText::FromString(TEXT("至少保留两个航点；请先修正坐标。"))); return; }
    Points.Pop(); BuildRows(Points); FieldChanged(FText());
}
void UDemoRouteWidget::Reload() { LoadSelection(); }
void UDemoRouteWidget::Apply()
{
    if(!IsCurrent()) { Status->SetText(FText::FromString(TEXT("草稿已过期，请重新读取所选航迹。"))); return; }
    TArray<FVector> Points; double Period=0;
    if(!ReadPoints(Points) || !FDefaultValueHelper::ParseDouble(Duration->GetText().ToString(),Period)) { Status->SetText(FText::FromString(TEXT("航点和时长必须是有效数字。"))); return; }
    auto* M=Manager();
    if(!M->EditRoutes(DraftIds,Points,Period,Closed->IsChecked())) { Status->SetText(FText::FromString(M->GetLastError())); return; }
    LoadSelection(); Status->SetText(FText::FromString(TEXT("已应用所选飞机航迹；可整体撤销。飞机仍静止。")));
}
void UDemoRouteWidget::Reset()
{
    if(!IsCurrent()) { Status->SetText(FText::FromString(TEXT("选择或场景已改变，请先重新读取。"))); return; }
    auto* M=Manager(); if(!M->ResetRoutes(DraftIds)) { Status->SetText(FText::FromString(M->GetLastError())); return; }
    LoadSelection(); Status->SetText(FText::FromString(TEXT("已移除所选飞机自定义航迹，使用默认来源；可撤销。")));
}
void UDemoRouteWidget::NativeTick(const FGeometry& Geometry,float Delta)
{
    Super::NativeTick(Geometry,Delta); RefreshSelection();
    if(!IsCurrent() || DraftIds.IsEmpty()) return;
    TArray<FVector> Points; if(!ReadPoints(Points)) return;
    for(int32 I=1;I<Points.Num();++I) DrawDebugLine(GetWorld(),Points[I-1]*100.,Points[I]*100.,FColor(40,160,255),false,0,0,3);
    if(Closed->IsChecked() && Points.Num()>2) DrawDebugLine(GetWorld(),Points.Last()*100.,Points[0]*100.,FColor(40,160,255),false,0,0,3);
    for(const auto& P:Points) DrawDebugPoint(GetWorld(),P*100.,12,FColor::Cyan,false,0);
}
