// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "RogueGASSettings.generated.h"

class UGameplayEffect;
class URogueAbilitySet;
class UWeaponDamageData;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Rogue GAS"))
class FPS_API URogueGASSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 대미지 캐리어 GE
	// URogueDamageSubsystem 이 스펙을 만들 때 사용
	UPROPERTY(Config, EditAnywhere, Category = "Damage")
	TSoftClassPtr<UGameplayEffect> DamageEffectClass;

	// 무기별 대미지 계수(기본 피해 / 거리 감쇠 / 부위 배율).
	// ExecCalc_Damage 가 WeaponTag 로 조회
	UPROPERTY(Config, EditAnywhere, Category = "Damage")
	TSoftObjectPtr<UWeaponDamageData> WeaponDamageData;

	// 매치 시작에 한 번 부여할 어빌리티 목록.
	// AShooterPlayerState 가 최초 아바타 연결 시 한 번만 부여
	UPROPERTY(Config, EditAnywhere, Category = "Ability")
	TSoftObjectPtr<URogueAbilitySet> StartupAbilitySet;
};
