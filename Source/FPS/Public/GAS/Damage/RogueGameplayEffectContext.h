// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GAS/Damage/RogueDamageTypes.h"

#include "RogueGameplayEffectContext.generated.h"

USTRUCT()
struct FPS_API FRogueGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

public:
	UPROPERTY() FGameplayTag SourceTag;
	UPROPERTY() FGameplayTag WeaponTag;
	UPROPERTY() FGameplayTag HitZoneTag;
	UPROPERTY() FGameplayTagContainer ContextTags;
	UPROPERTY() float HitDistanceMeters = 0.f;
	UPROPERTY() int32 PelletsHit = 1;
	UPROPERTY() int32 ShotIndexInMagazine = 0;

	// 거리/관통까지 계산된 부위별 원본 피해와 무기 부위 배율.
	// ExecCalc_Damage가 이 배열을 사용해 부위 배율을 적용한다.
	UPROPERTY() TArray<FRogueHitZoneDamagePart> HitZoneDamageParts;

	// 기존 DamageRequest 호출자 호환용. 신규 Hitscan은 HitZoneDamageParts를 사용한다.
	UPROPERTY() float HeadshotDamage = 0.f;
	UPROPERTY() float AppliedHeadshotMultiplier = 1.f;

	UPROPERTY() int32 ReflectDepth = 0;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FRogueGameplayEffectContext::StaticStruct();
	}

	virtual FRogueGameplayEffectContext* Duplicate() const override
	{
		FRogueGameplayEffectContext* NewContext = new FRogueGameplayEffectContext(*this);
		if (GetHitResult())
		{
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	static const FRogueGameplayEffectContext* ExtractFrom(const FGameplayEffectContextHandle& Handle);
};

template<>
struct TStructOpsTypeTraits<FRogueGameplayEffectContext>
	: public TStructOpsTypeTraitsBase2<FRogueGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
