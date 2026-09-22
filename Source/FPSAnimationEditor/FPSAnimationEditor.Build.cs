using UnrealBuildTool;
public class FPSAnimationEditor : ModuleRules
{
 public FPSAnimationEditor(ReadOnlyTargetRules Target):base(Target)
 {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","FPS","AnimGraph","AnimGraphRuntime","BlueprintGraph"});
  PrivateDependencyModuleNames.AddRange(new[]{"UnrealEd","CustomizableObject","AssetRegistry"});
  PrivateDependencyModuleNames.AddRange(new[]{"ChaosClothAssetEngine","ChaosClothAsset","ChaosOutfitAssetEngine","Chaos","MetaHumanCharacter","MetaHumanCharacterEditor","MetaHumanCharacterPalette","MetaHumanDefaultPipeline","MetaHumanDefaultEditorPipeline"});
 }
}
