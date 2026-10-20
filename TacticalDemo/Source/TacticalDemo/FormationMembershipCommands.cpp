#include "FormationCommands.h"
#include "ScenarioConfigLoader.h"
#include "Internationalization/Regex.h"

namespace
{
bool Match(const FString& Expression,const FString& Text,TArray<FString>& Captures,int32 GroupCount=3)
{
    FRegexMatcher M(FRegexPattern(Expression),Text);
    if(!M.FindNext()) return false;
    Captures.Reset();
    for(int32 I=1;I<=GroupCount;++I) Captures.Add(M.GetCaptureGroup(I));
    for(auto& S:Captures) S.TrimStartAndEndInline();
    return true;
}
bool ValidName(const FString& Name)
{
    FRegexMatcher M(FRegexPattern(TEXT("^[\\p{L}\\p{N}_-]{1,32}$")),Name);
    if(!M.FindNext()) return false;
    for(const TCHAR* Word:{TEXT("然后"),TEXT("同时"),TEXT("并"),TEXT("不要"),TEXT("删除"),TEXT("排成"),TEXT("移动"),TEXT("加入"),TEXT("移出"),TEXT("改名"),TEXT("解散")})
        if(Name.Contains(Word)) return false;
    return true;
}
FString ModelKey(FString S)
{
    S.ReplaceInline(TEXT("-"),TEXT("")); return S.ToUpper();
}
void ExactMembers(const FString& Token,const FDemoScenarioConfig& Config,TSet<FString>& Matches)
{
    for(const auto& G:Config.Units) if(G.Type==TEXT("aircraft")) for(const auto& I:G.Instances)
        if(Token==I.Id || Token==G.Id || (!I.Formation.IsEmpty() && (Token==I.Formation+TEXT("成员") || Token==I.Formation+TEXT("全部成员")))) Matches.Add(I.Id);
}
bool SelectAtom(FString Token,const FDemoScenarioConfig& Config,TSet<FString>& Matches)
{
    Token.TrimStartAndEndInline();
    if(Token.IsEmpty()) return false;
    ExactMembers(Token,Config,Matches);
    if(!Matches.IsEmpty()) return true;
    for(const TCHAR* Prefix:{TEXT("当前场景中的"),TEXT("当前场景中"),TEXT("当前的"),TEXT("当前"),TEXT("所有的"),TEXT("所有"),TEXT("全部的"),TEXT("全部")})
        Token.RemoveFromStart(Prefix);
    Token.TrimStartAndEndInline();
    ExactMembers(Token,Config,Matches);
    if(!Matches.IsEmpty()) return true;
    FString Filter=Token;
    Filter.RemoveFromEnd(TEXT("飞机")); Filter.RemoveFromEnd(TEXT("的"));
    FString Side;
    if(Filter.RemoveFromStart(TEXT("红方"))) Side=TEXT("red");
    else if(Filter.RemoveFromStart(TEXT("蓝方"))) Side=TEXT("blue");
    Filter.RemoveFromStart(TEXT("的")); Filter.TrimStartAndEndInline();
    const bool All=Token==TEXT("飞机");
    if(!All && Side.IsEmpty() && Filter.IsEmpty()) return false;
    for(const auto& G:Config.Units)
        if(G.Type==TEXT("aircraft") && (Side.IsEmpty() || G.Side==Side) && (All || Filter.IsEmpty() || ModelKey(Filter)==ModelKey(G.ModelId)))
            for(const auto& I:G.Instances) Matches.Add(I.Id);
    return !Matches.IsEmpty();
}
// Resolve full known names before treating characters such as 和/与 as list separators.
// Select only existing aircraft. A mixed list is atomic: every token must resolve.
bool SelectMembers(const FString& Selector,const FDemoScenarioConfig& Config,TSet<FString>& Selected)
{
    TArray<FString> Atoms;
    for(const auto& G:Config.Units) if(G.Type==TEXT("aircraft"))
    {
        Atoms.AddUnique(G.Id);
        for(const auto& I:G.Instances)
        {
            Atoms.AddUnique(I.Id);
            if(!I.Formation.IsEmpty()) { Atoms.AddUnique(I.Formation+TEXT("成员")); Atoms.AddUnique(I.Formation+TEXT("全部成员")); }
        }
    }
    TArray<FString> Tokens; int32 Start=0;
    for(int32 Offset=0;Offset<Selector.Len();)
    {
        int32 Longest=0;
        for(const auto& Atom:Atoms) if(Atom.Len()>Longest && Selector.Mid(Offset,Atom.Len())==Atom) Longest=Atom.Len();
        if(Longest) { Offset+=Longest; continue; }
        if(FString(TEXT("、，,和与")).Contains(Selector.Mid(Offset,1)))
        { Tokens.Add(Selector.Mid(Start,Offset-Start)); Start=Offset+1; }
        ++Offset;
    }
    Tokens.Add(Selector.Mid(Start));
    for(const FString& Token:Tokens)
    {
        TSet<FString> Matches;
        if(!SelectAtom(Token,Config,Matches)) return false;
        Selected.Append(Matches);
    }
    return Selected.Num()>0;
}
}

bool FFormationCommands::HandleMembership(const FString& User,FDemoScenarioConfig Config,FFormationCommandResult& Out)
{
    FString Text=User.TrimStartAndEnd();
    Text.RemoveFromStart(TEXT("请")); Text.TrimStartAndEndInline();
    while(Text.EndsWith(TEXT("。")) || Text.EndsWith(TEXT("！")) || Text.EndsWith(TEXT("!"))) Text.LeftChopInline(1);
    if(Text==TEXT("编队帮助") || Text==TEXT("编队指令") || Text==TEXT("如何创建编队") || Text==TEXT("怎么创建编队"))
    {
        Out.Reply=TEXT("编队指令直接在本地处理，无需等待Qwen。每次一个操作，生成候选后点击“应用到场景”；在“单位编辑与方案管理”中可撤销。\n创建：把红方飞机编成编队3\n加入：把蓝方飞机加入编队3\n指定成员：把飞机编号1、飞机编号2编成护航编队（用实际编号替换）\n移出：把飞机编号移出编队3\n改名：把编队3改名为护航编队\n解散：解散护航编队（保留飞机）\n排列：把编队3排成V形，间距50米\n查询：查看编队列表；查看编队3成员；查看飞机编号\n也可按机型选择，如F35飞机、J20_Map3飞机。创建和管理只改变成员归属，飞机保持原位置。使用“保存为新方案”可保存编队。");
        return true;
    }
    if(Text==TEXT("查看飞机编号") || Text==TEXT("列出飞机编号"))
    {
        Out.Reply=TEXT("当前飞机（编号 / 机型 / 阵营 / 编队）："); int32 Count=0;
        for(const auto& G:Config.Units) if(G.Type==TEXT("aircraft")) for(const auto& I:G.Instances)
        {
            if(Count++<40) Out.Reply+=FString::Printf(TEXT("\n%s / %s / %s / %s"),*I.Id,*G.ModelId,G.Side==TEXT("red")?TEXT("红方"):TEXT("蓝方"),I.Formation.IsEmpty()?TEXT("未编队"):*I.Formation);
        }
        if(!Count) Out.Reply+=TEXT("暂无飞机。");
        if(Count>40) Out.Reply+=TEXT("\n更多飞机请在单位编辑窗口查看。");
        return true;
    }
    TSet<FString> Names;
    for(const auto& G:Config.Units) for(const auto& I:G.Instances) if(!I.Formation.IsEmpty()) Names.Add(I.Formation);
    bool Mention=Text.Contains(TEXT("编队"));
    for(const auto& N:Names) Mention|=Text.Contains(N);
    Mention|=Text.StartsWith(TEXT("解散")) || Text.Contains(TEXT("改名")) || Text.Contains(TEXT("重命名"))
        || ((Text.StartsWith(TEXT("把")) || Text.StartsWith(TEXT("将"))) && (Text.Contains(TEXT("加入")) || Text.Contains(TEXT("移出"))));
    bool Action=false;
    for(const TCHAR* Word:{TEXT("编成"),TEXT("组成"),TEXT("组建"),TEXT("创建"),TEXT("加入"),TEXT("移出"),TEXT("解散"),TEXT("改名"),TEXT("重命名")}) Action|=Text.Contains(Word);
    if(!Action || (!Mention && !Text.Contains(TEXT("编成")))) return false;
    auto Fail=[&](const FString& Message){Out.Reply=Message; return true;};
    for(const TCHAR* Word:{TEXT("不要"),TEXT("别把"),TEXT("然后"),TEXT("同时"),TEXT("并"),TEXT("如果"),TEXT("是否"),TEXT("？"),TEXT("?"),TEXT("\n"),TEXT("\r")})
        if(Text.Contains(Word)) return Fail(TEXT("未生成修改。请直接描述一个编队操作，例如：把红方飞机编成编队3。输入“编队帮助”查看示例。"));
    enum class EOperation { Create, Add, Remove, Rename, Dissolve };
    EOperation Op; FString Name,Selector,NewName; TArray<FString> C;
    if(Match(TEXT("^(?:把|将)?(.+?)(?:改名为|改名成|重命名为)(.+)$"),Text,C,2))
    { Op=EOperation::Rename; Name=C[0]; NewName=C[1]; }
    else if(Match(TEXT("^解散(.+)$"),Text,C,1)) { Op=EOperation::Dissolve; Name=C[0]; }
    else if(Match(TEXT("^(?:创建|组建)(.+?)[，,]\\s*成员(?:是|为)(.+)$"),Text,C,2))
    { Op=EOperation::Create; Name=C[0]; Selector=C[1]; }
    else if(Match(TEXT("^(?:把|将)?(.+?)从(.+?)(?:中)?移出$"),Text,C,2))
    { Op=EOperation::Remove; Selector=C[0]; Name=C[1]; }
    else if(Match(TEXT("^(?:把|将)?(.+?)(编成|组成|组建为|加入|移出)(.+)$"),Text,C))
    {
        Selector=C[0]; Name=C[2];
        Op=C[1]==TEXT("加入")?EOperation::Add:C[1]==TEXT("移出")?EOperation::Remove:EOperation::Create;
    }
    else return Fail(TEXT("请说明成员和编队名称，例如：把红方飞机编成编队3；把蓝方飞机加入编队3。输入“编队帮助”查看全部指令。"));
    if(!ValidName(Name) || (Op==EOperation::Rename && !ValidName(NewName)))
        return Fail(TEXT("编队名请使用1至32个中文、字母、数字、下划线或连字符；每次只发送一个操作。"));
    if(Op==EOperation::Create && Names.Contains(Name)) return Fail(Name+TEXT("已存在。添加成员请输入“加入”；创建新编队请换一个名称。"));
    if(Op!=EOperation::Create && !Names.Contains(Name)) return Fail(TEXT("当前没有")+Name+TEXT("。可输入“查看编队列表”。"));
    if(Op==EOperation::Rename && Names.Contains(NewName)) return Fail(TEXT("新名称已存在，请使用其他名称，或通过“加入”合并成员。"));
    TSet<FString> Members;
    if(Op==EOperation::Create || Op==EOperation::Add || Op==EOperation::Remove)
        if(!SelectMembers(Selector,Config,Members)) return Fail(TEXT("无法完整确定成员，未生成修改。请使用红方飞机、蓝方飞机、准确机型或实际飞机编号；可输入“查看飞机编号”。"));
    int32 Changed=0,Transferred=0; FString Ids;
    for(auto& G:Config.Units) for(auto& I:G.Instances)
    {
        FString Next=I.Formation;
        if((Op==EOperation::Create || Op==EOperation::Add) && Members.Contains(I.Id)) Next=Name;
        else if(Op==EOperation::Remove && Members.Contains(I.Id) && I.Formation==Name) Next.Reset();
        else if(Op==EOperation::Dissolve && I.Formation==Name) Next.Reset();
        else if(Op==EOperation::Rename && I.Formation==Name) Next=NewName;
        if(Next==I.Formation) continue;
        if((Op==EOperation::Create || Op==EOperation::Add) && !I.Formation.IsEmpty()) ++Transferred;
        I.Formation=Next;
        if(Changed++<12) { if(!Ids.IsEmpty()) Ids+=TEXT("、"); Ids+=I.Id; }
    }
    if(!Changed) return Fail(TEXT("成员归属无需变化，未生成候选。移出操作只影响该编队内的成员。"));
    FDemoScenarioConfig Checked; FString Error;
    if(!FScenarioConfigLoader::Parse(FScenarioConfigLoader::Serialize(Config),Checked,Error)) return Fail(TEXT("编队数据无效：")+Error);
    const TCHAR* Verb=Op==EOperation::Create?TEXT("创建"):Op==EOperation::Add?TEXT("加入成员到"):Op==EOperation::Remove?TEXT("移出成员自"):Op==EOperation::Rename?TEXT("重命名"):TEXT("解散");
    Out.Reply=FString::Printf(TEXT("已生成候选：%s%s%s，涉及%d架飞机。\n成员：%s%s。"),Verb,*Name,Op==EOperation::Rename?*(TEXT(" → ")+NewName):TEXT(""),Changed,*Ids,Changed>12?TEXT("等"):TEXT(""));
    if(Transferred) Out.Reply+=FString::Printf(TEXT("\n其中%d架将从原编队转入；每架飞机只属于一个编队。"),Transferred);
    Out.Reply+=TEXT("\n点击“应用到场景”后生效，可撤销。飞机数量、位置、朝向和机型保持不变。");
    Out.Scenario=FScenarioConfigLoader::Serialize(Checked); Out.bMembershipOnly=true;
    return true;
}
