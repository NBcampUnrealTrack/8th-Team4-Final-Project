// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "RogueAugmentAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class FPS_API URogueAugmentAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	URogueAugmentAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 다중 증강 중첩 시 수치 폭발 방지
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

protected:
	// 공격 관련
	UFUNCTION() void OnRep_DamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SkillDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HeadshotDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ShieldPenetrationRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ShieldDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealthPercentDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_LifestealRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ShieldLeechRate(const FGameplayAttributeData& OldValue);

	// 방어 관련
	UFUNCTION() void OnRep_DamageReductionRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FlatDamageReduction(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HeadshotDamageTakenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DodgeChance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageReflectRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealingReceivedMultiplier(const FGameplayAttributeData& OldValue);

	// 무기 관련
	UFUNCTION() void OnRep_FireRateMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ReloadSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MagazineSizeMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_PelletCountBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_EffectiveRangeBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_PenetrationMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MeleeRangeBonus(const FGameplayAttributeData& OldValue);

	// 총기 관련
	UFUNCTION() void OnRep_SpreadMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_RecoilMultiplier(const FGameplayAttributeData& OldValue);

	// 이동 관련
	UFUNCTION() void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SprintSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JumpHeightMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ExtraJumpCount(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FootstepVolumeMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HearingRangeMultiplier(const FGameplayAttributeData& OldValue);

	// 스킬 관련
	UFUNCTION() void OnRep_CooldownReductionRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ThrowRangeMultiplier(const FGameplayAttributeData& OldValue);

	// 상점 관련
	UFUNCTION() void OnRep_CreditGainMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ShopCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_RareOrAboveDrawMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_BonusFreeRerolls(const FGameplayAttributeData& OldValue);

public:
	// 공격 관련 --------------------------------------------------
	// 무기 피해 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageMultiplier, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData DamageMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, DamageMultiplier)

	// 증강 스킬 피해 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SkillDamageMultiplier, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData SkillDamageMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, SkillDamageMultiplier)

	// 헤드샷 추가 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HeadshotDamageMultiplier, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData HeadshotDamageMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, HeadshotDamageMultiplier)

	// 보호막 관통률
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldPenetrationRate, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData ShieldPenetrationRate;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, ShieldPenetrationRate)

	// 보호막에 주는 피해 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldDamageMultiplier, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData ShieldDamageMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, ShieldDamageMultiplier)

	// 대상 최대 체력 비례 추가 피해
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealthPercentDamage, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData MaxHealthPercentDamage;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, MaxHealthPercentDamage)

	// 준 피해의 이 비율만큼 자신을 회복
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LifestealRate, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData LifestealRate;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, LifestealRate)

	// 준 피해의 이 비율만큼 보호막으로 전환. 체력 흡수와 별개로 적용
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldLeechRate, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData ShieldLeechRate;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, ShieldLeechRate)

	// 방어 관련 --------------------------------------------------
	// 받는 피해 감소 비율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageReductionRate, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData DamageReductionRate;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, DamageReductionRate)

	// 받는 피해 고정 감소. 비율 감소 이후 적용
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FlatDamageReduction, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData FlatDamageReduction;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, FlatDamageReduction)

	// 받는 헤드샷 피해 배율. 1 미만이면 헤드샷 경감
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HeadshotDamageTakenRate, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData HeadshotDamageTakenRate;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, HeadshotDamageTakenRate)

	// 회피 확률
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DodgeChance, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData DodgeChance;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, DodgeChance)

	// 반사 피해 비율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageReflectRate, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData DamageReflectRate;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, DamageReflectRate)

	// 받는 회복량 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingReceivedMultiplier, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData HealingReceivedMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, HealingReceivedMultiplier)

	// 무기 관련 --------------------------------------------------
	// 연사 속도 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireRateMultiplier, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData FireRateMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, FireRateMultiplier)

	// 재장전 속도 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadSpeedMultiplier, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData ReloadSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, ReloadSpeedMultiplier)

	// 탄창 용량 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagazineSizeMultiplier, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData MagazineSizeMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, MagazineSizeMultiplier)

	// 산탄총 펠릿 추가 수
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PelletCountBonus, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData PelletCountBonus;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, PelletCountBonus)

	// 유효 사거리 가산 (m)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EffectiveRangeBonus, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData EffectiveRangeBonus;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, EffectiveRangeBonus)

	// 관통력 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PenetrationMultiplier, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData PenetrationMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, PenetrationMultiplier)

	// 근접 무기 사거리 가산 (m)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MeleeRangeBonus, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData MeleeRangeBonus;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, MeleeRangeBonus)

	// 탄퍼짐 배율. 조준 중에는 적용 X
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpreadMultiplier, Category = "Rogue|Augment|Accuracy")
	FGameplayAttributeData SpreadMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, SpreadMultiplier)

	// 반동 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RecoilMultiplier, Category = "Rogue|Augment|Accuracy")
	FGameplayAttributeData RecoilMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, RecoilMultiplier)

	// 이동 관련 --------------------------------------------------
	// 이동 속도 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedMultiplier, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData MoveSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, MoveSpeedMultiplier)

	// 전력 질주 속도 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SprintSpeedMultiplier, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData SprintSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, SprintSpeedMultiplier)

	// 점프 높이 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JumpHeightMultiplier, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData JumpHeightMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, JumpHeightMultiplier)

	// 추가 점프 횟수 (이단 점프 증강)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExtraJumpCount, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData ExtraJumpCount;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, ExtraJumpCount)

	// 내가 내는 발소리 크기 배율. 1 미만이면 은신
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FootstepVolumeMultiplier, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData FootstepVolumeMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, FootstepVolumeMultiplier)

	// 내가 남의 소리를 듣는 범위 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HearingRangeMultiplier, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData HearingRangeMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, HearingRangeMultiplier)

	// 스킬 관련 --------------------------------------------------
	// 액티브 증강 쿨타임 감소 비율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CooldownReductionRate, Category = "Rogue|Augment|Skill")
	FGameplayAttributeData CooldownReductionRate;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, CooldownReductionRate)

	// 투척물 투척 거리 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ThrowRangeMultiplier, Category = "Rogue|Augment|Skill")
	FGameplayAttributeData ThrowRangeMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, ThrowRangeMultiplier)

	// 상점 관련 --------------------------------------------------
	// 크레딧 획득 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CreditGainMultiplier, Category = "Rogue|Augment|Meta")
	FGameplayAttributeData CreditGainMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, CreditGainMultiplier)

	// 상점 가격 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShopCostMultiplier, Category = "Rogue|Augment|Meta")
	FGameplayAttributeData ShopCostMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, ShopCostMultiplier)

	// 희귀 이상 증강 출현 확률 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RareOrAboveDrawMultiplier, Category = "Rogue|Augment|Meta")
	FGameplayAttributeData RareOrAboveDrawMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, RareOrAboveDrawMultiplier)

	// 추가 무료 리롤 횟수
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BonusFreeRerolls, Category = "Rogue|Augment|Meta")
	FGameplayAttributeData BonusFreeRerolls;
	ATTRIBUTE_ACCESSORS(URogueAugmentAttributeSet, BonusFreeRerolls)
};
