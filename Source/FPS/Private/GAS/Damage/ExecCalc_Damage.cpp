// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Damage/ExecCalc_Damage.h"

#include "AbilitySystemComponent.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Tags/GameplayTags.h"

namespace
{
	// 연산이 성립하기 위한 정의역 제한.
	// 수식이 깨지지 않게 하는 최소 조건
	constexpr float MaxDodgeChance    = 1.f;     // 확률이므로 0~1
	constexpr float MaxPenetration    = 1.f;     // 비율이므로 0~1
	constexpr float MaxReductionRate  = 0.95f;   // 1.0 이면 무적이 되어 PVP 가 성립하지 않는다

	struct FRogueDamageCapture
	{
		// Target / Vital
		DECLARE_ATTRIBUTE_CAPTUREDEF(Shield);

		// Source / Augment
		DECLARE_ATTRIBUTE_CAPTUREDEF(ShieldPenetrationRate);

		// Target / Augment
		DECLARE_ATTRIBUTE_CAPTUREDEF(DodgeChance);
		DECLARE_ATTRIBUTE_CAPTUREDEF(DamageReductionRate);
		DECLARE_ATTRIBUTE_CAPTUREDEF(FlatDamageReduction);

		FRogueDamageCapture()
		{
			// 적중 시점에 증강 배율을 적용을 위해 bSnapshot = false로 
			DEFINE_ATTRIBUTE_CAPTUREDEF(URogueVitalAttributeSet,   Shield,                Target, false);

			DEFINE_ATTRIBUTE_CAPTUREDEF(URogueAugmentAttributeSet, ShieldPenetrationRate, Source, false);

			DEFINE_ATTRIBUTE_CAPTUREDEF(URogueAugmentAttributeSet, DodgeChance,           Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(URogueAugmentAttributeSet, DamageReductionRate,   Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(URogueAugmentAttributeSet, FlatDamageReduction,   Target, false);
		}
	};

	const FRogueDamageCapture& Capture()
	{
		static FRogueDamageCapture Instance;
		return Instance;
	}
}

UExecCalc_Damage::UExecCalc_Damage()
{
	const FRogueDamageCapture& C = Capture();

	RelevantAttributesToCapture.Add(C.ShieldDef);
	RelevantAttributesToCapture.Add(C.ShieldPenetrationRateDef);
	RelevantAttributesToCapture.Add(C.DodgeChanceDef);
	RelevantAttributesToCapture.Add(C.DamageReductionRateDef);
	RelevantAttributesToCapture.Add(C.FlatDamageReductionDef);
}

void UExecCalc_Damage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FRogueDamageCapture& C = Capture();
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	float Damage = Spec.GetSetByCallerMagnitude(RogueTags::Data_Damage_Base, false, 0.f);
	if (Damage <= 0.f)
	{
		return;
	}

	FAggregatorEvaluateParameters Eval;
	Eval.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Eval.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 커스텀 컨텍스트가 설정이 안되어 있으면 증강 데이터가 들어오지 않음
	const FRogueGameplayEffectContext* Ctx =
		FRogueGameplayEffectContext::ExtractFrom(Spec.GetContext());
	if (!Ctx)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RogueDamage] 커스텀 EffectContext 가 없습니다. "
			     "DefaultGame.ini 의 AbilitySystemGlobalsClassName 등록을 확인하세요. "
			     "출처 구분 없이 무기 피해로 처리합니다."));
	}

	const FGameplayTag SourceTag = Ctx ? Ctx->SourceTag : RogueTags::Damage_Source_Weapon;
	const bool bIsSelfCost = (SourceTag == RogueTags::Damage_Source_SelfCost);

	// 회피 판정
	if (!bIsSelfCost)
	{
		float DodgeChance = 0.f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.DodgeChanceDef, Eval, DodgeChance);
		DodgeChance = FMath::Clamp(DodgeChance, 0.f, MaxDodgeChance);

		if (DodgeChance > 0.f && FMath::FRand() < DodgeChance)
		{
			return;
		}
	}

	// 보호막 관통 분리
	float Penetration = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.ShieldPenetrationRateDef, Eval, Penetration);
	Penetration = FMath::Clamp(Penetration, 0.f, MaxPenetration);

	float       ShieldDamage = Damage * (1.f - Penetration);
	const float PierceDamage = Damage * Penetration;

	//  보호막 차감
	float CurrentShield = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.ShieldDef, Eval, CurrentShield);
	CurrentShield = FMath::Max(0.f, CurrentShield);

	// 보호막이 흡수하지 못하고 넘친 몫은 체력으로
	const float Overkill = FMath::Max(0.f, ShieldDamage - CurrentShield);
	ShieldDamage = FMath::Min(ShieldDamage, CurrentShield);

	// 방어 측 경감
	float HealthDamage = PierceDamage + Overkill;

	// 자가 피해는 경감 X
	if (!bIsSelfCost)
	{
		float ReductionRate = 0.f;
		float FlatReduction = 0.f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.DamageReductionRateDef, Eval, ReductionRate);
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(C.FlatDamageReductionDef, Eval, FlatReduction);

		HealthDamage *= (1.f - FMath::Clamp(ReductionRate, 0.f, MaxReductionRate));

		// 고정 감소는 비율 감소 이후
		HealthDamage = FMath::Max(0.f, HealthDamage - FMath::Max(0.f, FlatReduction));
	}

	// 최종 피해량 적용
	if (ShieldDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			URogueVitalAttributeSet::GetIncomingShieldDamageAttribute(),
			EGameplayModOp::Additive, ShieldDamage));
	}

	if (HealthDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			URogueVitalAttributeSet::GetIncomingDamageAttribute(),
			EGameplayModOp::Additive, HealthDamage));
	}
}
