using UnrealBuildTool;
public class TacticalDemo : ModuleRules
{
    public TacticalDemo(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "UMG", "Slate", "SlateCore", "HTTP" });
    }
}
