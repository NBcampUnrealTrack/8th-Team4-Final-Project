// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "RogueGameplayEffectContext.generated.h"

USTRUCT()
struct FPS_API FRogueGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

public:
	// 데미지 종류 - Damage.Source.* 
	UPROPERTY() FGameplayTag SourceTag;

	// 무기 종류 - Weapon.Type.*
	UPROPERTY() FGameplayTag WeaponTag;

	// 피격 부위
	UPROPERTY() FGameplayTag HitZoneTag;

	// 공격의 정보를 담는 태그
	UPROPERTY() FGameplayTagContainer ContextTags;

	// 피격 거리
	UPROPERTY() float HitDistanceMeters = 0.f;

	// 이번 공격에서 맞은 총알 수
	UPROPERTY() int32 PelletsHit = 1;

	// 발사된 총알 인덱스 (첫 발, 마지막 발 등을 구분)
	UPROPERTY() int32 ShotIndexInMagazine = 0;

	// BaseDamage 중 머리에 맞은 몫. 헤드샷 증강을 이 부분에 적용
	UPROPERTY() float HeadshotDamage = 0.f;

	// 전투팀이 HeadshotDamage 에 곱한 무기 헤드 배율
	// 데미지를 복원할 때 사용
	UPROPERTY() float AppliedHeadshotMultiplier = 1.f;

	// 반사가 반사를 유발하는 무한 루프 차단
	UPROPERTY() int32 ReflectDepth = 0;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FRogueGameplayEffectContext::StaticStruct();
	}

	// 컨텍스트 복사를 위해 오버라이딩
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
		WithCopy          = true
	};
};
