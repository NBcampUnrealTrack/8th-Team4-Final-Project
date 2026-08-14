// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Damage/RogueGameplayEffectContext.h"

namespace
{
	void NetSerializeHitGroup(FArchive& Ar, UPackageMap* Map, FRogueHitGroup& Group, bool& bInOutSuccess)
	{
		bool bTagSuccess = true;
		Group.HitZoneTag.NetSerialize(Ar, Map, bTagSuccess);
		bInOutSuccess &= bTagSuccess;

		Ar << Group.WallRetentionSum;
		Ar << Group.PelletCount;
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

	bool bContainerSuccess = true;
	ContextTags.NetSerialize(Ar, Map, bContainerSuccess);
	bSuccess &= bContainerSuccess;

	Ar << HitDistanceMeters;
	Ar << ShotIndexInMagazine;

	uint8 HeadshotBit = bHeadshot ? 1 : 0;
	Ar.SerializeBits(&HeadshotBit, 1);
	if (Ar.IsLoading())
	{
		bHeadshot = HeadshotBit != 0;
	}

	uint8 GroupCount = Ar.IsSaving()
		? static_cast<uint8>(FMath::Min(HitGroups.Num(), RogueMaxHitGroups))
		: 0;
	Ar << GroupCount;

	if (Ar.IsLoading())
	{
		// 상한 HitGroup 개수를 초과하면 에러 처리
		if (GroupCount > RogueMaxHitGroups)
		{
			Ar.SetError();
			GroupCount = RogueMaxHitGroups;
		}

		HitGroups.SetNum(GroupCount);
	}

	for (int32 Index = 0; Index < static_cast<int32>(GroupCount); ++Index)
	{
		NetSerializeHitGroup(Ar, Map, HitGroups[Index], bSuccess);
	}

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
