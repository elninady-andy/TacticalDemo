#include "ChatProtocol.h"
#include "ScenarioConfigLoader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Internationalization/Regex.h"
#include "SceneLibrary.h"
#include "EquipmentLibrary.h"
#include "Misc/Paths.h"

namespace
{
    bool CheckFields(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& Allowed, FString& Error)
    {
        for(const auto& Field:Object->Values)
            if(!Allowed.Contains(Field.Key))
            { Error=TEXT("模型配置存在不支持或层级错误的字段：")+Field.Key+TEXT("。场景未修改。"); return false; }
        return true;
    }

    bool ExpandCommand(const TSharedPtr<FJsonObject>& Root, const FString& Current, const FString& User,
        TSharedPtr<FJsonObject>& Scene, FString& Error)
    {
        if(Root->HasField(TEXT("create")))
        {
            if(!CheckFields(Root,{TEXT("create"),TEXT("reply")},Error)) return false;
            const TArray<TSharedPtr<FJsonValue>>* Groups=nullptr;
            if(!Root->TryGetArrayField(TEXT("create"),Groups))
            { Error=TEXT("create 必须是单位组数组。"); return false; }
            Scene=MakeShared<FJsonObject>();
            Scene->SetStringField(TEXT("scenarioName"),TEXT("GeneratedScene"));
            TArray<TSharedPtr<FJsonValue>> Units;
            for(const auto& Value:*Groups)
            {
                if(Value->Type!=EJson::Object) { Error=TEXT("单位组必须是对象。"); return false; }
                const auto Group=Value->AsObject();
                if(!CheckFields(Group,{TEXT("type"),TEXT("side"),TEXT("count"),TEXT("x"),TEXT("y"),TEXT("height"),TEXT("spacing"),TEXT("modelId"),TEXT("textureId"),TEXT("route")},Error)) return false;
                const auto Type=Group->Values.Find(TEXT("type"));
                if(!Type || (*Type)->Type!=EJson::String || !FScenarioConfigLoader::IsSupportedType((*Type)->AsString()))
                { Error=TEXT("单位 type 必须为 aircraft/vehicle/radar/target。"); return false; }
                if(!Group->HasField(TEXT("side")) || !Group->HasField(TEXT("count")))
                { Error=TEXT("创建单位必须提供 side 和 count。"); return false; }
                auto Unit=MakeShared<FJsonObject>();
                Unit->SetStringField(TEXT("id"),FString::Printf(TEXT("U%03d"),Units.Num()+1));
                FString ModelId=(*Type)->AsString()+TEXT("_default");
                if(const auto Model=Group->Values.Find(TEXT("modelId")); Model && (User.IsEmpty() || User.Contains(TEXT("模型")) || User.Contains(TEXT("model"),ESearchCase::IgnoreCase)))
                {
                    if((*Model)->Type!=EJson::String || (*Model)->AsString().TrimStartAndEnd().IsEmpty())
                    { Error=TEXT("modelId 必须是设备库中已登记的模型 ID 字符串。"); return false; }
                    ModelId=(*Model)->AsString();
                }
                Unit->SetStringField(TEXT("modelId"),ModelId);
                if(const auto Texture=Group->Values.Find(TEXT("textureId")); Texture && (User.IsEmpty() || User.Contains(TEXT("贴图")) || User.Contains(TEXT("涂装")))) Unit->SetField(TEXT("textureId"),*Texture);
                for(const FString Key:{TEXT("type"),TEXT("side"),TEXT("count")}) Unit->SetField(Key,Group->Values[Key]);
                auto Number=[&](const FString& Key,double Default)->TSharedPtr<FJsonValue>
                {
                    bool Requested=true;
                    if(!User.IsEmpty())
                    {
                        if(Key==TEXT("height")) Requested=User.Contains(TEXT("高度")) || User.Contains(TEXT("height"),ESearchCase::IgnoreCase);
                        else if(Key==TEXT("spacing")) Requested=User.Contains(TEXT("间距")) || User.Contains(TEXT("spacing"),ESearchCase::IgnoreCase);
                        else Requested=User.Contains(Key,ESearchCase::IgnoreCase) || User.Contains(TEXT("坐标")) || User.Contains(TEXT("位置"));
                    }
                    const auto Found=Group->Values.Find(Key); return Requested && Found?*Found:MakeShared<FJsonValueNumber>(Default);
                };
                auto Position=MakeShared<FJsonObject>();
                Position->SetField(TEXT("x"),Number(TEXT("x"),0));
                Position->SetField(TEXT("y"),Number(TEXT("y"),0));
                Position->SetField(TEXT("z"),Number(TEXT("height"),0));
                Unit->SetObjectField(TEXT("position"),Position);
                Unit->SetField(TEXT("spacing"),Number(TEXT("spacing"),10));
                if(const auto Route=Group->Values.Find(TEXT("route")); Route && (User.IsEmpty() || User.Contains(TEXT("航迹")) || User.Contains(TEXT("航线")) || User.Contains(TEXT("航点")) || User.Contains(TEXT("轨迹")) || User.Contains(TEXT("路径")) || User.Contains(TEXT("route"),ESearchCase::IgnoreCase)))
                {
                    if((*Route)->Type!=EJson::Array) { Error=TEXT("route 必须是航点数组。"); return false; }
                    Unit->SetField(TEXT("route"),*Route);
                }
                Units.Add(MakeShared<FJsonValueObject>(Unit));
            }
            Scene->SetArrayField(TEXT("units"),Units);
            return true;
        }
        if(!CheckFields(Root,{TEXT("update"),TEXT("targetSide"),TEXT("targetModel"),TEXT("set"),TEXT("reply")},Error)) return false;
        const auto Target=Root->Values.Find(TEXT("update"));
        const auto ChangesField=Root->Values.Find(TEXT("set"));
        TSharedPtr<FJsonObject> ChangesObject;
        if(ChangesField && (*ChangesField)->Type==EJson::Object) ChangesObject=(*ChangesField)->AsObject();
        else if(ChangesField && (*ChangesField)->Type==EJson::Array && (*ChangesField)->AsArray().Num()==1 && (*ChangesField)->AsArray()[0]->Type==EJson::Object)
            ChangesObject=(*ChangesField)->AsArray()[0]->AsObject();
        if(!Target || (*Target)->Type!=EJson::String ||
            (!FScenarioConfigLoader::IsSupportedType((*Target)->AsString()) && (*Target)->AsString()!=TEXT("all")) ||
            !ChangesObject.IsValid() || ChangesObject->Values.IsEmpty())
        { Error=TEXT("修改配置需要有效的 update 类型和非空 set 对象。"); return false; }
        TSharedPtr<FJsonObject> NormalizedChanges=MakeShared<FJsonObject>();
        for(const auto& Pair:ChangesObject->Values) NormalizedChanges->SetField(Pair.Key,Pair.Value);
        // Qwen occasionally repeats the container key: set:{height:30,set:{side:"blue"}}.
        // Flatten exactly one nested set object; all other unknown fields remain rejected.
        if(const auto Nested=NormalizedChanges->Values.Find(TEXT("set")))
        {
            if((*Nested)->Type!=EJson::Object) { Error=TEXT("set 嵌套字段必须是对象。"); return false; }
            NormalizedChanges->RemoveField(TEXT("set"));
            // In the observed malformed response, the model also emitted count:0 as a
            // default while placing the requested side in the nested object. Preserve
            // the existing count in that case; an explicit standalone count:0 remains valid.
            if(const auto Count=NormalizedChanges->Values.Find(TEXT("count")); Count &&
                (*Count)->Type==EJson::Number && (*Count)->AsNumber()==0.0 && (*Nested)->AsObject()->HasField(TEXT("side")))
                NormalizedChanges->RemoveField(TEXT("count"));
            for(const auto& Pair:(*Nested)->AsObject()->Values)
            {
                if(NormalizedChanges->HasField(Pair.Key)) { Error=TEXT("set 中存在重复字段。"); return false; }
                NormalizedChanges->SetField(Pair.Key,Pair.Value);
            }
        }
        FString TargetSide;
        FString TargetModel;
        if(Root->HasField(TEXT("targetModel")) && (!Root->TryGetStringField(TEXT("targetModel"),TargetModel) || TargetModel.IsEmpty() || (!User.IsEmpty() && !User.Contains(TargetModel,ESearchCase::IgnoreCase))))
        { Error=TEXT("targetModel 必须是指令中明确指定的模型 ID。"); return false; }
        if(const auto Side=Root->Values.Find(TEXT("targetSide")))
        {
            if((*Side)->Type!=EJson::String || ((*Side)->AsString()!=TEXT("red") && (*Side)->AsString()!=TEXT("blue")))
            { Error=TEXT("targetSide 必须为 red 或 blue。"); return false; }
            TargetSide=(*Side)->AsString();
        }
        if(const auto Side=NormalizedChanges->Values.Find(TEXT("targetSide")))
        {
            if(!TargetSide.IsEmpty() || (*Side)->Type!=EJson::String || ((*Side)->AsString()!=TEXT("red") && (*Side)->AsString()!=TEXT("blue")))
            { Error=TEXT("targetSide 必须且只能指定一次 red 或 blue。"); return false; }
            TargetSide=(*Side)->AsString();
            NormalizedChanges->RemoveField(TEXT("targetSide"));
        }
        // The user's explicit source side takes precedence over an omitted or
        // hallucinated selector. Destination phrases such as "改为蓝方" do not
        // contain a unit noun after the side and therefore are not selectors.
        auto MentionsSourceSide=[&](const FString& Side)
        {
            for(const FString Noun:{TEXT("飞机"),TEXT("车辆"),TEXT("雷达"),TEXT("目标"),TEXT("单位")})
                if(User.Contains(Side+Noun) || User.Contains(Side+TEXT("的")+Noun)) return true;
            return false;
        };
        const bool bRedSource=MentionsSourceSide(TEXT("红方"));
        const bool bBlueSource=MentionsSourceSide(TEXT("蓝方"));
        if(bRedSource!=bBlueSource) TargetSide=bRedSource?TEXT("red"):TEXT("blue");
        if(!CheckFields(NormalizedChanges,{TEXT("count"),TEXT("side"),TEXT("height"),TEXT("z"),TEXT("x"),TEXT("y"),TEXT("spacing"),TEXT("modelId"),TEXT("textureId"),TEXT("route")},Error)) return false;
        const bool bHasRouteIntent=(User.Contains(TEXT("航迹")) || User.Contains(TEXT("航线")) || User.Contains(TEXT("轨迹")) || User.Contains(TEXT("航点")) || User.Contains(TEXT("路径")) || User.Contains(TEXT("route"), ESearchCase::IgnoreCase)) &&
            (User.Contains(TEXT("设置")) || User.Contains(TEXT("修改")) || User.Contains(TEXT("清除")) || User.Contains(TEXT("添加")) || User.Contains(TEXT("经过")) || User.Contains(TEXT("删除")));
        const bool bHasPositionIntent=!bHasRouteIntent && (User.Contains(TEXT("移动")) || User.Contains(TEXT("移到")) || User.Contains(TEXT("挪到")) || User.Contains(TEXT("位置")) || User.Contains(TEXT("坐标")));
        const bool bHasTextureIntent=User.Contains(TEXT("贴图")) || User.Contains(TEXT("涂装")) || User.Contains(TEXT("texture"),ESearchCase::IgnoreCase);
        const bool bHasModelIntent=User.Contains(TEXT("模型")) || User.Contains(TEXT("model"), ESearchCase::IgnoreCase) || (!bHasTextureIntent && (User.Contains(TEXT("替换")) || User.Contains(TEXT("换成")) || User.Contains(TEXT("更换"))));
        const bool bHasCountIntent=(User.Contains(TEXT("数量")) || User.Contains(TEXT("架")) || User.Contains(TEXT("辆")) || User.Contains(TEXT("台")) || User.Contains(TEXT("count"), ESearchCase::IgnoreCase)) && !User.Contains(TEXT("保持数量"));
        const bool bHasSideIntent=User.Contains(TEXT("阵营")) || User.Contains(TEXT("改为红方")) || User.Contains(TEXT("改为蓝方")) || User.Contains(TEXT("设置为红方")) || User.Contains(TEXT("设置为蓝方")) || User.Contains(TEXT("切换到红方")) || User.Contains(TEXT("切换到蓝方"));
        // Qwen may echo current values during a position-only request.  Treat
        // "保持高度和间距不变" as preservation even when the two words are not adjacent.
        const bool bPreservesExistingValues=User.Contains(TEXT("保持")) && User.Contains(TEXT("不变"));
        const bool bHasHeightChange=(User.Contains(TEXT("高度")) && !bPreservesExistingValues) || User.Contains(TEXT("height"), ESearchCase::IgnoreCase);
        const bool bHasSpacingChange=(User.Contains(TEXT("间距")) && !bPreservesExistingValues) || User.Contains(TEXT("spacing"), ESearchCase::IgnoreCase);
        if(!User.IsEmpty())
        {
            if(bHasTextureIntent)
                for(const FString Key:{TEXT("count"),TEXT("side"),TEXT("height"),TEXT("z"),TEXT("x"),TEXT("y"),TEXT("spacing"),TEXT("modelId"),TEXT("route")}) NormalizedChanges->RemoveField(Key);
            if(!bHasPositionIntent) { NormalizedChanges->RemoveField(TEXT("x")); NormalizedChanges->RemoveField(TEXT("y")); }
            if(!bHasHeightChange) { NormalizedChanges->RemoveField(TEXT("height")); NormalizedChanges->RemoveField(TEXT("z")); }
            if(!bHasSpacingChange) NormalizedChanges->RemoveField(TEXT("spacing"));
            if(!bHasCountIntent) NormalizedChanges->RemoveField(TEXT("count"));
            if(!bHasSideIntent) NormalizedChanges->RemoveField(TEXT("side"));
            if(!bHasModelIntent) NormalizedChanges->RemoveField(TEXT("modelId"));
            if(!bHasTextureIntent) NormalizedChanges->RemoveField(TEXT("textureId"));
            if(!bHasRouteIntent) NormalizedChanges->RemoveField(TEXT("route"));
        }
        if(const auto Z=NormalizedChanges->Values.Find(TEXT("z")))
        {
            if(NormalizedChanges->HasField(TEXT("height")))
            {
                const auto Height=NormalizedChanges->Values.Find(TEXT("height"));
                if((*Z)->Type!=EJson::Number || (*Height)->Type!=EJson::Number || (*Z)->AsNumber()!=(*Height)->AsNumber())
                { Error=TEXT("z 与 height 的值冲突，场景未修改。"); return false; }
                NormalizedChanges->RemoveField(TEXT("z"));
            }
            else { NormalizedChanges->SetField(TEXT("height"),*Z); NormalizedChanges->RemoveField(TEXT("z")); }
        }
        if(NormalizedChanges->Values.IsEmpty()) { Error=TEXT("模型回复没有提供与用户要求相符的可修改字段；当前场景保持不变。"); return false; }
        if(const auto Model=NormalizedChanges->Values.Find(TEXT("modelId")); Model && ((*Model)->Type!=EJson::String || (*Model)->AsString().TrimStartAndEnd().IsEmpty()))
        { Error=TEXT("modelId 必须是设备库中已登记的模型 ID 字符串。"); return false; }
        if(const auto Texture=NormalizedChanges->Values.Find(TEXT("textureId")); Texture && ((*Texture)->Type!=EJson::String || (*Texture)->AsString().TrimStartAndEnd().IsEmpty()))
        { Error=TEXT("textureId 必须是该模型已登记的贴图 ID 字符串。"); return false; }
        if(const auto Route=NormalizedChanges->Values.Find(TEXT("route")); Route && (*Route)->Type!=EJson::Array)
        { Error=TEXT("route 必须是航点数组。"); return false; }
        FDemoScenarioConfig Existing;
        if(!FScenarioConfigLoader::Parse(Current,Existing,Error) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Current),Scene))
        { Error=TEXT("无法读取当前场景，请先创建或加载场景。"); return false; }
        int32 Matched=0;
        FEquipmentLibrary SelectionLibrary;
        if(!TargetModel.IsEmpty() || bHasTextureIntent)
        {
            if(!SelectionLibrary.Load(FPaths::ProjectConfigDir()/TEXT("EquipmentLibrary.json"),Error)) return false;
            if(bHasTextureIntent)
            {
                FString RequestedModel;
                if(!SelectionLibrary.TextureModelSelector(User,RequestedModel,Error)) return false;
                if(!RequestedModel.IsEmpty()) TargetModel=RequestedModel;
            }
        }
        FString DesiredSide;
        if(const auto Side=NormalizedChanges->Values.Find(TEXT("side")); Side && (*Side)->Type==EJson::String)
            DesiredSide=(*Side)->AsString();
        const bool bTargetsAllOfType=User.Contains(TEXT("全部")) || User.Contains(TEXT("所有")) || User.Contains(TEXT("全体"));
        bool UseSideSelector=!TargetSide.IsEmpty();
        if(UseSideSelector)
        {
            int32 SideMatches=0;
            for(const auto& Value:Scene->GetArrayField(TEXT("units")))
                if(((*Target)->AsString()==TEXT("all") || Value->AsObject()->GetStringField(TEXT("type"))==(*Target)->AsString()) && Value->AsObject()->GetStringField(TEXT("side"))==TargetSide) ++SideMatches;
            // If the model repeats the destination side in targetSide, it is not a source
            // selector for a request that explicitly targets all units of this type.
            if(!bRedSource && !bBlueSource && DesiredSide==TargetSide && (bTargetsAllOfType || SideMatches==0)) UseSideSelector=false;
        }
        for(const auto& Value:Scene->GetArrayField(TEXT("units")))
        {
            auto Unit=Value->AsObject();
            if((*Target)->AsString()!=TEXT("all") && Unit->GetStringField(TEXT("type"))!=(*Target)->AsString()) continue;
            if(UseSideSelector && Unit->GetStringField(TEXT("side"))!=TargetSide) continue;
            if(!TargetModel.IsEmpty() && SelectionLibrary.EffectiveModelId(Unit->GetStringField(TEXT("modelId")),Unit->GetStringField(TEXT("type")),Unit->GetStringField(TEXT("side")))!=TargetModel) continue;
            ++Matched;
            if(NormalizedChanges->HasField(TEXT("modelId")) && !NormalizedChanges->HasField(TEXT("textureId"))) Unit->RemoveField(TEXT("textureId"));
            const TArray<TSharedPtr<FJsonValue>>* SavedInstances=nullptr;
            if(Unit->TryGetArrayField(TEXT("instances"),SavedInstances))
            {
                TArray<TSharedPtr<FJsonValue>> Instances=*SavedInstances;
                auto Anchor=Unit->GetObjectField(TEXT("position"));
                const FVector Before(Anchor->GetNumberField(TEXT("x")),Anchor->GetNumberField(TEXT("y")),Anchor->GetNumberField(TEXT("z")));
                FVector After=Before;
                double Number;
                if(NormalizedChanges->TryGetNumberField(TEXT("x"),Number)) After.X=Number;
                if(NormalizedChanges->TryGetNumberField(TEXT("y"),Number)) After.Y=Number;
                if(NormalizedChanges->TryGetNumberField(TEXT("height"),Number)) After.Z=Number;
                double Count=Instances.Num(),Spacing=10;
                Unit->TryGetNumberField(TEXT("spacing"),Spacing);
                NormalizedChanges->TryGetNumberField(TEXT("spacing"),Spacing);
                NormalizedChanges->TryGetNumberField(TEXT("count"),Count);
                if(!FMath::IsFinite(Count) || Count<0 || Count>100 || FMath::FloorToDouble(Count)!=Count || !FMath::IsFinite(Spacing))
                { Error=TEXT("数量或间距无效。"); return false; }
                for(int32 I=0;I<Instances.Num();++I)
                {
                    if(NormalizedChanges->HasField(TEXT("route"))) Instances[I]->AsObject()->RemoveField(TEXT("flightRoute"));
                    auto P=Instances[I]->AsObject()->GetObjectField(TEXT("position"));
                    P->SetNumberField(TEXT("x"),P->GetNumberField(TEXT("x"))+After.X-Before.X);
                    P->SetNumberField(TEXT("y"),P->GetNumberField(TEXT("y"))+After.Y-Before.Y);
                    P->SetNumberField(TEXT("z"),P->GetNumberField(TEXT("z"))+After.Z-Before.Z);
                    if(NormalizedChanges->HasField(TEXT("spacing"))) P->SetNumberField(TEXT("y"),After.Y+I*Spacing);
                }
                if(Count<Instances.Num()) Instances.SetNum(static_cast<int32>(Count));
                TSet<FString> UsedIds;
                for(const auto& Other:Scene->GetArrayField(TEXT("units")))
                {
                    const TArray<TSharedPtr<FJsonValue>>* Others=nullptr;
                    if(Other->AsObject()->TryGetArrayField(TEXT("instances"),Others))
                        for(const auto& I:*Others) UsedIds.Add(I->AsObject()->GetStringField(TEXT("id")));
                }
                for(int32 I=Instances.Num();I<static_cast<int32>(Count);++I)
                {
                    auto Instance=MakeShared<FJsonObject>(),P=MakeShared<FJsonObject>();
                    FString Id=FString::Printf(TEXT("%s_%03d"),*Unit->GetStringField(TEXT("id")),I+1);
                    if(UsedIds.Contains(Id)) Id=Unit->GetStringField(TEXT("id"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
                    UsedIds.Add(Id); Instance->SetStringField(TEXT("id"),Id); Instance->SetNumberField(TEXT("yaw"),0);
                    P->SetNumberField(TEXT("x"),After.X); P->SetNumberField(TEXT("y"),After.Y+I*Spacing); P->SetNumberField(TEXT("z"),After.Z);
                    Instance->SetObjectField(TEXT("position"),P); Instances.Add(MakeShared<FJsonValueObject>(Instance));
                }
                Unit->SetArrayField(TEXT("instances"),Instances);
            }
            for(const auto& Change:NormalizedChanges->Values)
            {
                if(Change.Key==TEXT("height") || Change.Key==TEXT("x") || Change.Key==TEXT("y"))
                    Unit->GetObjectField(TEXT("position"))->SetField(Change.Key==TEXT("height")?TEXT("z"):Change.Key,Change.Value);
                else Unit->SetField(Change.Key,Change.Value);
            }
        }
        if(Matched==0) { Error=TEXT("当前场景中没有该类型的单位，请先创建。"); return false; }
        return true;
    }

    bool IsCreation(const FString& User)
    { return User.Contains(TEXT("创建")) || User.Contains(TEXT("新建")) || User.Contains(TEXT("生成")); }
}

bool FDemoChatProtocol::Parse(const FString& Content, FDemoChatReply& Out, FString& Error, const FString& CurrentScenario, const FString& UserInstruction)
{
    Out = FDemoChatReply(); Error.Reset();
    FString Value=Content.TrimStartAndEnd();
    if(Value.IsEmpty() || Value.Len()>200000) { Error=TEXT("模型回复为空或超过长度限制。"); return false; }
    // Qwen sometimes writes the scope word as an invalid JSON count:all.
    // Normalize this observed token only for an explicit all-units request.
    // The unrequested count field is subsequently removed by intent filtering.
    if(UserInstruction.Contains(TEXT("全部")) || UserInstruction.Contains(TEXT("所有")) || UserInstruction.Contains(TEXT("全体")))
    {
        const FRegexPattern Pattern(TEXT("\"count\"\\s*:\\s*(all)(?=\\s*[,}])"));
        for(;;)
        {
            FRegexMatcher Matcher(Pattern,Value);
            if(!Matcher.FindNext()) break;
            const int32 Start=Matcher.GetCaptureGroupBeginning(1), End=Matcher.GetCaptureGroupEnding(1);
            Value=Value.Left(Start)+TEXT("null")+Value.Mid(End);
        }
    }
    if(Value.StartsWith(TEXT("```")))
    {
        int32 Newline;
        if(!Value.FindChar('\n',Newline) || !Value.EndsWith(TEXT("```"))) { Error=TEXT("JSON代码块不完整。"); return false; }
        Value=Value.Mid(Newline+1,Value.Len()-Newline-4).TrimStartAndEnd();
    }
    if(!Value.StartsWith(TEXT("{")) && !Value.StartsWith(TEXT("["))) { Out.Reply=Value; return true; }
    TSharedPtr<FJsonObject> Root;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Value),Root) || !Root.IsValid())
    { Error=TEXT("模型返回的JSON格式无效，请重试。"); return false; }
    const bool TextureChange=IsSceneRequest(UserInstruction) && !IsCreation(UserInstruction) && (UserInstruction.Contains(TEXT("贴图")) || UserInstruction.Contains(TEXT("涂装")) || UserInstruction.Contains(TEXT("texture"),ESearchCase::IgnoreCase));
    if(TextureChange && !Root->HasField(TEXT("update")))
    { Error=TEXT("贴图切换回复不符合要求，原场景保持不变。"); return false; }
    auto Reply=Root->Values.Find(TEXT("reply"));
    if(Reply && (!Reply->IsValid() || (*Reply)->Type!=EJson::String))
    { Error=TEXT("模型说明字段 reply 必须是字符串。场景未修改。"); return false; }
    FString Explanation=Reply?(*Reply)->AsString().TrimStartAndEnd():FString();
    if(Root->HasField(TEXT("levelId")))
    {
        FString Id,Asset;
        if(!CheckFields(Root,{TEXT("levelId"),TEXT("reply")},Error) || !Root->TryGetStringField(TEXT("levelId"),Id) || !IsSceneRequest(UserInstruction) || !FDemoSceneLibrary::IsSwitchRequest(UserInstruction) || !FDemoSceneLibrary::Resolve(Id,Asset,Error))
        { if(Error.IsEmpty()) Error=TEXT("场景切换需要明确指令和已登记场景 ID。"); return false; }
        Out.LevelId=Id; Out.Reply=Explanation.IsEmpty()?TEXT("场景已校验，点击应用切换并加载该场景初始配置。"):Explanation; return true;
    }
    TSharedPtr<FJsonObject> Object;
    if(Root->HasField(TEXT("create")) || Root->HasField(TEXT("update")))
    { if(!ExpandCommand(Root,CurrentScenario,UserInstruction,Object,Error)) return false; }
    else if(Root->HasField(TEXT("scenarioName")) || Root->HasField(TEXT("units"))) Object=Root;
    else
    {
        if(!CheckFields(Root,{TEXT("reply"),TEXT("scenario")},Error)) return false;
        if(auto Field=Root->Values.Find(TEXT("scenario")))
        {
            if((*Field)->Type!=EJson::Null)
            {
                if((*Field)->Type!=EJson::Object) { Error=TEXT("scenario必须为对象或null。"); return false; }
                Object=(*Field)->AsObject();
            }
        }
    }
    FString Scenario;
    if(Object.IsValid())
    {
            if(!CheckFields(Object,{TEXT("scenarioName"),TEXT("units"),TEXT("schemaVersion"),TEXT("mapAsset")},Error)) return false;
            // Reject unsupported actions rather than silently pretending to perform them.
            const TArray<TSharedPtr<FJsonValue>>* Groups=nullptr;
            if(Object->TryGetArrayField(TEXT("units"),Groups)) for(const auto& Group:*Groups)
            {
                if(Group->Type!=EJson::Object) continue;
                if(!CheckFields(Group->AsObject(),{TEXT("id"),TEXT("modelId"),TEXT("textureId"),TEXT("type"),TEXT("side"),TEXT("count"),TEXT("position"),TEXT("spacing"),TEXT("route"),TEXT("instances")},Error)) return false;
                const TSharedPtr<FJsonObject>* Position=nullptr;
                if(Group->AsObject()->TryGetObjectField(TEXT("position"),Position) && !CheckFields(*Position,{TEXT("x"),TEXT("y"),TEXT("z")},Error)) return false;
            }
            FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Scenario));
            FDemoScenarioConfig Parsed;
            if(!FScenarioConfigLoader::Parse(Scenario,Parsed,Error)) { Error=TEXT("场景校验失败：")+Error; return false; }
            // Only validated scene data may supply a missing presentation label.
            if(Explanation.IsEmpty()) Explanation=TEXT("场景配置已校验，请点击“应用到场景”。");
    }
    if(Explanation.IsEmpty()) { Error=TEXT("模型没有返回有效说明或完整场景。请重试，当前场景已保留。"); return false; }
    Out.Reply=MoveTemp(Explanation); Out.Scenario=MoveTemp(Scenario);
    return true;
}
bool FDemoChatProtocol::IsSceneRequest(const FString& User)
{
    for(const FString Word:{TEXT("如何"),TEXT("怎么"),TEXT("哪些"),TEXT("可以"),TEXT("能否"),TEXT("是否"),TEXT("是什么"),TEXT("什么是"),TEXT("不要修改"),TEXT("不要创建"),TEXT("不要生成")})
        if(User.Contains(Word)) return false;
    return FDemoSceneLibrary::IsSwitchRequest(User) || ((User.Contains(TEXT("贴图")) || User.Contains(TEXT("涂装"))) && User.Contains(TEXT("切换"))) || IsCreation(User) || User.Contains(TEXT("改为")) || User.Contains(TEXT("修改")) || User.Contains(TEXT("调整")) || User.Contains(TEXT("设置")) ||
        User.Contains(TEXT("移动")) || User.Contains(TEXT("移到")) || User.Contains(TEXT("挪到")) || User.Contains(TEXT("替换")) ||
        User.Contains(TEXT("更换")) || User.Contains(TEXT("换成")) || User.Contains(TEXT("航迹")) || User.Contains(TEXT("航线")) ||
        User.Contains(TEXT("轨迹")) || User.Contains(TEXT("航点")) || User.Contains(TEXT("路径"));
}
FString FDemoChatProtocol::SystemPrompt(const FString& User, const FString& ModelCatalog,const FString& TextureCatalog)
{
    if(IsSceneRequest(User) && !IsCreation(User) && (User.Contains(TEXT("贴图")) || User.Contains(TEXT("涂装"))))
        return TEXT("提取飞机贴图切换指令，只输出完整JSON。红方飞机换成第二款贴图 -> {\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"textureId\":\"2\"}}。蓝方飞机换成第一款贴图 -> {\"update\":\"aircraft\",\"targetSide\":\"blue\",\"set\":{\"textureId\":\"1\"}}。J20飞机换成第二款贴图 -> {\"update\":\"aircraft\",\"targetModel\":\"J20\",\"set\":{\"textureId\":\"2\"}}。J10飞机换成第一款贴图 -> {\"update\":\"aircraft\",\"targetModel\":\"J10\",\"set\":{\"textureId\":\"1\"}}。第一款/贴图1使用字符串1，第二款/贴图2使用字符串2。全部飞机不填targetSide或targetModel。set只有textureId，不修改飞机模型、阵营、坐标、数量或间距。可用模型及贴图ID：")+TextureCatalog;
    if(IsSceneRequest(User) && FDemoSceneLibrary::IsSwitchRequest(User))
        return TEXT("从用户指令选择背景场景，只输出JSON，例如切换到机场场景 -> {\"levelId\":\"airport\"}。可选场景：")+FDemoSceneLibrary::Catalog()+TEXT("。只能输出已登记的levelId，不要输出create/update或飞机配置。切换会加载目标场景的初始单位配置。");
    const FString Catalog = ModelCatalog.IsEmpty() ? TEXT("无可用模型目录，请告知用户先在设备库配置模型。") : TEXT("设备库允许的 modelId 与类型：") + ModelCatalog + TEXT("。只能选择列表中的 ID。");
    if(!IsSceneRequest(User))
        return TEXT("你是本地UE场景助手，用中文简短回答。现在是普通对话，只输出文字，不输出JSON，不要生成或修改场景。可用功能：创建aircraft飞机、vehicle车辆、radar雷达、target目标，修改数量、阵营、模型、静态坐标、高度、间距和静态航迹。航迹只保存并显示航点，单位不会沿航迹移动。用户需要发送明确场景指令，再点击应用到场景。不能声称已执行操作。");
    if(!IsCreation(User) && !ModelCatalog.IsEmpty() &&
        (User.Contains(TEXT("模型")) || User.Contains(TEXT("替换")) || User.Contains(TEXT("换成")) || User.Contains(TEXT("更换"))) &&
        !User.Contains(TEXT("数量")) && !User.Contains(TEXT("高度")) && !User.Contains(TEXT("移动")) && !User.Contains(TEXT("航迹")) && !User.Contains(TEXT("间距")))
        return TEXT("提取模型替换指令，只输出一个完整JSON对象。红方飞机换成J20：{\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"modelId\":\"J20\"}}。蓝方飞机换成J10：{\"update\":\"aircraft\",\"targetSide\":\"blue\",\"set\":{\"modelId\":\"J10\"}}。全部飞机换成J20：{\"update\":\"aircraft\",\"set\":{\"modelId\":\"J20\"}}。按用户要求选择模型和原有阵营，set只有modelId一个字段，不能有null或重复字段。不要添加高度、位置、间距、side或route。车辆用vehicle，雷达用radar，目标用target。")+Catalog;
    if(IsCreation(User))
        return TEXT("从创建场景指令提取参数，只输出JSON。格式示例：{\"create\":[{\"type\":\"aircraft\",\"side\":\"red\",\"count\":1,\"x\":0,\"y\":0,\"height\":50,\"spacing\":10}]}。每组单位一个对象。type为aircraft飞机/vehicle车辆/radar雷达/target目标。side为red/blue。数量count，高度height，间距spacing和route航点坐标单位均为米。可选modelId必须来自设备库目录。route格式为[{\"x\":0,\"y\":0,\"z\":5},{\"x\":20,\"y\":10,\"z\":5}]，至少2点；航迹只显示，不让单位移动。未提到x/y/height时为0，未提到间距时为10。") + Catalog;
    return TEXT("从用户指令提取需要修改的字段，只输出JSON。例：飞机数量改为4架 -> {\"update\":\"aircraft\",\"set\":{\"count\":4}}。例：只把红方飞机数量改为4架 -> {\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"count\":4}}。例：把蓝方车辆移动到X=20米、Y=10米 -> {\"update\":\"vehicle\",\"targetSide\":\"blue\",\"set\":{\"x\":20,\"y\":10}}。例：把当前场景中的飞机全部改为蓝方 -> {\"update\":\"aircraft\",\"set\":{\"side\":\"blue\"}}，此时不要填targetSide；targetSide只用于筛选原有阵营。全部只表示影响范围，绝不能输出 count:all。例：将红方飞机模型替换为 aircraft_blue_basic -> {\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"modelId\":\"aircraft_blue_basic\"}}。例：设置航迹 -> {\"update\":\"aircraft\",\"targetSide\":\"red\",\"set\":{\"route\":[{\"x\":0,\"y\":0,\"z\":5},{\"x\":20,\"y\":10,\"z\":5}]}}；清除航迹使用 route:[]。update只能填aircraft/vehicle/radar/target/all；targetSide只能填red或blue。set只能包含本次要求修改的count、side、height、x、y、spacing、modelId、route字段。位置高度使用height，不要把当前高度或间距重复输出。没有明确要求替换模型时不要输出modelId；没有要求航迹时不要输出route。坐标单位为米。未提及的现有参数必须保持不变。航迹只保存和静态显示航点，单位不会自行移动。不要生成超出设备库的模型 ID。") + Catalog;
}
