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
	UPROPERTY() FGameplayTagContainer ContextTags;
	UPROPERTY() float HitDistanceMeters = 0.f;
	UPROPERTY() int32 ShotIndexInMagazine = 0;
	UPROPERTY() bool bHeadshot = false;
	UPROPERTY() TArray<FRogueHitGroup> HitGroups;
	UPROPERTY() FVector_NetQuantizeNormal HitShotDirection = FVector::ZeroVector;
	UPROPERTY() FName HitBoneName = NAME_None;

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
