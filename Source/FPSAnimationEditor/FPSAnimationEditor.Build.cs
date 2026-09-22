using UnrealBuildTool;
public class FPSAnimationEditor : ModuleRules
{
 public FPSAnimationEditor(ReadOnlyTargetRules Target):base(Target)
 {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","FPS","AnimGraph","AnimGraphRuntime","BlueprintGraph"});
  PrivateDependencyModuleNames.AddRange(new[]{"UnrealEd"});
 }
}
