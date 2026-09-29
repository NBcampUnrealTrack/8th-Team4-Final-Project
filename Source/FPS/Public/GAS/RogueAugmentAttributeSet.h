// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "RogueAugmentAttributeSet.generated.h"


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

	// 방어 관련
	UFUNCTION() void OnRep_DamageReductionRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FlatDamageReduction(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HeadshotDamageTakenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageReflectRate(const FGameplayAttributeData& OldValue);

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
	UFUNCTION() void OnRep_LostHealthBonusRate(const FGameplayAttributeData& OldValue);

	// 스킬 관련
	UFUNCTION() void OnRep_CooldownReductionRate(const FGameplayAttributeData& OldValue);

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
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,DamageMultiplier)

	// 증강 스킬 피해 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SkillDamageMultiplier, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData SkillDamageMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,SkillDamageMultiplier)

	// 헤드샷 추가 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HeadshotDamageMultiplier, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData HeadshotDamageMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,HeadshotDamageMultiplier)

	// 보호막 관통률
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldPenetrationRate, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData ShieldPenetrationRate;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,ShieldPenetrationRate)

	// 보호막에 주는 피해 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShieldDamageMultiplier, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData ShieldDamageMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,ShieldDamageMultiplier)

	// 대상 최대 체력 비례 추가 피해
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealthPercentDamage, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData MaxHealthPercentDamage;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,MaxHealthPercentDamage)

	// 준 피해의 이 비율만큼 자신을 회복
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LifestealRate, Category = "Rogue|Augment|Offense")
	FGameplayAttributeData LifestealRate;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,LifestealRate)

	// 방어 관련 --------------------------------------------------
	// 받는 피해 감소 비율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageReductionRate, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData DamageReductionRate;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,DamageReductionRate)

	// 받는 피해 고정 감소. 비율 감소 이후 적용
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FlatDamageReduction, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData FlatDamageReduction;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,FlatDamageReduction)

	// 헤드샷 보너스 배율
	// 0 이면 상체 피격과 동일
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HeadshotDamageTakenRate, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData HeadshotDamageTakenRate;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,HeadshotDamageTakenRate)

	// 반사 피해 비율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageReflectRate, Category = "Rogue|Augment|Defense")
	FGameplayAttributeData DamageReflectRate;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,DamageReflectRate)

	// 무기 관련 --------------------------------------------------
	// 연사 속도 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireRateMultiplier, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData FireRateMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,FireRateMultiplier)

	// 재장전 속도 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadSpeedMultiplier, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData ReloadSpeedMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,ReloadSpeedMultiplier)

	// 탄창 용량 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagazineSizeMultiplier, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData MagazineSizeMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,MagazineSizeMultiplier)

	// 산탄총 펠릿 추가 수
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PelletCountBonus, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData PelletCountBonus;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,PelletCountBonus)

	// 유효 사거리 가산 (m)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EffectiveRangeBonus, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData EffectiveRangeBonus;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,EffectiveRangeBonus)

	// 관통력 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PenetrationMultiplier, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData PenetrationMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,PenetrationMultiplier)

	// 근접 무기 사거리 가산 (m)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MeleeRangeBonus, Category = "Rogue|Augment|Weapon")
	FGameplayAttributeData MeleeRangeBonus;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,MeleeRangeBonus)

	// 탄퍼짐 배율. 조준 중에는 적용 X
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpreadMultiplier, Category = "Rogue|Augment|Accuracy")
	FGameplayAttributeData SpreadMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,SpreadMultiplier)

	// 반동 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RecoilMultiplier, Category = "Rogue|Augment|Accuracy")
	FGameplayAttributeData RecoilMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,RecoilMultiplier)

	// 이동 관련 --------------------------------------------------
	// 이동 속도 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedMultiplier, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData MoveSpeedMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,MoveSpeedMultiplier)

	// 전력 질주 속도 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SprintSpeedMultiplier, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData SprintSpeedMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,SprintSpeedMultiplier)

	// 점프 높이 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JumpHeightMultiplier, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData JumpHeightMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,JumpHeightMultiplier)

	// 추가 점프 횟수 (이단 점프 증강)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ExtraJumpCount, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData ExtraJumpCount;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,ExtraJumpCount)

	// 잃은 체력 단계당 무기 피해, 연사, 이동 증가율 (광전사의 영혼)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LostHealthBonusRate, Category = "Rogue|Augment|Movement")
	FGameplayAttributeData LostHealthBonusRate;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,LostHealthBonusRate)

	// 스킬 관련 --------------------------------------------------
	// 액티브 증강 쿨타임 감소 비율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CooldownReductionRate, Category = "Rogue|Augment|Skill")
	FGameplayAttributeData CooldownReductionRate;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,CooldownReductionRate)

	// 상점 관련 --------------------------------------------------
	// 크레딧 획득 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CreditGainMultiplier, Category = "Rogue|Augment|Meta")
	FGameplayAttributeData CreditGainMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,CreditGainMultiplier)

	// 상점 가격 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShopCostMultiplier, Category = "Rogue|Augment|Meta")
	FGameplayAttributeData ShopCostMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,ShopCostMultiplier)

	// 희귀 이상 증강 출현 확률 배율
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RareOrAboveDrawMultiplier, Category = "Rogue|Augment|Meta")
	FGameplayAttributeData RareOrAboveDrawMultiplier;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,RareOrAboveDrawMultiplier)

	// 추가 무료 리롤 횟수
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BonusFreeRerolls, Category = "Rogue|Augment|Meta")
	FGameplayAttributeData BonusFreeRerolls;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAugmentAttributeSet,BonusFreeRerolls)
};
