#include "DemoLayoutWidget.h"
#include "DemoScenarioManager.h"
#include "DemoGameMode.h"
#include "DemoUnitActor.h"
#include "DemoRouteWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/SizeBox.h"
#include "Components/CheckBox.h"
#include "ScenarioConfigLoader.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Brushes/SlateColorBrush.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/DefaultValueHelper.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CanvasPanelSlot.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
void UDemoLayoutWidget::EnsureInitialized() { Initialize(); if(!UnitPicker) NativeOnInitialized(); }
void UDemoLayoutWidget::SelectFormation(const FString& Name)
{
    RefreshFormations(); FormationPicker->SetSelectedOption(Name); ShowFormations();
    if(auto* M=Manager()) for(const auto& G:M->GetScenarioConfig().Units) for(const auto& I:G.Instances) if(I.Formation==Name)
    { if(auto* P=Player()) P->SelectUnit(I.Id); RefreshSelection(); return; }
}
bool UDemoLayoutWidget::BeginWindowDrag(const FVector2D& Position,const FGeometry& ParentGeometry)
{
    auto* CanvasSlot=Cast<UCanvasPanelSlot>(Slot);
    if(!CanvasSlot || ParentGeometry.GetLocalSize().IsNearlyZero()) return false;
    DragStartPointer=ParentGeometry.AbsoluteToLocal(Position); DragStartOffset=CanvasSlot->GetPosition();
    bDraggingWindow=true; return true;
}
void UDemoLayoutWidget::MoveWindowDrag(const FVector2D& Position,const FGeometry& ParentGeometry)
{
    if(!bDraggingWindow) return;
    auto* CanvasSlot=Cast<UCanvasPanelSlot>(Slot); if(!CanvasSlot) { bDraggingWindow=false; return; }
    const FVector2D CanvasSize=ParentGeometry.GetLocalSize(),WindowSize=CanvasSlot->GetSize();
    const FVector2D Origin=CanvasSlot->GetAnchors().Minimum*CanvasSize-CanvasSlot->GetAlignment()*WindowSize;
    FVector2D TopLeft=Origin+DragStartOffset+ParentGeometry.AbsoluteToLocal(Position)-DragStartPointer;
    TopLeft.X=FMath::Clamp(TopLeft.X,0.,FMath::Max(0.,CanvasSize.X-WindowSize.X));
    TopLeft.Y=FMath::Clamp(TopLeft.Y,0.,FMath::Max(0.,CanvasSize.Y-WindowSize.Y));
    CanvasSlot->SetPosition(TopLeft-Origin);
}
FReply UDemoLayoutWidget::NativeOnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
    auto* Parent=GetParent(); const FVector2D Point=E.GetScreenSpacePosition();
    if(E.GetEffectingButton()==EKeys::LeftMouseButton && Parent && DragTitle
        && DragTitle->GetCachedGeometry().IsUnderLocation(Point)
        && (!HeaderCloseButton || !HeaderCloseButton->GetCachedGeometry().IsUnderLocation(Point))
        && BeginWindowDrag(Point,Parent->GetCachedGeometry()))
    { if(auto* P=Player()) P->CancelUnitDrag(); return FReply::Handled().CaptureMouse(TakeWidget()); }
    return Super::NativeOnMouseButtonDown(G,E);
}
FReply UDemoLayoutWidget::NativeOnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
    if(bDraggingWindow)
    {
        if(!E.IsMouseButtonDown(EKeys::LeftMouseButton)) { bDraggingWindow=false; return FReply::Handled().ReleaseMouseCapture(); }
        if(auto* Parent=GetParent()) MoveWindowDrag(E.GetScreenSpacePosition(),Parent->GetCachedGeometry());
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(G,E);
}
FReply UDemoLayoutWidget::NativeOnMouseButtonUp(const FGeometry& G,const FPointerEvent& E)
{
    if(bDraggingWindow && E.GetEffectingButton()==EKeys::LeftMouseButton)
    { bDraggingWindow=false; return FReply::Handled().ReleaseMouseCapture(); }
    return Super::NativeOnMouseButtonUp(G,E);
}
void UDemoLayoutWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& E)
{ bDraggingWindow=false; Super::NativeOnMouseCaptureLost(E); }

void UDemoLayoutWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Border=WidgetTree->ConstructWidget<UBorder>(); WidgetTree->RootWidget=Border;
    Border->SetBrushColor(FLinearColor(.94f,.96f,.98f,1)); Border->SetPadding(FMargin(24));
    auto* Root=WidgetTree->ConstructWidget<UVerticalBox>(); Border->SetContent(Root);
    auto Text=[&](const FString& S,int32 Size=15)
    {
        auto* T=WidgetTree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(S)); T->SetAutoWrapText(true);
        T->SetFont(FSlateFontInfo(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),Size));
        T->SetColorAndOpacity(FLinearColor(.06f,.12f,.2f)); return T;
    };
    auto Field=[&]()
    {
        auto* E=WidgetTree->ConstructWidget<UEditableTextBox>(); const FLinearColor Ink(.03f,.07f,.12f,1);
        auto& Style=E->WidgetStyle; Style.SetFont(FSlateFontInfo(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),16));
        Style.TextStyle.SetColorAndOpacity(Ink); Style.SetForegroundColor(Ink).SetFocusedForegroundColor(Ink).SetReadOnlyForegroundColor(Ink);
        Style.SetBackgroundColor(FLinearColor::White); Style.SetPadding(FMargin(12,10));
        Style.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor::White));
        Style.SetBackgroundImageHovered(FSlateColorBrush(FLinearColor(.98f,.99f,1,1)));
        Style.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor::White)); return E;
    };
    auto Row=[&](UVerticalBox* Box) { auto* R=WidgetTree->ConstructWidget<UHorizontalBox>(); Box->AddChildToVerticalBox(R)->SetPadding(FMargin(0,8)); return R; };
    auto Button=[&](UHorizontalBox* R,const FString& S,FName Handler)
    {
        auto* B=WidgetTree->ConstructWidget<UButton>(); B->SetBackgroundColor(FLinearColor(.08f,.34f,.57f));
        auto* Label=Text(S); Label->SetAutoWrapText(false); Label->SetColorAndOpacity(FLinearColor::White); B->AddChild(Label);
        auto* ChildSlot=R->AddChildToHorizontalBox(B); ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ChildSlot->SetPadding(FMargin(0,0,8,0));
        FScriptDelegate D; D.BindUFunction(this,Handler); B->OnClicked.Add(D); return B;
    };
    auto* Header=Row(Root); auto* Title=Text(TEXT("场景编辑"),24);
    DragTitle=Header; Title->SetToolTipText(FText::FromString(TEXT("按住标题栏拖动窗口")));
    Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* CloseButton=Button(Header,TEXT("关闭"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,Close));
    HeaderCloseButton=CloseButton;
    Cast<UHorizontalBoxSlot>(CloseButton->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    auto* Tabs=Row(Root);
    Button(Tabs,TEXT("单位"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ShowUnits));
    Button(Tabs,TEXT("编队"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ShowFormations));
    Button(Tabs,TEXT("方案库"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ShowPresets));
    Button(Tabs,TEXT("航迹"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ShowRoutes));
    Pages=WidgetTree->ConstructWidget<UWidgetSwitcher>();
    Root->AddChildToVerticalBox(Pages)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto Page=[&]() { auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); Pages->AddChild(Scroll); auto* Box=WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Box); return Box; };
    auto* Box=Page();
    Box->AddChildToVerticalBox(Text(TEXT("选择单位"),14))->SetPadding(FMargin(0,12,0,6));
    UnitPicker=WidgetTree->ConstructWidget<UComboBoxString>(); Box->AddChildToVerticalBox(UnitPicker)->SetPadding(FMargin(0,0,0,12));
    UnitPicker->OnSelectionChanged.AddDynamic(this,&UDemoLayoutWidget::SelectionChanged);
    SelectionInfo=Text(TEXT("请选择单位"),13); Box->AddChildToVerticalBox(SelectionInfo);
    auto* Selections=Row(Box);
    Button(Selections,TEXT("全选飞机"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,SelectAllAircraft));
    Button(Selections,TEXT("清空多选"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ClearSelectedAircraft));
    auto* NewFormation=Row(Box);
    SelectedFormationName=Field(); SelectedFormationName->SetHintText(FText::FromString(TEXT("新编队名称，例如编队3")));
    NewFormation->AddChildToHorizontalBox(SelectedFormationName)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Button(NewFormation,TEXT("用当前多选建立编队"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,CreateFromSelection));
    Box->AddChildToVerticalBox(Text(TEXT("Ctrl＋点击增减多选。多选时，下方坐标为主选飞机位置；应用会整体平移，并统一增加朝向差值。"),13));
    const TCHAR* Labels[]={TEXT("X · 米"),TEXT("Y · 米"),TEXT("世界高度 · 米"),TEXT("朝向 · 度")};
    for(int32 R=0;R<2;++R)
    {
        auto* Fields=Row(Box);
        for(int32 C=0;C<2;++C)
        {
            auto* Column=WidgetTree->ConstructWidget<UVerticalBox>(); auto* ChildSlot=Fields->AddChildToHorizontalBox(Column);
            ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ChildSlot->SetPadding(FMargin(0,0,12,0));
            Column->AddChildToVerticalBox(Text(Labels[R*2+C],14))->SetPadding(FMargin(0,0,0,6));
            auto* E=Field(); Column->AddChildToVerticalBox(E); TransformFields.Add(E);
        }
    }
    Button(Row(Box),TEXT("应用到当前单选 / 多选"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ApplyTransform));
    Box->AddChildToVerticalBox(Text(TEXT("场景中左键拖动，Esc取消。高度为世界坐标；0°保留模型原始方向。"),13))->SetPadding(FMargin(0,8));
    auto* Formation=Page();
    FormationPicker=WidgetTree->ConstructWidget<UComboBoxString>(); Formation->AddChildToVerticalBox(FormationPicker);
    FormationPicker->OnSelectionChanged.AddDynamic(this,&UDemoLayoutWidget::FormationChanged);
    Button(Row(Formation),TEXT("选择或修改成员"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ToggleMembers));
    MemberSection=WidgetTree->ConstructWidget<UVerticalBox>(); Formation->AddChildToVerticalBox(MemberSection);
    FormationName=Field(); FormationName->SetHintText(FText::FromString(TEXT("编队名称，可新建或修改成员")));
    MemberSection->AddChildToVerticalBox(FormationName)->SetPadding(FMargin(0,8));
    auto* MemberArea=WidgetTree->ConstructWidget<USizeBox>(); MemberArea->SetHeightOverride(130);
    MemberSection->AddChildToVerticalBox(MemberArea);
    auto* MemberScroll=WidgetTree->ConstructWidget<UScrollBox>(); MemberArea->SetContent(MemberScroll);
    MemberList=WidgetTree->ConstructWidget<UVerticalBox>(); MemberScroll->AddChild(MemberList);
    Button(Row(MemberSection),TEXT("保存勾选成员"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,SaveFormation));
    PatternPicker=WidgetTree->ConstructWidget<UComboBoxString>();
    for(const TCHAR* S:{TEXT("横排"),TEXT("纵列"),TEXT("楔形")}) PatternPicker->AddOption(S);
    PatternPicker->SetSelectedIndex(0); Formation->AddChildToVerticalBox(PatternPicker);
    const TCHAR* FormationLabels[]={TEXT("起点 X · 米"),TEXT("起点 Y · 米"),TEXT("间距 · 米"),TEXT("方向 · 度")};
    for(int32 R=0;R<2;++R)
    {
        auto* Inputs=Row(Formation);
        for(int32 C=0;C<2;++C)
        {
            auto* Column=WidgetTree->ConstructWidget<UVerticalBox>(); auto* ChildSlot=Inputs->AddChildToHorizontalBox(Column);
            ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ChildSlot->SetPadding(FMargin(0,0,12,0));
            Column->AddChildToVerticalBox(Text(FormationLabels[R*2+C],14))->SetPadding(FMargin(0,0,0,6));
            auto* E=Field(); Column->AddChildToVerticalBox(E); FormationFields.Add(E);
        }
    }
    FormationFields[2]->SetText(FText::FromString(TEXT("35"))); FormationFields[3]->SetText(FText::FromString(TEXT("0")));
    Button(Row(Formation),TEXT("排列当前编队"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ApplyFormation));
    Formation->AddChildToVerticalBox(Text(TEXT("先保存成员，再排列。保留各架高度；一次排列可整体撤销。方向0°沿世界X轴，首架为起点。"),13));
    auto* Library=Page();
    Library->AddChildToVerticalBox(Text(TEXT("保存当前场景"),17))->SetPadding(FMargin(0,12,0,6));
    PresetName=Field(); PresetName->SetHintText(FText::FromString(TEXT("方案名称"))); Library->AddChildToVerticalBox(PresetName);
    Button(Row(Library),TEXT("另存为新方案"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,SavePreset));
    Library->AddChildToVerticalBox(Text(TEXT("已保存方案"),17))->SetPadding(FMargin(0,12,0,6));
    PresetPicker=WidgetTree->ConstructWidget<UComboBoxString>(); Library->AddChildToVerticalBox(PresetPicker);
    auto* Loads=Row(Library); Button(Loads,TEXT("加载方案"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,LoadPreset));
    Button(Loads,TEXT("刷新"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,RefreshPresets));
    Library->AddChildToVerticalBox(Text(TEXT("JSON 导入 / 导出"),17))->SetPadding(FMargin(0,16,0,6));
    FilePath=Field(); FilePath->SetHintText(FText::FromString(TEXT("完整文件路径，如 E:/方案.json"))); Library->AddChildToVerticalBox(FilePath);
    auto* Files=Row(Library); Button(Files,TEXT("导入"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ImportPreset)); Button(Files,TEXT("导出"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,ExportPreset));
    Library->AddChildToVerticalBox(Text(TEXT("导入后选择并加载。方案包含地图；同名保存及文件覆盖会被拒绝。"),13));
    auto* RoutePage=Page(); RouteEditor=WidgetTree->ConstructWidget<UDemoRouteWidget>(); RouteEditor->SetOwningPlayer(GetOwningPlayer()); RouteEditor->EnsureInitialized(); RoutePage->AddChildToVerticalBox(RouteEditor);
    Feedback=Text(TEXT("选择飞机后可批量编辑、建立编队或修改航迹；修改均可撤销。"),13); Root->AddChildToVerticalBox(Feedback)->SetPadding(FMargin(0,12));
    auto* History=Row(Root); UndoButton=Button(History,TEXT("撤销"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,Undo)); RedoButton=Button(History,TEXT("重做"),GET_FUNCTION_NAME_CHECKED(UDemoLayoutWidget,Redo));
    RefreshPresets(); RefreshSelection();
}
void UDemoLayoutWidget::ShowUnits() { Pages->SetActiveWidgetIndex(0); }
void UDemoLayoutWidget::ShowFormations() { RefreshSelection(); Pages->SetActiveWidgetIndex(1); }
void UDemoLayoutWidget::ToggleMembers()
{ MemberSection->SetVisibility(MemberSection->GetVisibility()==ESlateVisibility::Collapsed?ESlateVisibility::Visible:ESlateVisibility::Collapsed); }
void UDemoLayoutWidget::ShowPresets() { Pages->SetActiveWidgetIndex(2); }
void UDemoLayoutWidget::ShowRoutes() { RouteEditor->RefreshSelection(); Pages->SetActiveWidgetIndex(3); }
void UDemoLayoutWidget::SelectAllAircraft()
{
    auto* P=Player(); auto* M=Manager(); if(!P || !M) return;
    P->SelectUnit(FString());
    for(const auto& G:M->GetScenarioConfig().Units) if(G.Type==TEXT("aircraft"))
        for(ADemoUnitActor* U:M->GetUnits()) if(G.Instances.ContainsByPredicate([&](const FDemoUnitInstance& I){return I.Id==U->UnitId;}) || (G.Instances.IsEmpty() && U->UnitId.StartsWith(G.Id+TEXT("_")))) P->ToggleUnitSelection(U->UnitId);
    RefreshSelection();
}
void UDemoLayoutWidget::ClearSelectedAircraft() { if(auto* P=Player()) P->SelectUnit(FString()); RefreshSelection(); }
void UDemoLayoutWidget::CreateFromSelection()
{
    auto* P=Player(); auto* M=Manager(); if(!P || !M) { Result(TEXT("请在场景中选中飞机。")); return; }
    P->CancelUnitDrag();
    const FString Name=SelectedFormationName->GetText().ToString().TrimStartAndEnd();
    const auto Ids=P->SelectedUnitIds();
    Result(M->CreateSelectedFormation(Name,Ids)?FString::Printf(TEXT("已用%d架选中飞机建立%s，可撤销。"),Ids.Num(),*Name):M->GetLastError());
    RefreshSelection();
}
void UDemoLayoutWidget::RefreshFormations()
{
    auto* M=Manager(); if(!M || !FormationPicker) return;
    const FString Previous=FormationPicker->GetSelectedOption();
    FDemoScenarioConfig Config; FString Error;
    if(!FScenarioConfigLoader::Parse(M->SnapshotJson(),Config,Error)) return;
    TArray<FString> Names={TEXT("编队1"),TEXT("编队2")};
    for(const auto& Group:Config.Units) for(const auto& Instance:Group.Instances)
        if(!Instance.Formation.IsEmpty()) Names.AddUnique(Instance.Formation);
    const bool WasRefreshing=bRefreshing; bRefreshing=true;
    FormationPicker->ClearOptions(); for(const auto& Name:Names) FormationPicker->AddOption(Name);
    if(Names.Contains(Previous)) FormationPicker->SetSelectedOption(Previous); else FormationPicker->SetSelectedIndex(0);
    bRefreshing=WasRefreshing; FormationRevision=M->GetRevision(); RefreshMembers();
}
void UDemoLayoutWidget::FormationChanged(FString,ESelectInfo::Type)
{ if(!bRefreshing) RefreshMembers(); }
void UDemoLayoutWidget::RefreshMembers()
{
    auto* M=Manager(); if(!M || !MemberList) return;
    const FString Name=FormationPicker->GetSelectedOption(); FormationName->SetText(FText::FromString(Name));
    FDemoScenarioConfig Config; FString Error;
    if(!FScenarioConfigLoader::Parse(M->SnapshotJson(),Config,Error)) return;
    MemberList->ClearChildren(); MemberChecks.Reset(); MemberIds.Reset();
    bool HaveAnchor=false;
    for(const auto& Group:Config.Units) for(const auto& Instance:Group.Instances)
    {
        if(Group.Type!=TEXT("aircraft")) continue;
        auto* Check=WidgetTree->ConstructWidget<UCheckBox>();
        Check->SetIsChecked(Instance.Formation==Name);
        auto* Label=WidgetTree->ConstructWidget<UTextBlock>();
        FString S=Instance.Id+TEXT(" · ")+Group.ModelId+TEXT(" · ")+(Group.Side==TEXT("red")?TEXT("红方"):TEXT("蓝方"));
        if(!Instance.Formation.IsEmpty() && Instance.Formation!=Name) S+=TEXT(" · ")+Instance.Formation;
        Label->SetText(FText::FromString(S)); Label->SetColorAndOpacity(FLinearColor(.06f,.12f,.2f));
        Label->SetFont(FSlateFontInfo(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),14));
        Check->AddChild(Label); MemberList->AddChildToVerticalBox(Check)->SetPadding(FMargin(0,4));
        MemberChecks.Add(Check); MemberIds.Add(Instance.Id);
        if(!HaveAnchor && Instance.Formation==Name)
        {
            FormationFields[0]->SetText(FText::FromString(FString::Printf(TEXT("%.3f"),Instance.PositionMeters.X)));
            FormationFields[1]->SetText(FText::FromString(FString::Printf(TEXT("%.3f"),Instance.PositionMeters.Y)));
            FormationFields[3]->SetText(FText::FromString(FString::Printf(TEXT("%.3f"),Instance.Yaw))); HaveAnchor=true;
        }
    }
    if(!HaveAnchor && M->GetUnits().Num())
    {
        const FVector P=M->GetUnits()[0]->GetActorLocation()/100.;
        FormationFields[0]->SetText(FText::FromString(FString::Printf(TEXT("%.3f"),P.X)));
        FormationFields[1]->SetText(FText::FromString(FString::Printf(TEXT("%.3f"),P.Y)));
    }
    MemberSection->SetVisibility(HaveAnchor?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
}
void UDemoLayoutWidget::SaveFormation()
{
    auto* M=Manager(); if(!M) return; if(auto* P=Player()) P->CancelUnitDrag();
    TArray<FString> Ids; for(int32 I=0;I<MemberIds.Num();++I) if(MemberChecks[I]->IsChecked()) Ids.Add(MemberIds[I]);
    const FString Name=FormationName->GetText().ToString().TrimStartAndEnd();
    if(!M->SetFormationMembers(Name,Ids)) { Result(M->GetLastError()); return; }
    if(FormationPicker->FindOptionIndex(Name)<0) FormationPicker->AddOption(Name);
    FormationPicker->SetSelectedOption(Name); RefreshSelection();
    Result(FString::Printf(TEXT("%s：已保存%d架成员。其他编队保留，转入成员从原编队移除。"),*Name,Ids.Num()));
}
void UDemoLayoutWidget::ApplyFormation()
{
    auto* M=Manager(); if(!M) return; if(auto* P=Player()) P->CancelUnitDrag();
    const FString Name=FormationPicker->GetSelectedOption();
    FDemoScenarioConfig Config; FString Error;
    if(!FScenarioConfigLoader::Parse(M->SnapshotJson(),Config,Error)) { Result(Error); return; }
    TSet<FString> Saved; for(const auto& G:Config.Units) for(const auto& I:G.Instances) if(I.Formation==Name) Saved.Add(I.Id);
    if(FormationName->GetText().ToString().TrimStartAndEnd()!=Name) { Result(TEXT("名称已修改，请先保存勾选成员。")); return; }
    for(int32 I=0;I<MemberIds.Num();++I) if(MemberChecks[I]->IsChecked()!=Saved.Contains(MemberIds[I]))
    { Result(TEXT("成员勾选已修改，请先保存成员，再排列。")); return; }
    double Values[4];
    for(int32 I=0;I<4;++I) if(!FDefaultValueHelper::ParseDouble(FormationFields[I]->GetText().ToString().TrimStartAndEnd(),Values[I]) || !FMath::IsFinite(Values[I]))
    { Result(TEXT("起点、间距和方向需要完整有效的数字。")); return; }
    const bool Ok=M->ArrangeFormation(Name,PatternPicker->GetSelectedIndex(),Values[2],Values[3],FVector2D(Values[0],Values[1]));
    Result(Ok?TEXT("已排列当前编队；高度保留，其他飞机不变，可整体撤销。"):M->GetLastError()); RefreshSelection();
}
ADemoScenarioManager* UDemoLayoutWidget::Manager() const
{ for(TActorIterator<ADemoScenarioManager> It(GetWorld());It;++It) return *It; return nullptr; }
ADemoPlayerController* UDemoLayoutWidget::Player() const
{
    if(auto* P=Cast<ADemoPlayerController>(GetOwningPlayer())) return P;
    return GetWorld()?Cast<ADemoPlayerController>(GetWorld()->GetFirstPlayerController()):nullptr;
}
void UDemoLayoutWidget::Result(const FString& Message) { if(Feedback) Feedback->SetText(FText::FromString(Message)); }
void UDemoLayoutWidget::NativeTick(const FGeometry& Geometry,float Delta)
{ Super::NativeTick(Geometry,Delta); RefreshSelection(); }
void UDemoLayoutWidget::RefreshSelection()
{
    auto* M=Manager(); if(!M || !UnitPicker) return;
    if(FormationRevision!=M->GetRevision()) RefreshFormations();
    FString Signature; for(ADemoUnitActor* Unit:M->GetUnits()) Signature+=Unit->UnitId+TEXT(";");
    bRefreshing=true;
    if(Signature!=SelectionSignature)
    {
        const FString Previous=UnitPicker->GetSelectedOption(); SelectionSignature=Signature; UnitPicker->ClearOptions();
        for(ADemoUnitActor* Unit:M->GetUnits()) UnitPicker->AddOption(Unit->UnitId);
        if(UnitPicker->FindOptionIndex(Previous)>=0) UnitPicker->SetSelectedOption(Previous);
        else if(UnitPicker->GetOptionCount()) UnitPicker->SetSelectedIndex(0);
    }
    if(auto* P=Player())
    {
        if(auto* Unit=P->SelectedUnit()) UnitPicker->SetSelectedOption(Unit->UnitId);
        else UnitPicker->ClearSelection();
    }
    bRefreshing=false;
    UndoButton->SetIsEnabled(M->CanUndo()); RedoButton->SetIsEnabled(M->CanRedo());
    const FString Id=UnitPicker->GetSelectedOption(); auto* Unit=M->FindUnit(Id);
    const uint64 SelectionVersion=Player()?Player()->SelectionRevision():0;
    if(SeenRevision==M->GetRevision() && DisplayedUnitId==Id && SeenSelectionRevision==SelectionVersion) return;
    SeenSelectionRevision=SelectionVersion;
    SeenRevision=M->GetRevision(); DisplayedUnitId=Id;
    const int32 SelectedCount=Player()?Player()->SelectedUnitIds().Num():(Unit?1:0);
    SelectionInfo->SetText(FText::FromString(Unit?FString::Printf(TEXT("已选%d个单位 · 主选：%s"),SelectedCount,*Id):TEXT("请在场景中点击飞机，Ctrl＋点击多选，或点击全选飞机。")));
    for(UEditableTextBox* E:TransformFields) E->SetIsEnabled(Unit!=nullptr);
    if(!Unit) return;
    const FVector Position=Unit->GetActorLocation()/100.; const double Values[]={Position.X,Position.Y,Position.Z,Unit->GetActorRotation().Yaw};
    for(int32 I=0;I<4;++I) TransformFields[I]->SetText(FText::FromString(FString::Printf(TEXT("%.6f"),Values[I])));
}
void UDemoLayoutWidget::SelectionChanged(FString Id,ESelectInfo::Type)
{
    if(bRefreshing) return; if(auto* P=Player()) P->SelectUnit(Id);
    SeenRevision=MAX_uint64; RefreshSelection();
}
void UDemoLayoutWidget::ApplyTransform()
{
    auto* M=Manager(); if(!M) return; if(auto* P=Player()) P->CancelUnitDrag();
    if(SeenRevision!=M->GetRevision() || (Player() && SeenSelectionRevision!=Player()->SelectionRevision()))
    { Result(TEXT("场景或选择已变化，已刷新输入。请检查后重新应用。")); RefreshSelection(); return; }
    double Values[4];
    for(int32 I=0;I<4;++I)
    {
        const FString S=TransformFields[I]->GetText().ToString().TrimStartAndEnd();
        if(!FDefaultValueHelper::ParseDouble(S,Values[I]) || !FMath::IsFinite(Values[I])) { Result(TEXT("四个输入框都需要完整、有效的数字。")); return; }
    }
    const auto Ids=Player()?Player()->SelectedUnitIds():TArray<FString>{UnitPicker->GetSelectedOption()};
    Result(M->EditUnits(Ids,UnitPicker->GetSelectedOption(),FVector(Values[0],Values[1],Values[2]),Values[3])?TEXT("已修改当前选择，保留相对位置；可整体撤销。"):M->GetLastError());
    RefreshSelection();
}
void UDemoLayoutWidget::Undo()
{ if(auto* P=Player()) P->CancelUnitDrag(); if(auto* M=Manager()) Result(M->UndoEdit()?TEXT("已撤销上一次单位编辑。"):TEXT("没有可撤销的编辑。")); RefreshSelection(); }
void UDemoLayoutWidget::Redo()
{ if(auto* P=Player()) P->CancelUnitDrag(); if(auto* M=Manager()) Result(M->RedoEdit()?TEXT("已重做单位编辑。"):TEXT("没有可重做的编辑。")); RefreshSelection(); }
void UDemoLayoutWidget::SavePreset()
{
    auto* M=Manager(); if(!M) return; if(auto* P=Player()) P->CancelUnitDrag();
    FString Id,Error;
    if(!FScenarioPresetStore().SaveNew(PresetName->GetText().ToString(),M->SnapshotJson(),Id,Error)) { Result(Error); return; }
    RefreshPresets();
    for(int32 I=0;I<Presets.Num();++I) if(Presets[I].Id==Id) PresetPicker->SetSelectedIndex(I);
    Result(TEXT("方案已保存，重启后可从列表加载。"));
}
void UDemoLayoutWidget::RefreshPresets()
{
    if(!PresetPicker) return; const FString Previous=PresetPicker->GetSelectedOption();
    Presets=FScenarioPresetStore().List(); PresetPicker->ClearOptions();
    for(const auto& Entry:Presets) PresetPicker->AddOption(Entry.Name+TEXT(" · ")+FPaths::GetBaseFilename(Entry.MapAsset));
    if(PresetPicker->FindOptionIndex(Previous)>=0) PresetPicker->SetSelectedOption(Previous);
    else if(Presets.Num()) PresetPicker->SetSelectedIndex(0);
}
void UDemoLayoutWidget::LoadPreset()
{
    auto* M=Manager(); const int32 Index=PresetPicker->GetSelectedIndex();
    if(!M || !Presets.IsValidIndex(Index)) { Result(TEXT("请先选择方案。")); return; }
    FString Json,Error,Map; const FString Id=Presets[Index].Id;
    if(!FScenarioPresetStore().Read(Id,Json,Error) || !FScenarioPresetStore::ValidateResources(Json,Map,Error)) { Result(Error); return; }
    if(auto* P=Player()) P->CancelUnitDrag();
    if(!Map.IsEmpty() && !M->CurrentMapAsset().IsEmpty() && Map!=M->CurrentMapAsset())
    { UGameplayStatics::OpenLevel(this,FName(*Map),true,TEXT("game=/Script/TacticalDemo.DemoGameMode?LayoutPreset=")+Id); return; }
    Result(M->LoadJson(Json,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"))?TEXT("已恢复方案的位置、朝向、型号及贴图。"):M->GetLastError()); RefreshSelection();
}
void UDemoLayoutWidget::ImportPreset()
{
    FString Id,Error; const FString Path=FilePath->GetText().ToString().TrimStartAndEnd();
    FString Name=PresetName->GetText().ToString().TrimStartAndEnd(); if(Name.IsEmpty()) Name=FPaths::GetBaseFilename(Path);
    if(!FScenarioPresetStore().Import(Path,Name,Id,Error)) { Result(Error); return; }
    RefreshPresets(); Result(TEXT("已导入方案库，选择后点击加载才会应用。"));
}
void UDemoLayoutWidget::ExportPreset()
{
    const int32 Index=PresetPicker->GetSelectedIndex(); if(!Presets.IsValidIndex(Index)) { Result(TEXT("请先选择方案。")); return; }
    FString Error;
    Result(FScenarioPresetStore().Export(Presets[Index].Id,FilePath->GetText().ToString().TrimStartAndEnd(),Error)?TEXT("已导出方案 JSON。"):Error);
}
void UDemoLayoutWidget::Close() { bDraggingWindow=false; SetVisibility(ESlateVisibility::Collapsed); }
