using UnrealBuildTool;
public class TacticalDemoTarget : TargetRules
{
    public TacticalDemoTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V4;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
        ExtraModuleNames.Add("TacticalDemo");
    }
}
