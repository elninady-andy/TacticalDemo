#include "DemoControlWidget.h"
#include "FlightPreview.h"
#include "FormationCommands.h"
#include "HighlightCommands.h"
#include "ChatProtocol.h"
#include "DemoScenarioManager.h"
#include "DemoGameMode.h"
#include "DemoLayoutWidget.h"
#include "DemoUnitActor.h"
#include "EquipmentLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "SceneLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Brushes/SlateColorBrush.h"

namespace
{
void SetReadableInputStyle(UEditableTextBox* Field, int32 FontSize)
{
    const FLinearColor Ink(.02f,.04f,.07f,1.f);
    auto& Style = Field->WidgetStyle;
    Style.SetFont(FSlateFontInfo(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),FontSize));
    // Slate uses separate text and focus colors; set all states so typing stays readable.
    Style.TextStyle.SetColorAndOpacity(Ink);
    Style.SetForegroundColor(Ink).SetFocusedForegroundColor(Ink).SetReadOnlyForegroundColor(Ink);
    Style.SetBackgroundColor(FLinearColor::White);
    Style.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor(.94f,.96f,.98f,1.f)));
    Style.SetBackgroundImageHovered(FSlateColorBrush(FLinearColor::White));
    Style.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor::White));
    Style.SetBackgroundImageReadOnly(FSlateColorBrush(FLinearColor(.88f,.91f,.94f,1.f)));
    Style.SetPadding(FMargin(10,8));
}
}

UTextBlock* UDemoControlWidget::Text(const FString& Value, int32 Size)
{
    auto* Widget = WidgetTree->ConstructWidget<UTextBlock>();
    Widget->SetText(FText::FromString(Value));
    Widget->SetFont(FSlateFontInfo(FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"), Size));
    Widget->SetColorAndOpacity(FSlateColor(FLinearColor(.86f,.91f,1.f)));
    Widget->SetAutoWrapText(true);
    return Widget;
}
UButton* UDemoControlWidget::Button(UVerticalBox* Parent, const FString& Label, FName Handler)
{
    auto* B = WidgetTree->ConstructWidget<UButton>();
    B->SetBackgroundColor(FLinearColor(.12f,.3f,.48f));
    auto* LabelText=Text(Label); LabelText->SetAutoWrapText(false); B->AddChild(LabelText);
    Parent->AddChildToVerticalBox(B)->SetPadding(FMargin(0,5));
    FScriptDelegate Delegate;
    Delegate.BindUFunction(this, Handler);
    B->OnClicked.Add(Delegate);
    return B;
}
void UDemoControlWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto Panel = [&](FAnchors Anchors, FMargin Offsets)
    {
        auto* Border = WidgetTree->ConstructWidget<UBorder>();
        InputPanels.Add(Border);
        Border->SetBrushColor(FLinearColor(.018f,.035f,.063f,.97f));
        Border->SetPadding(FMargin(14));
        auto* Slot = Root->AddChildToCanvas(Border);
        Slot->SetAnchors(Anchors); Slot->SetOffsets(Offsets);
        auto* Box = WidgetTree->ConstructWidget<UVerticalBox>();
        Border->SetContent(Box);
        return Box;
    };
    auto* Top = Panel(FAnchors(0,0,1,0), FMargin(12,12,12,66));
    Status = Text(TEXT("态势场景 · 本地 Qwen 对话"), 20);
    Top->AddChildToVerticalBox(Status);
    auto* Left = Panel(FAnchors(0,0,0,1), FMargin(12,90,440,16));
    Left->AddChildToVerticalBox(Text(TEXT("对话助手"),22));
    Left->AddChildToVerticalBox(Text(TEXT("描述想要的场景，生成后点击应用。\n编队示例：把红方飞机编成编队3\n特效示例：让选中的飞机高亮\n输入“编队帮助”或“高亮帮助”查看用法。"),13));
    HistoryBox = WidgetTree->ConstructWidget<UScrollBox>();
    Left->AddChildToVerticalBox(HistoryBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Input = WidgetTree->ConstructWidget<UEditableTextBox>();
    SetReadableInputStyle(Input,17);
    Input->SetHintText(FText::FromString(TEXT("输入指令，例如：创建3架红方飞机")));
    Left->AddChildToVerticalBox(Input)->SetPadding(FMargin(0,8));
    SendButton = Button(Left,TEXT("发送消息"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,Send));
    Button(Left,TEXT("取消请求"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,Cancel));
    auto* Right = Panel(FAnchors(1,0,1,1),FMargin(-360,90,348,16));
    auto* RightBorder=Cast<UBorder>(Right->GetParent());
    Right->RemoveFromParent();
    auto* RightScroll=WidgetTree->ConstructWidget<UScrollBox>(); RightBorder->SetContent(RightScroll); RightScroll->AddChild(Right);
    Right->AddChildToVerticalBox(Text(TEXT("本地模型连接"),18));
    auto Edit = [&](const FString& Value)
    {
        auto* E = WidgetTree->ConstructWidget<UEditableTextBox>();
        SetReadableInputStyle(E,14);
        E->SetText(FText::FromString(Value));
        Right->AddChildToVerticalBox(E)->SetPadding(FMargin(0,5));
        return E;
    };
    FString Url, Name, Settings;
    if (FFileHelper::LoadFileToString(Settings,*(FPaths::ProjectConfigDir()/TEXT("Qwen.json"))))
    {
        TSharedPtr<FJsonObject> O;
        if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Settings),O) && O.IsValid())
        { O->TryGetStringField(TEXT("endpoint"),Url); O->TryGetStringField(TEXT("model"),Name); }
    }
    Right->AddChildToVerticalBox(Text(TEXT("OpenAI兼容接口完整地址"),12));
    Endpoint=Edit(Url);
    Endpoint->SetHintText(FText::FromString(TEXT("http://127.0.0.1:端口/v1/chat/completions")));
    Right->AddChildToVerticalBox(Text(TEXT("模型名称"),12));
    Model=Edit(Name);
    Right->AddChildToVerticalBox(Text(TEXT("单位信息"),18));
    Units=WidgetTree->ConstructWidget<UComboBoxString>();
    Units->OnSelectionChanged.AddDynamic(this,&UDemoControlWidget::UnitSelectionChanged);
    Right->AddChildToVerticalBox(Units);
    Details=Text(TEXT("尚未选中单位"),14);
    Right->AddChildToVerticalBox(Details);
    Button(Right,TEXT("单位编辑与方案管理"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,ToggleLayout));
    CandidateInfo=Text(TEXT("输入指令开始修改场景。"),13);
    Right->AddChildToVerticalBox(CandidateInfo)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ApplyButton=Button(Right,TEXT("应用到场景"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,Apply));
    ApplyButton->SetIsEnabled(false);
    Button(Right,TEXT("加载原始配置"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,Reload));
    Button(Right,TEXT("清空场景"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,Clear));
    Button(Right,TEXT("重新取景"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,Frame));
    FlightButton=Button(Right,TEXT("播放轨迹"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,ToggleFlightPreview));
    Button(Right,TEXT("资源与导入配置"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,ToggleResources));
    ResourceWindow=WidgetTree->ConstructWidget<UBorder>();
    InputPanels.Add(ResourceWindow);
    ResourceWindow->SetBrushColor(FLinearColor(.025f,.045f,.075f,1.f));
    ResourceWindow->SetPadding(FMargin(18));
    auto* ResourceSlot=Root->AddChildToCanvas(ResourceWindow);
    ResourceSlot->SetAnchors(FAnchors(.5f,.5f));
    ResourceSlot->SetAlignment(FVector2D(.5f,.5f));
    ResourceSlot->SetOffsets(FMargin(0,0,720,850));
    ResourceSlot->SetZOrder(10);
    auto* ResourceScroll=WidgetTree->ConstructWidget<UScrollBox>();
    ResourceWindow->SetContent(ResourceScroll);
    auto* ResourceBox=WidgetTree->ConstructWidget<UVerticalBox>();
    ResourceScroll->AddChild(ResourceBox);
    ResourceBox->AddChildToVerticalBox(Text(TEXT("资源与导入配置"),22));
    ResourceBox->AddChildToVerticalBox(Text(TEXT("保存本地来源路径，下次运行自动恢复。"),14));
    const TCHAR* Labels[]={TEXT("场景 · Scenario JSON 文件"),TEXT("涂装 · 材质/贴图资源来源（待接入）"),TEXT("模型 · 模型库资源来源（待接入）"),TEXT("轨迹/弹道 · 数据来源（待接入）")};
    for(const TCHAR* Label:Labels)
    {
        ResourceBox->AddChildToVerticalBox(Text(Label,14));
        auto* Field=WidgetTree->ConstructWidget<UEditableTextBox>();
        SetReadableInputStyle(Field,14);
        Field->SetHintText(FText::FromString(TEXT("粘贴本地文件或目录的完整路径")));
        ResourceBox->AddChildToVerticalBox(Field)->SetPadding(FMargin(0,4,0,10));
        ResourcePaths.Add(Field);
    }
    ResourceStatus=Text(TEXT(""),13);
    ResourceBox->AddChildToVerticalBox(ResourceStatus);
    Button(ResourceBox,TEXT("保存来源配置"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,SaveResources));
    Button(ResourceBox,TEXT("恢复来源配置"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,LoadResources));
    Button(ResourceBox,TEXT("加载场景配置"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,LoadResourceScene));
    ResourceBox->AddChildToVerticalBox(Text(TEXT("模型和贴图切换使用已登记的资源；J20/J10各有两款贴图。航迹为静态航点，外部涂装文件和弹道导入仍待接入。"),13));
    Button(ResourceBox,TEXT("关闭配置窗口"),GET_FUNCTION_NAME_CHECKED(UDemoControlWidget,ToggleResources));
    LoadResources();
    ResourceWindow->SetVisibility(FParse::Param(FCommandLine::Get(),TEXT("DemoResources"))?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    LayoutWindow=GetOwningPlayer()?CreateWidget<UDemoLayoutWidget>(GetOwningPlayer(),UDemoLayoutWidget::StaticClass()):NewObject<UDemoLayoutWidget>(this);
    LayoutWindow->EnsureInitialized();
    InputPanels.Add(LayoutWindow);
    auto* LayoutSlot=Root->AddChildToCanvas(LayoutWindow); LayoutSlot->SetAnchors(FAnchors(.5f,.5f));
    LayoutSlot->SetAlignment(FVector2D(.5f,.5f)); LayoutSlot->SetOffsets(FMargin(0,0,760,660)); LayoutSlot->SetZOrder(11);
    LayoutWindow->SetVisibility(ESlateVisibility::Collapsed);
    AddMessage(TEXT("提示"),TEXT("可尝试：\n把红方飞机的贴图换成第二款\n把蓝方飞机的贴图换成第一款\n把J20飞机的贴图换成第一款\n把红方飞机换成J20\n切换到机场场景\n每次发送一条，回复后点击应用。\n默认静止；map3可手动播放轨迹。"));
}
ADemoScenarioManager* UDemoControlWidget::Manager() const
{
    for (TActorIterator<ADemoScenarioManager> It(GetWorld()); It; ++It) return *It;
    return nullptr;
}
bool UDemoControlWidget::BlocksScenePointer(const FVector2D& Position) const
{
    for(UWidget* Panel:InputPanels)
        if(IsValid(Panel) && Panel->IsVisible() && Panel->GetCachedGeometry().IsUnderLocation(Position)) return true;
    return false;
}
void UDemoControlWidget::NativeTick(const FGeometry& Geometry,float Delta)
{
    Super::NativeTick(Geometry,Delta);
    auto* M=Manager(); if(!M || !Status) return;
    bool Playing=false;
    if(FlightButton)
    {
        FlightButton->SetVisibility(ESlateVisibility::Visible);
        for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) Playing|=It->IsPlaying();
        if(auto* Label=Cast<UTextBlock>(FlightButton->GetChildAt(0))) Label->SetText(FText::FromString(Playing?TEXT("停止轨迹并复位"):TEXT("播放轨迹")));
    }
    if(ObservedRevision!=M->GetRevision())
    {
        if(ObservedRevision!=MAX_uint64)
        {
            if(ActiveRequest.IsValid() || !Candidate.IsEmpty() || !CandidateLevel.IsEmpty()) Cancel();
            Messages.Reset();
        }
        ObservedRevision=M->GetRevision();
    }
    Status->SetText(FText::FromString(FString::Printf(TEXT("态势场景  /  %s  ·  %d 个单位  |  %s"),*M->GetScenarioName(),M->GetUnits().Num(),Playing?TEXT("轨迹预览 · 跟随所选编组 · Esc停止"):ActiveRequest.IsValid()?TEXT("Qwen 正在回复…"):TEXT("按住右键观察 / WASD移动"))));
    FString Signature;
    for(ADemoUnitActor* U:M->GetUnits()) if(IsValid(U)) Signature+=U->UnitId+TEXT(";");
    if(Signature!=UnitSignature)
    {
        bRefreshingUnits=true;
        const FString Previous=Units->GetSelectedOption();
        UnitSignature=Signature; Units->ClearOptions();
        for(ADemoUnitActor* U:M->GetUnits()) if(IsValid(U)) Units->AddOption(U->UnitId);
        if(Units->FindOptionIndex(Previous)>=0) Units->SetSelectedOption(Previous);
        else if(Units->GetOptionCount()) Units->SetSelectedIndex(0);
        bRefreshingUnits=false;
    }
    if(auto* P=Cast<ADemoPlayerController>(GetOwningPlayer()))
    {
        bRefreshingUnits=true;
        if(auto* Selected=P->SelectedUnit()) Units->SetSelectedOption(Selected->UnitId); else Units->ClearSelection();
        bRefreshingUnits=false;
    }
    Details->SetText(FText::FromString(TEXT("无单位")));
    for(ADemoUnitActor* U:M->GetUnits()) if(IsValid(U) && U->UnitId==Units->GetSelectedOption())
    {
        const FVector P=U->GetActorLocation()/100.;
        int32 RoutePoints = 0;
        FString ModelId;
        FString TextureId;
        for (const FDemoUnitConfig& Group : M->GetScenarioConfig().Units)
            if (Group.Instances.ContainsByPredicate([&](const auto& Instance){return Instance.Id==U->UnitId;}) || (Group.Instances.IsEmpty() && U->UnitId.StartsWith(Group.Id+TEXT("_")))) { RoutePoints=Group.RouteMeters.Num(); ModelId=Group.ModelId; TextureId=Group.TextureId; break; }
        Details->SetText(FText::FromString(FString::Printf(TEXT("%s  %s\n编辑位置 X %.1f / Y %.1f / Z %.2f 米\n朝向 %.1f° · 静态航点 %d\n模型 %s · 贴图 %s\n%s"),*U->UnitId,U->Side==TEXT("red")?TEXT("红方"):TEXT("蓝方"),P.X,P.Y,P.Z,U->GetActorRotation().Yaw,RoutePoints,*ModelId,TextureId.IsEmpty()?TEXT("原始"):*TextureId,Playing?TEXT("预览不改方案；停止后可编辑"):TEXT("左键选择并拖动，Esc取消"))));
        break;
    }
}
void UDemoControlWidget::AddMessage(const FString& Speaker,const FString& Value)
{
    HistoryBox->AddChild(Text(Speaker+TEXT("\n")+Value+TEXT("\n"),14));
    while(HistoryBox->GetChildrenCount()>30) HistoryBox->RemoveChildAt(0);
    HistoryBox->ScrollToEnd();
}
void UDemoControlWidget::ToggleFlightPreview()
{
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It)
    {
        if(It->IsPlaying()) { It->Stop(); Frame(); }
        else
        {
            if(auto* P=Cast<ADemoPlayerController>(GetOwningPlayer())) P->CancelUnitDrag();
            if(!It->Start(Manager())) AddMessage(TEXT("提示"),It->GetLastError());
            else AddMessage(TEXT("提示"),TEXT("正在预览轨迹，镜头跟随所选单位的编组；可在单位列表切换。停止后回到编辑位置，保存方案仍保存编辑状态。"));
        }
        return;
    }
}
void UDemoControlWidget::Send()
{
    if(ActiveRequest.IsValid()) return;
    const FString User=Input->GetText().ToString().TrimStartAndEnd();
    if(User.IsEmpty()) return;
    if(User.Len()>4000) { AddMessage(TEXT("提示"),TEXT("单条输入最多4000字。")); return; }
    FFormationCommandResult Formation;
    auto* SceneManager=Manager();
    FHighlightCommandResult Highlight;
    if(FHighlightCommands::Handle(User,SceneManager?SceneManager->SnapshotJson():FString(),Units?Units->GetSelectedOption():FString(),Highlight))
    {
        Cancel(); AddMessage(TEXT("你"),User); Input->SetText(FText::GetEmpty()); AddMessage(TEXT("助手"),Highlight.Reply);
        if(!Highlight.Scenario.IsEmpty() && SceneManager)
        {
            Candidate=Highlight.Scenario; bHighlightCandidate=true;
            CandidateBaseScenario=SceneManager->GetScenarioJson(); CandidateRevision=SceneManager->GetRevision(); ObservedRevision=CandidateRevision;
            ApplyButton->SetIsEnabled(true); CandidateInfo->SetText(FText::FromString(TEXT("飞机高亮候选已就绪，点击应用。")));
        }
        return;
    }
    if(FFormationCommands::Handle(User,SceneManager?SceneManager->SnapshotJson():TEXT("{\"scenarioName\":\"Empty\",\"units\":[]}"),Formation))
    {
        Cancel(); AddMessage(TEXT("你"),User); Input->SetText(FText::GetEmpty()); AddMessage(TEXT("助手"),Formation.Reply);
        if(!Formation.SelectName.IsEmpty() && LayoutWindow)
        { ResourceWindow->SetVisibility(ESlateVisibility::Collapsed); LayoutWindow->SelectFormation(Formation.SelectName); LayoutWindow->SetVisibility(ESlateVisibility::Visible); }
        if(!Formation.Scenario.IsEmpty() && SceneManager)
        {
            Candidate=Formation.Scenario; bFormationCandidate=true;
            bFormationMembershipOnly=Formation.bMembershipOnly;
            CandidateBaseScenario=SceneManager->GetScenarioJson(); CandidateRevision=SceneManager->GetRevision(); ObservedRevision=CandidateRevision;
            ApplyButton->SetIsEnabled(true); CandidateInfo->SetText(FText::FromString(TEXT("编队修改已就绪，点击应用。")));
        }
        return;
    }
    FString Url=Endpoint->GetText().ToString().TrimStartAndEnd();
    FString Name=Model->GetText().ToString().TrimStartAndEnd();
    // This demo is explicitly local: never send scene data to an arbitrary remote host.
    if(!(Url.StartsWith(TEXT("http://127.0.0.1:")) || Url.StartsWith(TEXT("http://localhost:"))) || Name.IsEmpty())
    { AddMessage(TEXT("连接设置"),TEXT("请填写本地 http://127.0.0.1:端口/… 接口及模型名称。")); return; }
    if(User.Len()>4000) { AddMessage(TEXT("提示"),TEXT("单条输入最多4000字。")); return; }
    Candidate.Reset(); bFormationCandidate=false; bFormationMembershipOnly=false; bHighlightCandidate=false; CandidateLevel.Reset(); CandidateBaseScenario.Reset(); ApplyButton->SetIsEnabled(false); CandidateInfo->SetText(FText::FromString(TEXT("正在处理指令…")));
    AddMessage(TEXT("你"),User); Input->SetText(FText::GetEmpty());
    TArray<TSharedPtr<FJsonValue>> PayloadMessages;
    auto Add=[&](const FString& Role,const FString& Content)
    { auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("role"),Role); O->SetStringField(TEXT("content"),Content); PayloadMessages.Add(MakeShared<FJsonValueObject>(O)); };
    PendingScenario=Manager()?Manager()->GetScenarioJson():TEXT("{\"scenarioName\":\"Empty\",\"units\":[]}");
    PendingRevision=Manager()?Manager()->GetRevision():0;
    ObservedRevision=PendingRevision;
    const bool SceneRequest=FDemoChatProtocol::IsSceneRequest(User);
    bPendingSceneRequest=SceneRequest;
    const bool bModelRequest=User.Contains(TEXT("模型")) || User.Contains(TEXT("model"), ESearchCase::IgnoreCase) || User.Contains(TEXT("替换")) || User.Contains(TEXT("换成")) || User.Contains(TEXT("更换"));
    const FString ModelCatalog = bModelRequest ? FEquipmentLibrary::ModelCatalog(FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")) : FString();
    const bool bTextureRequest=User.Contains(TEXT("贴图")) || User.Contains(TEXT("涂装"));
    const FString TextureCatalog=bTextureRequest?FEquipmentLibrary::TextureCatalog(FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")):FString();
    Add(TEXT("system"),FDemoChatProtocol::SystemPrompt(User,ModelCatalog,TextureCatalog)+(SceneRequest?FString():TEXT("\n当前场景配置：")+PendingScenario));
    // A small local model copies old JSON values when extraction includes full scene history.
    // C++ merges updates into the captured scene after validating the model's parameters.
    if(!SceneRequest) for(const auto& M:Messages) Add(M.Key,M.Value);
    Add(TEXT("user"),User);
    Messages.Emplace(TEXT("user"),User);
    auto Body=MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("model"),Name); Body->SetArrayField(TEXT("messages"),PayloadMessages);
    Body->SetBoolField(TEXT("stream"),false); Body->SetNumberField(TEXT("temperature"),0); Body->SetNumberField(TEXT("max_tokens"),1024);
    FString Json; FJsonSerializer::Serialize(Body,TJsonWriterFactory<>::Create(&Json));
    ActiveRequest=FHttpModule::Get().CreateRequest();
    ActiveRequest->SetURL(Url); ActiveRequest->SetVerb(TEXT("POST"));
    ActiveRequest->SetHeader(TEXT("Content-Type"),TEXT("application/json"));
    ActiveRequest->SetContentAsString(Json); ActiveRequest->SetTimeout(300.f);
    ActiveRequest->OnProcessRequestComplete().BindUObject(this,&UDemoControlWidget::FinishRequest);
    SendButton->SetIsEnabled(false);
    if(!ActiveRequest->ProcessRequest()) { Cancel(); AddMessage(TEXT("错误"),TEXT("请求未能启动。")); }
}
void UDemoControlWidget::FinishRequest(FHttpRequestPtr Request,FHttpResponsePtr Response,bool Success)
{
    if(Request!=ActiveRequest) return;
    ActiveRequest.Reset(); SendButton->SetIsEnabled(true);
    CandidateInfo->SetText(FText::FromString(TEXT("回复已完成。")));
    auto Fail=[&](const FString& Message)
    { if(Messages.Num() && Messages.Last().Key==TEXT("user")) Messages.Pop(); CandidateInfo->SetText(FText::FromString(TEXT("未生成可应用的修改。"))); AddMessage(TEXT("错误"),Message); };
    if(!Success || !Response.IsValid()) { Fail(TEXT("连接失败或超时，请检查本地 Qwen 是否启动及接口地址。")); return; }
    if(Response->GetResponseCode()<200 || Response->GetResponseCode()>=300)
    { Fail(FString::Printf(TEXT("接口返回 HTTP %d。请检查模型名称和服务日志。"),Response->GetResponseCode())); return; }
    if(Response->GetContentLength()>1024*1024) { Fail(TEXT("响应过大，已拒绝。")); return; }
    TSharedPtr<FJsonObject> Root; const TArray<TSharedPtr<FJsonValue>>* Choices=nullptr;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()),Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("choices"),Choices) || Choices->IsEmpty() || (*Choices)[0]->Type!=EJson::Object)
    { Fail(TEXT("接口未返回有效的 OpenAI 兼容响应。")); return; }
    const TSharedPtr<FJsonObject>* Message=nullptr;
    FString Content;
    if(!(*Choices)[0]->AsObject()->TryGetObjectField(TEXT("message"),Message) || !(*Message)->TryGetStringField(TEXT("content"),Content))
    { Fail(TEXT("响应缺少 message.content。")); return; }
    FDemoChatReply Reply; FString Error;
    const FString UserInstruction = Messages.Num() && Messages.Last().Key==TEXT("user") ? Messages.Last().Value : FString();
    if(!FDemoChatProtocol::Parse(Content,Reply,Error,PendingScenario,UserInstruction))
    { UE_LOG(LogTemp,Warning,TEXT("[TacticalDemo] Qwen parse failed: %s | raw=%s"),*Error,*Content.Left(1000)); Fail(Error); return; }
    if(!bPendingSceneRequest && (!Reply.Scenario.IsEmpty() || !Reply.LevelId.IsEmpty()))
    { Fail(TEXT("普通对话不能生成场景配置，请发送明确的创建或修改指令。")); return; }
    if((!Reply.Scenario.IsEmpty() || !Reply.LevelId.IsEmpty()) && (!Manager() || Manager()->GetScenarioJson()!=PendingScenario || Manager()->GetRevision()!=PendingRevision))
    { Fail(TEXT("等待回复期间场景已变化，请重新发送指令。")); return; }
    if(!Reply.Scenario.IsEmpty() && !FEquipmentLibrary::ValidateModelChanges(PendingScenario,Reply.Scenario,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"),Error))
    { Fail(TEXT("模型候选校验失败：")+Error); return; }
    Messages.Emplace(TEXT("assistant"),Content);
    while(Messages.Num()>12) Messages.RemoveAt(0,2);
    AddMessage(TEXT("Qwen"),Reply.Reply);
    Candidate=Reply.Scenario;
    CandidateLevel=Reply.LevelId;
    CandidateBaseScenario=Candidate.IsEmpty() && CandidateLevel.IsEmpty()?FString():PendingScenario;
    CandidateRevision=PendingRevision;
    ApplyButton->SetIsEnabled(!Candidate.IsEmpty() || !CandidateLevel.IsEmpty());
    if(!CandidateLevel.IsEmpty()) CandidateInfo->SetText(FText::FromString(TEXT("场景切换已就绪，点击应用。")));
    if(!Candidate.IsEmpty())
    {
        CandidateInfo->SetText(FText::FromString(TEXT("修改已就绪，点击应用。")));
    }
}
void UDemoControlWidget::Cancel()
{
    bFormationCandidate=false;
    bFormationMembershipOnly=false;
    bHighlightCandidate=false;
    CandidateLevel.Reset();
    Candidate.Reset();
    bPendingSceneRequest=false;
    CandidateBaseScenario.Reset(); PendingScenario.Reset();
    if(ApplyButton) ApplyButton->SetIsEnabled(false);
    if(ActiveRequest.IsValid())
    {
        ActiveRequest->OnProcessRequestComplete().Unbind(); ActiveRequest->CancelRequest(); ActiveRequest.Reset();
        if(Messages.Num() && Messages.Last().Key==TEXT("user")) Messages.Pop();
        AddMessage(TEXT("提示"),TEXT("已取消请求。"));
    }
    if(SendButton) SendButton->SetIsEnabled(true);
    if(CandidateInfo) CandidateInfo->SetText(FText::FromString(TEXT("暂无新的场景修改。")));
}
void UDemoControlWidget::NativeDestruct() { Cancel(); Super::NativeDestruct(); }
void UDemoControlWidget::Apply()
{
    if(!CandidateBaseScenario.IsEmpty() && (!Manager() || Manager()->GetRevision()!=CandidateRevision || Manager()->GetScenarioJson()!=CandidateBaseScenario))
    { Cancel(); AddMessage(TEXT("提示"),TEXT("场景已变化，旧候选已取消。请重新发送指令。")); return; }
    if(!CandidateLevel.IsEmpty())
    {
        FString Asset,Error;
        if(!FDemoSceneLibrary::Resolve(CandidateLevel,Asset,Error)) { AddMessage(TEXT("错误"),Error); return; }
        Cancel();
        UGameplayStatics::OpenLevel(this,FName(*Asset),true,TEXT("game=/Script/TacticalDemo.DemoGameMode"));
        return;
    }
    auto* M=Manager(); if(!M || Candidate.IsEmpty()) return;
    if(!CandidateBaseScenario.IsEmpty() && M->GetScenarioJson()!=CandidateBaseScenario)
    { Cancel(); AddMessage(TEXT("提示"),TEXT("场景已变化，旧候选已取消。请重新发送指令。")); return; }
    const FString Path=FPaths::ProjectSavedDir()/TEXT("Chat/GeneratedScenario.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    if(!FFileHelper::SaveStringToFile(Candidate,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { AddMessage(TEXT("错误"),TEXT("无法保存候选配置。")); return; }
    const bool Applied=bHighlightCandidate?M->ApplyHighlightCandidate(Candidate):bFormationCandidate?M->ApplyFormationCandidate(Candidate,bFormationMembershipOnly):M->LoadChatCandidate(Path,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"));
    if(Applied)
    { Candidate.Reset(); bFormationCandidate=false; bFormationMembershipOnly=false; bHighlightCandidate=false; ApplyButton->SetIsEnabled(false); CandidateInfo->SetText(FText::FromString(TEXT("已应用到场景。"))); AddMessage(TEXT("场景"),TEXT("已更新场景。")); }
    else AddMessage(TEXT("错误"),M->GetLastError());
}
void UDemoControlWidget::Reload()
{
    Cancel();
    Candidate.Reset(); ApplyButton->SetIsEnabled(false); Messages.Reset();
    if(auto* M=Manager())
    { if(M->ReloadScenario()) { AddMessage(TEXT("场景"),TEXT("已加载原始配置。")); } else AddMessage(TEXT("错误"),M->GetLastError()); }
}
void UDemoControlWidget::Clear()
{
    Cancel();
    Candidate.Reset(); ApplyButton->SetIsEnabled(false); Messages.Reset();
    if(auto* M=Manager()) M->ClearScenario();
    AddMessage(TEXT("场景"),TEXT("已清空。"));
}
void UDemoControlWidget::Frame()
{
    if(auto* Mode=GetWorld()->GetAuthGameMode<ADemoGameMode>()) Mode->FrameScene(Manager());
}
void UDemoControlWidget::ToggleResources()
{
    if(LayoutWindow) LayoutWindow->SetVisibility(ESlateVisibility::Collapsed);
    ResourceWindow->SetVisibility(ResourceWindow->GetVisibility()==ESlateVisibility::Collapsed?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}

void UDemoControlWidget::ToggleLayout()
{
    for(TActorIterator<ADemoFlightPreview> It(GetWorld());It;++It) It->Stop();
    if(auto* P=Cast<ADemoPlayerController>(GetOwningPlayer())) P->CancelUnitDrag();
    ResourceWindow->SetVisibility(ESlateVisibility::Collapsed);
    LayoutWindow->RefreshSelection();
    LayoutWindow->SetVisibility(LayoutWindow->GetVisibility()==ESlateVisibility::Collapsed?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UDemoControlWidget::UnitSelectionChanged(FString Id,ESelectInfo::Type)
{ if(!bRefreshingUnits) if(auto* P=Cast<ADemoPlayerController>(GetOwningPlayer())) P->SelectUnit(Id); }
void UDemoControlWidget::SaveResources()
{
    auto Root=MakeShared<FJsonObject>();
    const TCHAR* Keys[]={TEXT("scene"),TEXT("livery"),TEXT("models"),TEXT("trajectory")};
    for(int32 Index=0;Index<4;++Index)
        Root->SetStringField(Keys[Index],ResourcePaths[Index]->GetText().ToString().TrimStartAndEnd());
    FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    const FString Path=FPaths::ProjectSavedDir()/TEXT("ResourceSources.json");
    IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(),true);
    ResourceStatus->SetText(FText::FromString(FFileHelper::SaveStringToFile(Json,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)?TEXT("来源配置已保存，下次运行自动恢复。保存路径不会自动导入资源。"):TEXT("保存失败，请检查目录权限。")));
}
void UDemoControlWidget::LoadResources()
{
    FString Json;
    const FString Path=FPaths::ProjectSavedDir()/TEXT("ResourceSources.json");
    if(!FFileHelper::LoadFileToString(Json,*Path))
    {
        ResourcePaths[0]->SetText(FText::FromString(FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir()/TEXT("Scenario.json"))));
        ResourceStatus->SetText(FText::FromString(TEXT("尚无已保存配置。可填写未来资源的来源路径。")));
        return;
    }
    TSharedPtr<FJsonObject> Root;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root) || !Root.IsValid())
    { ResourceStatus->SetText(FText::FromString(TEXT("已保存配置格式错误，当前输入已保留。"))); return; }
    const TCHAR* Keys[]={TEXT("scene"),TEXT("livery"),TEXT("models"),TEXT("trajectory")};
    TArray<FString> Values;
    for(const TCHAR* Key:Keys)
    {
        FString Value;
        if(!Root->TryGetStringField(Key,Value))
        { ResourceStatus->SetText(FText::FromString(TEXT("已保存配置缺少有效路径字段，当前输入已保留。"))); return; }
        Values.Add(Value);
    }
    for(int32 Index=0;Index<4;++Index) ResourcePaths[Index]->SetText(FText::FromString(Values[Index]));
    ResourceStatus->SetText(FText::FromString(TEXT("已恢复四类资源来源配置。")));
}
void UDemoControlWidget::LoadResourceScene()
{
    auto* M=Manager(); if(!M) return;
    const FString Path=ResourcePaths[0]->GetText().ToString().TrimStartAndEnd();
    if(!Path.EndsWith(TEXT(".json"),ESearchCase::IgnoreCase) || !IFileManager::Get().FileExists(*Path))
    { ResourceStatus->SetText(FText::FromString(TEXT("请填写存在的场景 JSON 文件。"))); return; }
    Cancel();
    if(M->LoadChatCandidate(Path,FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json")))
    { Messages.Reset(); ResourceStatus->SetText(FText::FromString(TEXT("场景配置已校验并加载。"))); Frame(); }
    else ResourceStatus->SetText(FText::FromString(M->GetLastError()));
}
