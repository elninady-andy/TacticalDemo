#include "HighlightCommands.h"
#include "ScenarioConfigLoader.h"
#include "FormationCommands.h"
#include "Internationalization/Regex.h"

bool FHighlightCommands::Handle(const FString& User,const FString& Current,const FString& SelectedId,FHighlightCommandResult& Out)
{
    Out={};
    if(!User.Contains(TEXT("高亮")) && !User.Contains(TEXT("发光"))) return false;
    // Effect words inside an existing or newly requested formation name are names,
    // not effect operations. Probe only; formation handling still owns the candidate.
    FFormationCommandResult Formation;
    if(FFormationCommands::Handle(User,Current,Formation))
    {
        TArray<FString> Names; FDemoScenarioConfig Scene; FString ParseError;
        for(const FString& Json:{Current,Formation.Scenario})
            if(!Json.IsEmpty() && FScenarioConfigLoader::Parse(Json,Scene,ParseError))
                for(const auto& G:Scene.Units) for(const auto& I:G.Instances) if(!I.Formation.IsEmpty()) Names.AddUnique(I.Formation);
        Names.Sort([](const FString& A,const FString& B){return A.Len()>B.Len();});
        FString Operation=User;
        for(const FString& Name:Names)
        {
            if(!Name.Contains(TEXT("高亮")) && !Name.Contains(TEXT("发光"))) continue;
            for(int32 Offset=0;Offset+Name.Len()<=Operation.Len();++Offset)
            {
                if(Operation.Mid(Offset,Name.Len())!=Name) continue;
                if(FFormationCommands::IsNameMention(Operation,Offset,Name))
                { Operation=Operation.Left(Offset)+FString::ChrN(Name.Len(),TEXT('_'))+Operation.Mid(Offset+Name.Len()); Offset+=Name.Len()-1; }
            }
        }
        if(!Operation.Contains(TEXT("高亮")) && !Operation.Contains(TEXT("发光"))) return false;
    }
    auto Fail=[&](const FString& Message){Out.Reply=Message; return true;};
    if(User.TrimStartAndEnd()==TEXT("高亮帮助"))
        return Fail(TEXT("让指定飞机发光高亮：让 RED01_001 发光高亮\n关闭：取消 RED01_001 高亮\n也可输入：让选中的飞机高亮。请先在右侧单位列表选择飞机。\n输入“查看飞机编号”获取本场景实际编号。每次指定一架，收到候选后点击“应用到场景”；可撤销，可随方案保存。固定金色发光，不改变原贴图和阵营。"));
    FString Text=User.TrimStartAndEnd(); Text.RemoveFromStart(TEXT("请")); Text.TrimStartAndEndInline();
    if(Text.EndsWith(TEXT("。"))) Text.LeftChopInline(1);
    FString Target; bool Enabled=true;
    auto Parse=[&](const TCHAR* Pattern)
    {
        FRegexMatcher M(FRegexPattern(Pattern),Text);
        if(!M.FindNext()) return false;
        Target=M.GetCaptureGroup(1).TrimStartAndEnd(); return true;
    };
    if(Parse(TEXT("^(?:取消|关闭)\\s*(.+?)\\s*(?:的)?(?:发光高亮|高亮|发光)(?:效果|特效)?$"))) Enabled=false;
    else if(Parse(TEXT("^(?:让|把|将|给)\\s*(.+?)\\s*(?:的)?(?:发光高亮|高亮|发光)(?:效果|特效)?$"))) {}
    else if(Parse(TEXT("^(?:高亮|点亮)\\s*(.+)$"))) {}
    else return Fail(TEXT("未生成修改。请输入：让飞机编号发光高亮，或取消飞机编号高亮。每次指定一架；输入“高亮帮助”查看示例。"));
    if(Target==TEXT("选中的飞机") || Target==TEXT("当前选中的飞机") || Target==TEXT("选中飞机")) Target=SelectedId;
    else { Target.RemoveFromStart(TEXT("飞机")); Target.RemoveFromEnd(TEXT("飞机")); Target.TrimStartAndEndInline(); }
    if(Target.IsEmpty()) return Fail(TEXT("请先在右侧单位列表选择飞机，或直接填写实际编号。"));
    FDemoScenarioConfig Config; FString Error;
    if(!FScenarioConfigLoader::Parse(Current,Config,Error)) return Fail(TEXT("当前场景数据无效，未生成高亮候选。"));
    for(auto& Group:Config.Units) for(auto& Instance:Group.Instances) if(Instance.Id==Target)
    {
        if(Group.Type!=TEXT("aircraft")) return Fail(TEXT("本轮高亮只支持飞机，请选择飞机编号。"));
        if(Instance.bHighlighted==Enabled) return Fail(Target+(Enabled?TEXT("已经高亮，无需重复应用。"):TEXT("当前未高亮，无需关闭。")));
        Instance.bHighlighted=Enabled;
        Out.Scenario=FScenarioConfigLoader::Serialize(Config);
        Out.Reply=TEXT("已生成候选：")+Target+(Enabled?TEXT("开启金色发光高亮。"):TEXT("关闭发光高亮，恢复原外观。"))+TEXT("点击“应用到场景”后生效，可撤销；位置、机型和编队不变。");
        return true;
    }
    return Fail(TEXT("无法确定单架飞机：")+Target+TEXT("。请输入实际编号，例如RED01_001；可输入“查看飞机编号”。"));
}
