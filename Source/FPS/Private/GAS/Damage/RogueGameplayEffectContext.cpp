// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Damage/RogueGameplayEffectContext.h"

bool FRogueGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	SourceTag.NetSerialize(Ar, Map, bOutSuccess);
	WeaponTag.NetSerialize(Ar, Map, bOutSuccess);
	HitZoneTag.NetSerialize(Ar, Map, bOutSuccess);
	ContextTags.NetSerialize(Ar, Map, bOutSuccess);

	// 필드 추가 시 직렬화도 추가
	// 직렬화되지 않으면 서버와 클라이언트 간에 값이 달라질	수 있음
	Ar << HitDistanceMeters;
	Ar << PelletsHit;
	Ar << ShotIndexInMagazine;
	Ar << HeadshotDamage;
	Ar << AppliedHeadshotMultiplier;
	Ar << ReflectDepth;

	bOutSuccess = true;
	return true;
}

const FRogueGameplayEffectContext* FRogueGameplayEffectContext::ExtractFrom(
	const FGameplayEffectContextHandle& Handle)
{
	const FGameplayEffectContext* Raw = Handle.Get();
	if (!Raw)
	{
		return nullptr;
	}

	if (Raw->GetScriptStruct() != FRogueGameplayEffectContext::StaticStruct())
	{
		return nullptr;
	}

	return static_cast<const FRogueGameplayEffectContext*>(Raw);
}
