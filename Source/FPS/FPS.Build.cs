// Fill out your copyright notice in the Description page of Project Settings.

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
			"PhysicsCore",
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",
			"CustomizableObject",
			"AnimGraphRuntime",
			"DeveloperSettings",
			"OnlineSubsystem",
			"OnlineSubsystemSteam",
			"OnlineSubsystemUtils",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Niagara",
			"NetworkReplayStreaming",
			"InMemoryNetworkReplayStreaming"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"UMGEditor",
				"AssetTools",
				"AssetRegistry"
			});
		}

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
	}
}
