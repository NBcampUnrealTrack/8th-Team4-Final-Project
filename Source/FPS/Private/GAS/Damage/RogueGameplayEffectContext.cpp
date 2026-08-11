// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Damage/RogueGameplayEffectContext.h"

namespace
{
	constexpr int32 MaxSerializedHitZoneParts = 16;

	void NetSerializeHitZonePart(
		FArchive& Ar,
		UPackageMap* Map,
		FRogueHitZoneDamagePart& Part,
		bool& bInOutSuccess)
	{
		bool bTagSuccess = true;
		Part.HitZoneTag.NetSerialize(Ar, Map, bTagSuccess);
		bInOutSuccess &= bTagSuccess;

		Ar << Part.BaseDamage;
		Ar << Part.HitZoneMultiplier;
		Ar << Part.PelletsHit;

		uint8 HeadshotBit = Part.bHeadshot ? 1 : 0;
		Ar.SerializeBits(&HeadshotBit, 1);
		if (Ar.IsLoading())
		{
			Part.bHeadshot = HeadshotBit != 0;
		}
	}
}

bool FRogueGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuccess = true;
	bool bSuperSuccess = true;
	Super::NetSerialize(Ar, Map, bSuperSuccess);
	bSuccess &= bSuperSuccess;

	auto SerializeTag = [&Ar, Map, &bSuccess](FGameplayTag& Tag)
	{
		bool bTagSuccess = true;
		Tag.NetSerialize(Ar, Map, bTagSuccess);
		bSuccess &= bTagSuccess;
	};

	SerializeTag(SourceTag);
	SerializeTag(WeaponTag);
	SerializeTag(HitZoneTag);

	bool bContainerSuccess = true;
	ContextTags.NetSerialize(Ar, Map, bContainerSuccess);
	bSuccess &= bContainerSuccess;

	Ar << HitDistanceMeters;
	Ar << PelletsHit;
	Ar << ShotIndexInMagazine;

	uint8 PartCount = Ar.IsSaving()
		? static_cast<uint8>(FMath::Min(HitZoneDamageParts.Num(), MaxSerializedHitZoneParts))
		: 0;
	Ar << PartCount;

	if (Ar.IsLoading())
	{
		HitZoneDamageParts.SetNum(PartCount);
	}

	for (int32 Index = 0; Index < static_cast<int32>(PartCount); ++Index)
	{
		NetSerializeHitZonePart(Ar, Map, HitZoneDamageParts[Index], bSuccess);
	}

	// 레거시 호출자 호환 데이터.
	Ar << HeadshotDamage;
	Ar << AppliedHeadshotMultiplier;
	Ar << ReflectDepth;

	bOutSuccess = bSuccess && !Ar.IsError();
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
