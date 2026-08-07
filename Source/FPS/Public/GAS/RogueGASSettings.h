// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "RogueGASSettings.generated.h"

class UGameplayEffect;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Rogue GAS"))
class FPS_API URogueGASSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 대미지 캐리어 GE
	// URogueDamageSubsystem 이 스펙을 만들 때 사용
	UPROPERTY(Config, EditAnywhere, Category = "Damage")
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;
};
