// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class FPSTarget : TargetRules
{
	public FPSTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		ExtraModuleNames.AddRange( new string[] { "FPS" } );

		// 쉬핑 빌드에서도 UE_LOG와 로그 파일이 남도록 한다
		// 패키징 이후 문제가 해결되면 삭제
		bUseLoggingInShipping = true;
	}
}
