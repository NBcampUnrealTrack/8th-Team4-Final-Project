// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class FPSTarget : TargetRules
{
	public FPSTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		ExtraModuleNames.AddRange( new string[] { "FPS" } );
	}
}
