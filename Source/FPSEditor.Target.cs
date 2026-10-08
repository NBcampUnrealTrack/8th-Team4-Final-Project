// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class FPSEditorTarget : TargetRules
{
	public FPSEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		ExtraModuleNames.AddRange( new string[] { "FPS" } );
	}
}
