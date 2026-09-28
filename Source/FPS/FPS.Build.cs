using UnrealBuildTool;

public class FPS : ModuleRules
{
	public FPS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"PropertyBindingUtils",
			"GameplayStateTreeModule",
			"PhysicsCore",
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",
			"CustomizableObject",
			"AnimGraphRuntime",
			"AnimationCore",
			"DeveloperSettings",
			"OnlineSubsystem",
			"OnlineSubsystemSteam",
			"OnlineSubsystemUtils",
			"HairStrandsCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Niagara",
			"NetworkReplayStreaming",
			"InMemoryNetworkReplayStreaming"
		});


		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
