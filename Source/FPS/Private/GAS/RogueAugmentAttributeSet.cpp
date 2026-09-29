// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueAugmentAttributeSet.h"
#include "Combat/CombatComponent.h"
#include "GameplayEffectExtension.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/WeaponBase.h"

URogueAugmentAttributeSet::URogueAugmentAttributeSet()
{
	// 배율 계열: 1.0
	InitDamageMultiplier(1.f);
	InitSkillDamageMultiplier(1.f);
	InitHeadshotDamageMultiplier(1.f);
	InitShieldDamageMultiplier(1.f);
	InitHeadshotDamageTakenRate(1.f);
	InitFireRateMultiplier(1.f);
	InitReloadSpeedMultiplier(1.f);
	InitMagazineSizeMultiplier(1.f);
	InitPenetrationMultiplier(1.f);
	InitSpreadMultiplier(1.f);
	InitRecoilMultiplier(1.f);
	InitMoveSpeedMultiplier(1.f);
	InitSprintSpeedMultiplier(1.f);
	InitJumpHeightMultiplier(1.f);
	InitCreditGainMultiplier(1.f);
	InitShopCostMultiplier(1.f);
	InitRareOrAboveDrawMultiplier(1.f);

	// 비율 / 가산 계열: 0.0
	InitShieldPenetrationRate(0.f);
	InitMaxHealthPercentDamage(0.f);
	InitLifestealRate(0.f);
	InitDamageReductionRate(0.f);
	InitFlatDamageReduction(0.f);
	InitDamageReflectRate(0.f);
	InitPelletCountBonus(0.f);
	InitEffectiveRangeBonus(0.f);
	InitMeleeRangeBonus(0.f);
	InitExtraJumpCount(0.f);
	InitCooldownReductionRate(0.f);
	InitBonusFreeRerolls(0.f);
	InitLostHealthBonusRate(0.f);
	InitCooldownRefundRate(0.f);
	InitAmmoRefund(0.f);
	InitHighAmmoFireRateBonus(0.f);
}

void URogueAugmentAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 이동 관련은 원격 클라이언트에게도 필요
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, MoveSpeedMultiplier,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, SprintSpeedMultiplier,   COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, JumpHeightMultiplier,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, ExtraJumpCount,          COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, LostHealthBonusRate,     COND_None, REPNOTIFY_Always);

	// 나머지는 오너에게만 필요
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, DamageMultiplier,          COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, SkillDamageMultiplier,     COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, HeadshotDamageMultiplier,  COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, ShieldPenetrationRate,     COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, ShieldDamageMultiplier,    COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, MaxHealthPercentDamage,    COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, LifestealRate,             COND_OwnerOnly, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, DamageReductionRate,       COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, FlatDamageReduction,       COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, HeadshotDamageTakenRate,   COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, DamageReflectRate,         COND_OwnerOnly, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, FireRateMultiplier,        COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, HighAmmoFireRateBonus,     COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, ReloadSpeedMultiplier,     COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, MagazineSizeMultiplier,    COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, PelletCountBonus,          COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, EffectiveRangeBonus,       COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, PenetrationMultiplier,     COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, MeleeRangeBonus,           COND_OwnerOnly, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, SpreadMultiplier,          COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, RecoilMultiplier,          COND_OwnerOnly, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, CooldownReductionRate,     COND_OwnerOnly, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, CreditGainMultiplier,      COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, ShopCostMultiplier,        COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, RareOrAboveDrawMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueAugmentAttributeSet, BonusFreeRerolls,          COND_OwnerOnly, REPNOTIFY_Always);
}

void URogueAugmentAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 1.0 을 허용하면 게임이 성립하지 않는 축들. 0 ~ 0.95
	// 체력 비례 데미지, 데미지 감소율, 쿨타임 감소율
	if (Attribute == GetMaxHealthPercentDamageAttribute()
	 || Attribute == GetDamageReductionRateAttribute()
	 || Attribute == GetCooldownReductionRateAttribute()
	 )
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 0.95f);
		return;
	}

	// 1.0 이어도 성립하는 축들. 0 ~ 1.0
	// 방어구 관통률, 생명력 흡수율, 데미지 반사율
	if(Attribute == GetShieldPenetrationRateAttribute()
	|| Attribute == GetLifestealRateAttribute()
	|| Attribute == GetDamageReflectRateAttribute()
	|| Attribute == GetLostHealthBonusRateAttribute()
	|| Attribute == GetHighAmmoFireRateBonusAttribute()
	|| Attribute == GetCooldownRefundRateAttribute()
	|| Attribute == GetHeadshotDamageTakenRateAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
		return;
	}

	// 가산 계열은 음수 방지
	// 데미지 감소량, 산탄 수 증가량, 유효 사거리 증가량, 근접 사거리 증가량,
	// 추가 점프 횟수, 무료 재시도 횟수
	if (Attribute == GetFlatDamageReductionAttribute()
	 || Attribute == GetPelletCountBonusAttribute()
	 || Attribute == GetEffectiveRangeBonusAttribute()
	 || Attribute == GetMeleeRangeBonusAttribute()
	 || Attribute == GetExtraJumpCountAttribute()
	 || Attribute == GetAmmoRefundAttribute()
	 || Attribute == GetBonusFreeRerollsAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
		return;
	}

	// 배율 계열은 0.01 ~ 10.0
	// 0이 되면 나누기 연산에서 오류가 생길 수 있음
	if (Attribute == GetDamageMultiplierAttribute()
	 || Attribute == GetSkillDamageMultiplierAttribute()
	 || Attribute == GetHeadshotDamageMultiplierAttribute()
	 || Attribute == GetShieldDamageMultiplierAttribute()
	 || Attribute == GetFireRateMultiplierAttribute()
	 || Attribute == GetReloadSpeedMultiplierAttribute()
	 || Attribute == GetMagazineSizeMultiplierAttribute()
	 || Attribute == GetPenetrationMultiplierAttribute()
	 || Attribute == GetSpreadMultiplierAttribute()
	 || Attribute == GetRecoilMultiplierAttribute()
	 || Attribute == GetMoveSpeedMultiplierAttribute()
	 || Attribute == GetSprintSpeedMultiplierAttribute()
	 || Attribute == GetJumpHeightMultiplierAttribute()
	 || Attribute == GetCreditGainMultiplierAttribute()
	 || Attribute == GetShopCostMultiplierAttribute()
	 || Attribute == GetRareOrAboveDrawMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.01f, 10.f);
		return;
	}

	// 위 네 버킷에 포함되지 않는 축
	ensureMsgf(false,
		TEXT("[RogueAugment] PreAttributeChange 에 등록되지 않은 어트리뷰트입니다: %s. "
		     "정의역 버킷 네 개 중 하나에 추가하세요."),
		*Attribute.GetName());
}

void URogueAugmentAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 남은 액티브 쿨타임 감소
	if (Data.EvaluatedData.Attribute == GetCooldownRefundRateAttribute())
	{
		const float Rate = GetCooldownRefundRate();
		SetCooldownRefundRate(0.f);

		if (URogueAbilitySystemComponent* ASC = Cast<URogueAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			ASC->ReduceActiveCooldowns(Rate);
		}
	}

	// 장착 무기 탄창에 탄 추가
	if (Data.EvaluatedData.Attribute == GetAmmoRefundAttribute())
	{
		const int32 Amount = FMath::FloorToInt(GetAmmoRefund());
		SetAmmoRefund(0.f);

		const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
		const AActor* Avatar = ASC ? ASC->GetAvatarActor() : nullptr;
		const UCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UCombatComponent>() : nullptr;
		if (AWeaponBase* Weapon = Combat ? Combat->GetEquippedWeapon() : nullptr)
		{
			Weapon->AddMagazineAmmo(Amount);
		}
	}
}

// RepNotify 는 매크로로 정의
#define ROGUE_AUG_REPNOTIFY(PropertyName)                                                    \
	void URogueAugmentAttributeSet::OnRep_##PropertyName(const FGameplayAttributeData& Old)  \
	{                                                                                        \
		GAMEPLAYATTRIBUTE_REPNOTIFY(URogueAugmentAttributeSet, PropertyName, Old);           \
	}

ROGUE_AUG_REPNOTIFY(DamageMultiplier)
ROGUE_AUG_REPNOTIFY(SkillDamageMultiplier)
ROGUE_AUG_REPNOTIFY(HeadshotDamageMultiplier)
ROGUE_AUG_REPNOTIFY(ShieldPenetrationRate)
ROGUE_AUG_REPNOTIFY(ShieldDamageMultiplier)
ROGUE_AUG_REPNOTIFY(MaxHealthPercentDamage)
ROGUE_AUG_REPNOTIFY(LifestealRate)

ROGUE_AUG_REPNOTIFY(DamageReductionRate)
ROGUE_AUG_REPNOTIFY(FlatDamageReduction)
ROGUE_AUG_REPNOTIFY(HeadshotDamageTakenRate)
ROGUE_AUG_REPNOTIFY(DamageReflectRate)

ROGUE_AUG_REPNOTIFY(FireRateMultiplier)
ROGUE_AUG_REPNOTIFY(ReloadSpeedMultiplier)
ROGUE_AUG_REPNOTIFY(MagazineSizeMultiplier)
ROGUE_AUG_REPNOTIFY(PelletCountBonus)
ROGUE_AUG_REPNOTIFY(EffectiveRangeBonus)
ROGUE_AUG_REPNOTIFY(PenetrationMultiplier)
ROGUE_AUG_REPNOTIFY(MeleeRangeBonus)

ROGUE_AUG_REPNOTIFY(SpreadMultiplier)
ROGUE_AUG_REPNOTIFY(RecoilMultiplier)

ROGUE_AUG_REPNOTIFY(MoveSpeedMultiplier)
ROGUE_AUG_REPNOTIFY(SprintSpeedMultiplier)
ROGUE_AUG_REPNOTIFY(JumpHeightMultiplier)
ROGUE_AUG_REPNOTIFY(ExtraJumpCount)
ROGUE_AUG_REPNOTIFY(LostHealthBonusRate)
ROGUE_AUG_REPNOTIFY(HighAmmoFireRateBonus)

ROGUE_AUG_REPNOTIFY(CooldownReductionRate)

ROGUE_AUG_REPNOTIFY(CreditGainMultiplier)
ROGUE_AUG_REPNOTIFY(ShopCostMultiplier)
ROGUE_AUG_REPNOTIFY(RareOrAboveDrawMultiplier)
ROGUE_AUG_REPNOTIFY(BonusFreeRerolls)

#undef ROGUE_AUG_REPNOTIFY
