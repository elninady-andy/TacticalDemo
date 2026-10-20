using UnrealBuildTool;
public class TacticalDemoEditorTarget : TargetRules
{
    public TacticalDemoEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V4;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
        ExtraModuleNames.Add("TacticalDemo");
    }
}
