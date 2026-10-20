#include "FormationCommands.h"
#include "ScenarioConfigLoader.h"
#include "Internationalization/Regex.h"
#include "Misc/DefaultValueHelper.h"

bool FFormationCommands::IsNameMention(const FString& Text,int32 Offset,const FString& Name)
{
    if(Name.IsEmpty() || Offset<0 || Text.Mid(Offset,Name.Len())!=Name) return false;
    const FString Head=Text.Left(Offset).TrimEnd(),Tail=Text.Mid(Offset+Name.Len()).TrimStart();
    bool Start=Head.IsEmpty(),End=Tail.IsEmpty();
    for(const TCHAR* Token:{TEXT("把"),TEXT("将"),TEXT("让"),TEXT("给"),TEXT("按"),TEXT("调用"),TEXT("查看"),TEXT("选择"),TEXT("和"),TEXT("与"),TEXT("当前"),TEXT("现有"),TEXT("“"),TEXT("‘"),TEXT("\""),TEXT("'"),TEXT("，"),TEXT(","),TEXT("、"),TEXT("解散"),TEXT("创建"),TEXT("组建"),TEXT("编成"),TEXT("组成"),TEXT("组建为"),TEXT("加入"),TEXT("移出"),TEXT("从"),TEXT("改名为"),TEXT("改名成"),TEXT("重命名为")}) Start|=Head.EndsWith(Token);
    for(const TCHAR* Token:{TEXT("？"),TEXT("?"),TEXT("！"),TEXT("!"),TEXT("”"),TEXT("’"),TEXT("\""),TEXT("'"),TEXT("，"),TEXT(","),TEXT("。"),TEXT("、"),TEXT("；"),TEXT(";"),TEXT("和"),TEXT("与"),TEXT("的"),TEXT("整体"),TEXT("排成"),TEXT("排列"),TEXT("移动"),TEXT("位置"),TEXT("高度"),TEXT("朝向"),TEXT("方向"),TEXT("成员"),TEXT("多少"),TEXT("有哪些"),TEXT("有多少"),TEXT("在当前位置"),TEXT("全部成员"),TEXT("改名"),TEXT("重命名"),TEXT("中移出"),TEXT("移出")}) End|=Tail.StartsWith(Token);
    return Start && End;
}

bool FFormationCommands::Arrange(FDemoScenarioConfig& Config,const FString& Name,int32 Pattern,double Spacing,double Heading,const FVector2D& Anchor,FString& Error)
{
    Error.Reset();
    if(Name.TrimStartAndEnd().IsEmpty() || Pattern<0 || Pattern>2 || !FMath::IsFinite(Spacing) || Spacing<=0 || Spacing>10000
        || !FMath::IsFinite(Heading) || FMath::Abs(Heading)>360000 || Anchor.ContainsNaN())
    { Error=TEXT("队形参数无效；间距需要大于0且不超过10000米。"); return false; }
    auto Next=Config;
    const double Angle=FMath::DegreesToRadians(FRotator::NormalizeAxis(Heading));
    const FVector Forward(FMath::Cos(Angle),FMath::Sin(Angle),0),Right(-FMath::Sin(Angle),FMath::Cos(Angle),0);
    int32 Index=0;
    for(auto& G:Next.Units) for(auto& I:G.Instances) if(I.Formation==Name.TrimStartAndEnd())
    {
        double X=0,Y=0;
        if(Pattern==0) Y=Index*Spacing;
        else if(Pattern==1) X=-Index*Spacing;
        else if(Index>0) { const int32 Rank=(Index+1)/2; X=-Rank*Spacing; Y=(Index%2? -1:1)*Rank*Spacing; }
        I.PositionMeters=FVector(Anchor.X,Anchor.Y,I.PositionMeters.Z)+Forward*X+Right*Y;
        I.Yaw=FRotator::NormalizeAxis(Heading); ++Index;
    }
    if(!Index) { Error=TEXT("该编队没有成员，请先选择飞机并保存成员。"); return false; }
    for(auto& G:Next.Units) if(G.Instances.Num()) G.PositionMeters=G.Instances[0].PositionMeters;
    FDemoScenarioConfig Checked;
    if(!FScenarioConfigLoader::Parse(FScenarioConfigLoader::Serialize(Next),Checked,Error)) return false;
    Config=MoveTemp(Checked); return true;
}

bool FFormationCommands::Handle(const FString& User,const FString& Current,FFormationCommandResult& Out)
{
    Out=FFormationCommandResult();
    FDemoScenarioConfig Config; FString Error;
    if(!FScenarioConfigLoader::Parse(Current,Config,Error)) return false;
    if(HandleMembership(User,Config,Out)) return true;
    TArray<FString> Names; TMap<FString,int32> Counts;
    for(const auto& G:Config.Units) for(const auto& I:G.Instances) if(!I.Formation.IsEmpty())
    { Names.AddUnique(I.Formation); ++Counts.FindOrAdd(I.Formation); }
    FString Text=User;
    FRegexMatcher Numbers(FRegexPattern(TEXT("编队\\s*([0-9]+)")),User);
    TArray<FString> NumberNames;
    while(Numbers.FindNext())
    {
        const FString Name=TEXT("编队")+Numbers.GetCaptureGroup(1); NumberNames.AddUnique(Name);
        Text.ReplaceInline(*Numbers.GetCaptureGroup(0),*Name);
    }
    TArray<FString> Matched;
    const bool RawNameMention=Names.ContainsByPredicate([&](const FString& Name){return Text.Contains(Name);});
    // Match the longest name at each occurrence; separate mentions still count separately.
    for(int32 Offset=0;Offset<Text.Len();)
    {
        FString Longest;
        for(const FString& Name:Names)
            if(Name.Len()>Longest.Len() && Text.Mid(Offset,Name.Len())==Name) Longest=Name;
        if(Longest.IsEmpty()) ++Offset;
        else
        {
            if(IsNameMention(Text,Offset,Longest)) Matched.AddUnique(Longest);
            Offset+=Longest.Len();
        }
    }
    const bool Mention=Text.Contains(TEXT("编队")) || RawNameMention;
    const bool Action=Text.Contains(TEXT("调用")) || Text.Contains(TEXT("查看")) || Text.Contains(TEXT("选择"))
        || Text.Contains(TEXT("排成")) || Text.Contains(TEXT("排列")) || Text.Contains(TEXT("移动"))
        || Text.Contains(TEXT("位置")) || Text.Contains(TEXT("有哪些")) || Text.Contains(TEXT("列出")) || Text.Contains(TEXT("多少")) || Text.Contains(TEXT("列表"))
        || Text.Contains(TEXT("高度")) || Text.Contains(TEXT("朝向")) || Text.Contains(TEXT("方向")) || Text.Contains(TEXT("成员"));
    if(!Mention || !Action) return false;
    auto Fail=[&](const FString& S) { Out.Reply=S; Out.Scenario.Reset(); Out.SelectName.Reset(); return true; };
    FString List; for(const auto& Name:Names) { if(!List.IsEmpty()) List+=TEXT("、"); List+=FString::Printf(TEXT("%s（%d架）"),*Name,Counts[Name]); }
    if(List.IsEmpty()) return Fail(TEXT("当前场景还没有编队。可输入：把红方飞机编成编队3。点击应用后，再调整队形；输入“编队帮助”查看示例。"));
    if(NumberNames.Num()>1) return Fail(TEXT("每次只操作一个编队，请分别发送指令。"));
    for(const auto& NumberName:NumberNames)
        if(!Matched.ContainsByPredicate([&](const FString& N){return N.Contains(NumberName);}))
            return Fail(TEXT("当前场景没有 ")+NumberName+TEXT("。现有编队：")+List);
    if(Matched.Num()>1) return Fail(TEXT("每次只操作一个编队，请分别发送指令。"));
    if(Matched.IsEmpty())
    {
        if(Text.Contains(TEXT("有哪些")) || Text.Contains(TEXT("列出")) || Text.Contains(TEXT("多少")) || Text.Contains(TEXT("列表"))) { Out.Reply=TEXT("当前编队：")+List; return true; }
        return Fail(TEXT("请指定当前场景中已有的编队名称：")+List);
    }
    const FString Name=Matched[0]; FString Parameters=Text.Replace(*Name,TEXT(""));
    Parameters.ReplaceInline(TEXT("横队"),TEXT("横排"));
    Parameters.ReplaceInline(TEXT("纵队"),TEXT("纵列"));
    Parameters.ReplaceInline(TEXT("V字形"),TEXT("楔形"));
    Parameters.ReplaceInline(TEXT("V形"),TEXT("楔形"));
    Parameters.ReplaceInline(TEXT("人字形"),TEXT("楔形"));
    for(const TCHAR* Unsupported:{TEXT("换成"),TEXT("替换"),TEXT("贴图"),TEXT("型号"),TEXT("数量"),TEXT("阵营"),TEXT("删除"),TEXT("清空"),TEXT("创建"),TEXT("航迹"),TEXT("切换"),TEXT("同时"),TEXT("然后")})
        if(Parameters.Contains(Unsupported)) return Fail(TEXT("编队指令支持查看、调用、排列、整体移动、高度及朝向；请一次发送一个操作。"));
    const TCHAR* PatternNames[]={TEXT("横排"),TEXT("纵列"),TEXT("楔形")}; int32 Pattern=-1;
    for(int32 I=0;I<3;++I) if(Parameters.Contains(PatternNames[I]))
    { if(Pattern>=0) return Fail(TEXT("一次只能指定一种队形。")); Pattern=I; }
    const bool Move=Parameters.Contains(TEXT("移动")) || (Pattern<0 && Parameters.Contains(TEXT("位置")));
    const bool Height=Parameters.Contains(TEXT("高度")) && !Parameters.Contains(TEXT("保持高度")) && !Parameters.Contains(TEXT("高度不变"));
    const bool Turn=Pattern<0 && (Parameters.Contains(TEXT("朝向")) || Parameters.Contains(TEXT("方向")));
    if(int32(Pattern>=0)+int32(Move)+int32(Height)+int32(Turn)>1) return Fail(TEXT("排列、移动、高度和朝向请分开发送，每次只执行一个操作。"));
    if(Pattern>=0 && Move) return Fail(TEXT("排列和整体移动请分开发送；排列可指定起点X/Y。"));
    const bool Call=Parameters.Contains(TEXT("调用")) || Parameters.Contains(TEXT("查看")) || Parameters.Contains(TEXT("选择"));
    if(Pattern<0 && !Move && !Height && !Turn)
    {
        if(Call || Parameters.Contains(TEXT("多少")) || Parameters.Contains(TEXT("成员")))
        {
            Out.Reply=FString::Printf(TEXT("%s，共%d架。已打开编队窗口，可继续排列或调整成员。"),*Name,Counts[Name]);
            if(Parameters.Contains(TEXT("成员")))
            {
                int32 Shown=0;
                for(const auto& G:Config.Units) for(const auto& I:G.Instances) if(I.Formation==Name && Shown++<12)
                    Out.Reply+=FString::Printf(TEXT("\n%s：%s，%s"),*I.Id,*G.ModelId,*G.Side);
                if(Counts[Name]>12) Out.Reply+=TEXT("\n其余成员请在编队窗口查看。");
            }
            Out.SelectName=Name; return true;
        }
        return Fail(TEXT("可输入：调用")+Name+TEXT("；把")+Name+TEXT("排成楔形，间距35米；把")+Name+TEXT("整体移动到X=550米、Y=-515米。"));
    }
    FDemoUnitInstance Leader; bool Found=false;
    for(const auto& G:Config.Units) for(const auto& I:G.Instances) if(!Found && I.Formation==Name) { Leader=I; Found=true; }
    double X=Leader.PositionMeters.X,Y=Leader.PositionMeters.Y,Spacing=35,Heading=Leader.Yaw,Z=Leader.PositionMeters.Z;
    auto Number=[&](const FString& Label,double& Value,bool& Present)
    {
        Present=Parameters.Contains(Label,ESearchCase::IgnoreCase);
        if(!Present) return true;
        // Count labels, including malformed values, before extracting a number.
        FRegexMatcher Labels(FRegexPattern(TEXT("(?i)")+Label),Parameters);
        int32 Occurrences=0; while(Labels.FindNext()) ++Occurrences;
        if(Occurrences!=1) return false;
        const FString Expression=TEXT("(?i)")+Label+TEXT("\\s*(?:[=：:]|改为|改成|设置为|设为|调整为|为|到)?\\s*([+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?)(?=$|[\\s米度°，,、；;。XxYy])");
        FRegexMatcher Matcher(FRegexPattern(Expression),Parameters);
        if(!Matcher.FindNext() || !FDefaultValueHelper::ParseDouble(Matcher.GetCaptureGroup(1),Value) || !FMath::IsFinite(Value)) return false;
        return !Matcher.FindNext();
    };
    bool HasX=false,HasY=false,HasSpacing=false,HasHeading=false,HasHeight=false;
    if(!Number(TEXT("X"),X,HasX) || !Number(TEXT("Y"),Y,HasY) || !Number(TEXT("间距"),Spacing,HasSpacing))
        return Fail(TEXT("坐标和间距需要完整有效的数字，例如X=550米、Y=-515米、间距35米。"));
    const FString DirectionLabel=Parameters.Contains(TEXT("朝向"))?TEXT("朝向"):TEXT("方向");
    if(Parameters.Contains(TEXT("朝向")) && Parameters.Contains(TEXT("方向"))) return Fail(TEXT("方向和朝向请只指定一项角度。"));
    if(!Number(DirectionLabel,Heading,HasHeading)) return Fail(TEXT("方向需要有效角度，例如方向90度。"));
    if(Parameters.Contains(TEXT("Z"),ESearchCase::IgnoreCase)) return Fail(TEXT("设置整队高度请输入：把编队高度改为30米。"));
    if(Height || Turn)
    {
        if(HasX || HasY || HasSpacing) return Fail(TEXT("高度或朝向操作不能同时指定坐标或间距。"));
        if(Height && (!Number(TEXT("高度"),Z,HasHeight) || !HasHeight)) return Fail(TEXT("高度需要有效数字，例如高度改为30米。"));
        if(Turn && !HasHeading) return Fail(TEXT("朝向需要有效角度，例如朝向改为90度。"));
        for(auto& G:Config.Units) for(auto& I:G.Instances) if(I.Formation==Name)
        { if(Height) I.PositionMeters.Z=Z; else I.Yaw=Heading; }
        for(auto& G:Config.Units) if(G.Instances.Num()) G.PositionMeters=G.Instances[0].PositionMeters;
        FDemoScenarioConfig Checked;
        if(!FScenarioConfigLoader::Parse(FScenarioConfigLoader::Serialize(Config),Checked,Error)) return Fail(TEXT("高度或角度超出有效范围，场景保持不变。"));
        Config=MoveTemp(Checked);
        Out.Reply=Height?FString::Printf(TEXT("已生成%s高度修改：世界Z=%.2f米，保留平面位置和朝向。点击应用。"),*Name,Z)
            :FString::Printf(TEXT("已生成%s朝向修改：%.1f度，保留全部位置。点击应用。"),*Name,FRotator::NormalizeAxis(Heading));
    }
    else if(Move)
    {
        if((!HasX && !HasY) || HasHeading || HasSpacing) return Fail(TEXT("整体移动请指定X/Y坐标；间距和方向通过排列指令设置。"));
        const FVector Delta(X-Leader.PositionMeters.X,Y-Leader.PositionMeters.Y,0);
        for(auto& G:Config.Units) for(auto& I:G.Instances) if(I.Formation==Name) I.PositionMeters+=Delta;
        for(auto& G:Config.Units) if(G.Instances.Num()) G.PositionMeters=G.Instances[0].PositionMeters;
        FDemoScenarioConfig Checked;
        if(!FScenarioConfigLoader::Parse(FScenarioConfigLoader::Serialize(Config),Checked,Error)) return Fail(TEXT("移动超出有效范围，场景保持不变。"));
        Out.Reply=FString::Printf(TEXT("已生成%s整体移动：起点X=%.2f、Y=%.2f米，保留相对位置、高度和朝向。点击应用。"),*Name,X,Y);
    }
    else
    {
        if(!Arrange(Config,Name,Pattern,Spacing,Heading,FVector2D(X,Y),Error)) return Fail(Error);
        Out.Reply=FString::Printf(TEXT("已生成%s的%s排列，共%d架；起点X=%.2f、Y=%.2f米，间距%.2f米，方向%.1f度。点击应用。"),*Name,PatternNames[Pattern],Counts[Name],X,Y,Spacing,Heading);
    }
    Out.Scenario=FScenarioConfigLoader::Serialize(Config); return true;
}
