// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Damage/ExecCalc_Damage.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentTypes.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Tags/GameplayTags.h"

namespace
{
	// 연산이 성립하기 위한 정의역 제한.
	// 수식이 깨지지 않게 하는 최소 조건
	constexpr float MaxPenetration    = 1.f;     // 비율이므로 0~1
	constexpr float MaxReductionRate  = 0.95f;   // 1.0 이면 무적이 되어 PVP 가 성립하지 않는다

	FGameplayEffectAttributeCaptureDefinition MakeDef(
		const FGameplayAttribute& Attribute, EGameplayEffectAttributeCaptureSource CaptureSource)
	{
		return FGameplayEffectAttributeCaptureDefinition(Attribute, CaptureSource, false);
	}

	struct FRogueDamageCapture
	{
		using ECaptureSource = EGameplayEffectAttributeCaptureSource;

		// Vital
		FGameplayEffectAttributeCaptureDefinition TargetShieldDef;
		FGameplayEffectAttributeCaptureDefinition TargetHealthDef;
		FGameplayEffectAttributeCaptureDefinition TargetMaxHealthDef;
		FGameplayEffectAttributeCaptureDefinition SourceHealthDef;
		FGameplayEffectAttributeCaptureDefinition SourceMaxHealthDef;

		// Augment / 공격 측
		FGameplayEffectAttributeCaptureDefinition SourceHeadshotDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceSkillDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceShieldPenetrationRateDef;
		FGameplayEffectAttributeCaptureDefinition SourceShieldDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceMaxHealthPercentDamageDef;

		// Augment / 방어 측
		FGameplayEffectAttributeCaptureDefinition TargetHeadshotDamageTakenRateDef;
		FGameplayEffectAttributeCaptureDefinition TargetDamageReductionRateDef;
		FGameplayEffectAttributeCaptureDefinition TargetFlatDamageReductionDef;

		FRogueDamageCapture()
		{
			TargetShieldDef    = MakeDef(URogueVitalAttributeSet::GetShieldAttribute(),    ECaptureSource::Target);
			TargetHealthDef    = MakeDef(URogueVitalAttributeSet::GetHealthAttribute(),    ECaptureSource::Target);
			TargetMaxHealthDef = MakeDef(URogueVitalAttributeSet::GetMaxHealthAttribute(), ECaptureSource::Target);
			SourceHealthDef    = MakeDef(URogueVitalAttributeSet::GetHealthAttribute(),    ECaptureSource::Source);
			SourceMaxHealthDef = MakeDef(URogueVitalAttributeSet::GetMaxHealthAttribute(), ECaptureSource::Source);

			SourceHeadshotDamageMultiplierDef =
				MakeDef(URogueAugmentAttributeSet::GetHeadshotDamageMultiplierAttribute(), ECaptureSource::Source);
			SourceDamageMultiplierDef =
				MakeDef(URogueAugmentAttributeSet::GetDamageMultiplierAttribute(),       ECaptureSource::Source);
			SourceSkillDamageMultiplierDef =
				MakeDef(URogueAugmentAttributeSet::GetSkillDamageMultiplierAttribute(),  ECaptureSource::Source);
			SourceShieldPenetrationRateDef =
				MakeDef(URogueAugmentAttributeSet::GetShieldPenetrationRateAttribute(),  ECaptureSource::Source);
			SourceShieldDamageMultiplierDef =
				MakeDef(URogueAugmentAttributeSet::GetShieldDamageMultiplierAttribute(), ECaptureSource::Source);
			SourceMaxHealthPercentDamageDef =
				MakeDef(URogueAugmentAttributeSet::GetMaxHealthPercentDamageAttribute(), ECaptureSource::Source);

			TargetHeadshotDamageTakenRateDef =
				MakeDef(URogueAugmentAttributeSet::GetHeadshotDamageTakenRateAttribute(), ECaptureSource::Target);
			TargetDamageReductionRateDef =
				MakeDef(URogueAugmentAttributeSet::GetDamageReductionRateAttribute(),    ECaptureSource::Target);
			TargetFlatDamageReductionDef =
				MakeDef(URogueAugmentAttributeSet::GetFlatDamageReductionAttribute(),    ECaptureSource::Target);
		}
	};

	const FRogueDamageCapture& Capture()
	{
		static FRogueDamageCapture Instance;
		return Instance;
	}

	// 캡처 정의 및 결과 묶은 구조체
	struct FRogueDamageEval
	{
		const FGameplayEffectCustomExecutionParameters& Params;
		const FRogueDamageCapture&                      Capture;
		FAggregatorEvaluateParameters                   Aggregator;

		// 캡처 실패 시 Default 를 반환
		float Read(const FGameplayEffectAttributeCaptureDefinition& Def, float Default) const
		{
			float Value = Default;
			Params.AttemptCalculateCapturedAttributeMagnitude(Def, Aggregator, Value);
			return Value;
		}
	};

	// 이 히트의 성격
	struct FRogueDamageSource
	{
		FGameplayTag Tag;

		// 자가 피해 여부
		bool bIsSelfCost = false;

		// 적의 공격 여부
		bool bIsOffensive = false;
	};

	// 배율 및 고정값
	struct FRogueConditionalTotals
	{
		float DealtMultiplier = 1.f;
		float DealtFlat       = 0.f;
		float TakenMultiplier = 1.f;
		float TakenFlat       = 0.f;
		float HeadMultiplier  = 1.f;
		float HeadFlat        = 0.f;
	};

	// 규칙 판정에 필요한 히트 정보
	struct FRogueRuleContext
	{
		const FRogueGameplayEffectContext* Ctx = nullptr;

		float TargetHealthPct = 100.f;
		float SourceHealthPct = 100.f;
		bool  bTargetHealthKnown = false;
		bool  bSourceHealthKnown = false;
	};

	// 보호막, 체력 피해량
	struct FRogueDamageSplit
	{
		float ShieldDamage = 0.f;
		float HealthDamage = 0.f;
	};

	// --------------------------------------------------
	// 규칙 판정

	bool IsInRange(float Value, float Min, float Max)
	{
		return Value >= Min && Value <= Max;
	}

	bool DoesRuleApply(const FAugmentConditionalDamageRule& Rule, const FRogueRuleContext& RC)
	{
		if (!RC.Ctx)
		{
			return false;
		}

		if (!Rule.RequiredContextTags.IsEmpty() && !RC.Ctx->ContextTags.HasAll(Rule.RequiredContextTags))
		{
			return false;
		}

		if (!Rule.BlockedContextTags.IsEmpty() && RC.Ctx->ContextTags.HasAny(Rule.BlockedContextTags))
		{
			return false;
		}

		// 장착 무기가 아닌, 공격한 무기
		if (!Rule.RequiredWeaponTags.IsEmpty() && !Rule.RequiredWeaponTags.HasTag(RC.Ctx->WeaponTag))
		{
			return false;
		}

		if (Rule.bUseDistanceRange
			&& !IsInRange(RC.Ctx->HitDistanceMeters, Rule.MinDistanceMeters, Rule.MaxDistanceMeters))
		{
			return false;
		}

		// 체력, 보호막을 읽지 못한 경우 조건부 규칙을 적용 X
		if (Rule.bUseTargetHealthRange && RC.bTargetHealthKnown
			&& !IsInRange(RC.TargetHealthPct, Rule.TargetHealthMinPct, Rule.TargetHealthMaxPct))
		{
			return false;
		}

		if (Rule.bUseSourceHealthRange && RC.bSourceHealthKnown
			&& !IsInRange(RC.SourceHealthPct, Rule.SourceHealthMinPct, Rule.SourceHealthMaxPct))
		{
			return false;
		}

		if (Rule.bUseShotIndexRange
			&& !IsInRange(RC.Ctx->ShotIndexInMagazine, Rule.MinShotIndexInMagazine, Rule.MaxShotIndexInMagazine))
		{
			return false;
		}

		return true;
	}

	// 한쪽 인벤토리의 규칙을 훑어 합계에 누적
	void AccumulateRules(const UAugmentInventoryComponent* Inventory,
	                     EAugmentDamageRuleSide Side,
	                     const FRogueRuleContext& RC,
	                     FRogueConditionalTotals& Totals)
	{
		if (!Inventory)
		{
			return;
		}

		for (const FAugmentConditionalDamageRule& Rule : Inventory->GetConditionalDamageRules())
		{
			if (Rule.Side != Side)
			{
				continue;
			}

			if (!DoesRuleApply(Rule, RC))
			{
				continue;
			}

			// 헤드샷 보너스 계산
			if (Rule.bAppliesToHeadshotBonusOnly)
			{
				Totals.HeadMultiplier *= FMath::Max(0.f, Rule.DamageMultiplier);
				Totals.HeadFlat       += Rule.FlatDamageBonus;
			}
			else if (Side == EAugmentDamageRuleSide::Dealt)
			{
				Totals.DealtMultiplier *= FMath::Max(0.f, Rule.DamageMultiplier);
				Totals.DealtFlat       += Rule.FlatDamageBonus;
			}
			else
			{
				Totals.TakenMultiplier *= FMath::Max(0.f, Rule.DamageMultiplier);
				Totals.TakenFlat       += Rule.FlatDamageBonus;
			}
		}
	}

	// ── 단계별 계산 ───────────────────────────────────────────────────

	FRogueDamageSource ResolveSource(const FRogueGameplayEffectContext* Ctx)
	{
		FRogueDamageSource Source;
		Source.Tag = Ctx ? Ctx->SourceTag : RogueTags::Damage_Source_Weapon;

		Source.bIsSelfCost  = (Source.Tag == RogueTags::Damage_Source_SelfCost);
		Source.bIsOffensive = (Source.Tag == RogueTags::Damage_Source_Weapon)
			|| (Source.Tag == RogueTags::Damage_Source_Skill);

		return Source;
	}

	FRogueRuleContext MakeRuleContext(const FRogueDamageEval& E, const FRogueGameplayEffectContext* Ctx)
	{
		FRogueRuleContext RC;
		RC.Ctx = Ctx;

		const float TargetMaxHealth = E.Read(E.Capture.TargetMaxHealthDef, 0.f);
		if (TargetMaxHealth > 0.f)
		{
			RC.TargetHealthPct    = E.Read(E.Capture.TargetHealthDef, 0.f) / TargetMaxHealth * 100.f;
			RC.bTargetHealthKnown = true;
		}

		const float SourceMaxHealth = E.Read(E.Capture.SourceMaxHealthDef, 0.f);
		if (SourceMaxHealth > 0.f)
		{
			RC.SourceHealthPct    = E.Read(E.Capture.SourceHealthDef, 0.f) / SourceMaxHealth * 100.f;
			RC.bSourceHealthKnown = true;
		}

		return RC;
	}

	// 가해자, 피해자 인벤토리의 조건부 규칙 합계
	FRogueConditionalTotals GatherConditionalTotals(const FRogueDamageEval& E,
	                                                const FRogueRuleContext& RC,
	                                                const FRogueDamageSource& Source)
	{
		FRogueConditionalTotals Totals;

		if (Source.bIsOffensive)
		{
			AccumulateRules(UAugmentInventoryComponent::FindInventory(E.Params.GetSourceAbilitySystemComponent()),
				EAugmentDamageRuleSide::Dealt, RC, Totals);
		}

		if (!Source.bIsSelfCost)
		{
			AccumulateRules(UAugmentInventoryComponent::FindInventory(E.Params.GetTargetAbilitySystemComponent()),
				EAugmentDamageRuleSide::Taken, RC, Totals);
		}

		return Totals;
	}

	float ApplyHeadshotBonusModifiers(const FRogueDamageEval& E,
	                                  const FRogueDamageSource& Source,
	                                  const FRogueConditionalTotals& Totals,
	                                  const float OriginalBonus)
	{
		if (OriginalBonus <= 0.0f)
		{
			return 0.0f;
		}

		float Bonus = OriginalBonus;
		if (Source.bIsOffensive)
		{
			Bonus *= FMath::Max(0.f, E.Read(E.Capture.SourceHeadshotDamageMultiplierDef, 1.f));
		}

		if (!Source.bIsSelfCost)
		{
			Bonus *= FMath::Max(0.f, E.Read(E.Capture.TargetHeadshotDamageTakenRateDef, 1.f));
		}

		return FMath::Max(0.f, Bonus * Totals.HeadMultiplier + Totals.HeadFlat);
	}

	// 신규 Hitscan 계약: BaseDamage는 거리/관통까지만 적용된 값이다.
	// 부위 배율과 헤드샷 보너스 증강은 ExecCalc에서 한 번만 적용한다.
	float ApplyHitZoneCoefficients(const FRogueDamageEval& E,
	                               const FRogueDamageSource& Source,
	                               const FRogueGameplayEffectContext* Ctx,
	                               const FRogueConditionalTotals& Totals,
	                               float Damage)
	{
		if (!Ctx)
		{
			return Damage;
		}

		if (!Ctx->HitZoneDamageParts.IsEmpty())
		{
			float TotalPartBaseDamage = 0.0f;
			for (const FRogueHitZoneDamagePart& Part : Ctx->HitZoneDamageParts)
			{
				if (FMath::IsFinite(Part.BaseDamage) && Part.BaseDamage > 0.0f)
				{
					TotalPartBaseDamage += Part.BaseDamage;
				}
			}

			if (TotalPartBaseDamage <= KINDA_SMALL_NUMBER)
			{
				return Damage;
			}

			// 잘못된 요청이 부위 배열로 BaseDamage를 부풀리지 못하도록 전체 합을 SetByCaller BaseDamage에 맞춘다.
			const float PartScale = TotalPartBaseDamage > Damage + KINDA_SMALL_NUMBER
				? Damage / TotalPartBaseDamage
				: 1.0f;
			const float CoveredBaseDamage = FMath::Min(Damage, TotalPartBaseDamage * PartScale);
			float ZonedDamage = FMath::Max(0.0f, Damage - CoveredBaseDamage);
			float OriginalHeadshotBonus = 0.0f;

			for (const FRogueHitZoneDamagePart& Part : Ctx->HitZoneDamageParts)
			{
				if (!FMath::IsFinite(Part.BaseDamage)
					|| !FMath::IsFinite(Part.HitZoneMultiplier)
					|| Part.BaseDamage <= 0.0f
					|| Part.HitZoneMultiplier < 0.0f)
				{
					continue;
				}

				const float PartBaseDamage = Part.BaseDamage * PartScale;
				const float PartDamage = PartBaseDamage * FMath::Max(0.0f, Part.HitZoneMultiplier);
				if (!FMath::IsFinite(PartBaseDamage) || !FMath::IsFinite(PartDamage))
				{
					// 데이터 오류가 무한/NaN 피해로 번지지 않게 부위 보정 전체를 포기하고 원본 피해로 복귀한다.
					return Damage;
				}

				ZonedDamage += PartDamage;
				if (!FMath::IsFinite(ZonedDamage))
				{
					return Damage;
				}

				if (Part.bHeadshot)
				{
					OriginalHeadshotBonus += FMath::Max(0.0f, PartDamage - PartBaseDamage);
				}
			}

			if (OriginalHeadshotBonus > 0.0f)
			{
				const float AdjustedHeadshotBonus = ApplyHeadshotBonusModifiers(
					E, Source, Totals, OriginalHeadshotBonus);
				ZonedDamage += AdjustedHeadshotBonus - OriginalHeadshotBonus;
			}

			return FMath::Max(0.0f, ZonedDamage);
		}

		// 레거시 호출자 호환: 이미 헤드 배율이 적용된 HeadshotDamage를 역산해 보너스만 조정한다.
		if (Ctx->HeadshotDamage <= 0.f)
		{
			return Damage;
		}

		const float AppliedMultiplier = FMath::Max(KINDA_SMALL_NUMBER, Ctx->AppliedHeadshotMultiplier);
		const float TorsoEquivalent = Ctx->HeadshotDamage / AppliedMultiplier;
		const float OriginalBonus = Ctx->HeadshotDamage - TorsoEquivalent;
		if (OriginalBonus <= 0.f)
		{
			return Damage;
		}

		const float AdjustedBonus = ApplyHeadshotBonusModifiers(E, Source, Totals, OriginalBonus);
		return FMath::Max(0.f, Damage + (AdjustedBonus - OriginalBonus));
	}

	// 공격 측 배율
	float ApplyOffenseCoefficients(const FRogueDamageEval& E, const FRogueDamageSource& Source, float Damage)
	{
		if (!Source.bIsOffensive)
		{
			return Damage;
		}

		// 무기 피해 및 스킬 피해
		const FGameplayEffectAttributeCaptureDefinition& MultiplierDef =
			(Source.Tag == RogueTags::Damage_Source_Skill)
				? E.Capture.SourceSkillDamageMultiplierDef
				: E.Capture.SourceDamageMultiplierDef;

		Damage *= FMath::Max(0.f, E.Read(MultiplierDef, 1.f));

		// 최대 체력 비례 추가 피해
		const float MaxHealthPercent = E.Read(E.Capture.SourceMaxHealthPercentDamageDef, 0.f);
		if (MaxHealthPercent > 0.f)
		{
			Damage += FMath::Max(0.f, E.Read(E.Capture.TargetMaxHealthDef, 0.f)) * MaxHealthPercent;
		}

		return Damage;
	}

	// 보호막 관통 데미지
	FRogueDamageSplit SplitByShield(const FRogueDamageEval& E, const FRogueDamageSource& Source, float Damage)
	{
		const float Penetration =
			FMath::Clamp(E.Read(E.Capture.SourceShieldPenetrationRateDef, 0.f), 0.f, MaxPenetration);

		float       ShieldDamage = Damage * (1.f - Penetration);
		const float PierceDamage = Damage * Penetration;

		// 보호막에 주는 피해 배율
		if (Source.bIsOffensive)
		{
			ShieldDamage *= FMath::Max(0.f, E.Read(E.Capture.SourceShieldDamageMultiplierDef, 1.f));
		}

		const float CurrentShield = FMath::Max(0.f, E.Read(E.Capture.TargetShieldDef, 0.f));
		const float Overkill      = FMath::Max(0.f, ShieldDamage - CurrentShield);

		FRogueDamageSplit Split;
		Split.ShieldDamage = FMath::Min(ShieldDamage, CurrentShield);
		Split.HealthDamage = PierceDamage + Overkill;

		return Split;
	}

	// 방어 측 경감
	float ApplyDefense(const FRogueDamageEval& E,
	                   const FRogueDamageSource& Source,
	                   const FRogueConditionalTotals& Totals,
	                   float Damage)
	{
		if (Source.bIsSelfCost)
		{
			return Damage;
		}

		// 비율 감소
		const float ReductionRate =
			FMath::Clamp(E.Read(E.Capture.TargetDamageReductionRateDef, 0.f), 0.f, MaxReductionRate);
		Damage *= (1.f - ReductionRate);

		// 고정 감소
		const float FlatReduction = FMath::Max(0.f, E.Read(E.Capture.TargetFlatDamageReductionDef, 0.f));
		Damage = FMath::Max(0.f, Damage - FlatReduction);

		// 조건부 규칙 적용
		return FMath::Max(0.f, Damage * Totals.TakenMultiplier + Totals.TakenFlat);
	}
}

UExecCalc_Damage::UExecCalc_Damage()
{
	const FRogueDamageCapture& C = Capture();

	RelevantAttributesToCapture.Add(C.TargetShieldDef);
	RelevantAttributesToCapture.Add(C.TargetHealthDef);
	RelevantAttributesToCapture.Add(C.TargetMaxHealthDef);
	RelevantAttributesToCapture.Add(C.SourceHealthDef);
	RelevantAttributesToCapture.Add(C.SourceMaxHealthDef);

	RelevantAttributesToCapture.Add(C.SourceHeadshotDamageMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceDamageMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceSkillDamageMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceShieldPenetrationRateDef);
	RelevantAttributesToCapture.Add(C.SourceShieldDamageMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceMaxHealthPercentDamageDef);

	RelevantAttributesToCapture.Add(C.TargetHeadshotDamageTakenRateDef);
	RelevantAttributesToCapture.Add(C.TargetDamageReductionRateDef);
	RelevantAttributesToCapture.Add(C.TargetFlatDamageReductionDef);
}

void UExecCalc_Damage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	float Damage = Spec.GetSetByCallerMagnitude(RogueTags::Data_Damage_Base, false, 0.f);
	if (Damage <= 0.f)
	{
		return;
	}

	FRogueDamageEval E{ ExecutionParams, Capture(), FAggregatorEvaluateParameters() };
	E.Aggregator.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	E.Aggregator.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 커스텀 컨텍스트가 설정이 안되어 있으면 증강 데이터가 들어오지 않음
	const FRogueGameplayEffectContext* Ctx = FRogueGameplayEffectContext::ExtractFrom(Spec.GetContext());
	if (!Ctx)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[RogueDamage] 커스텀 EffectContext 가 없습니다. "
			     "DefaultGame.ini 의 AbilitySystemGlobalsClassName 등록을 확인하세요. "
			     "출처 구분 없이 무기 피해로 처리합니다."));
	}

	const FRogueDamageSource Source = ResolveSource(Ctx);

	const FRogueRuleContext      RC     = MakeRuleContext(E, Ctx);
	const FRogueConditionalTotals Totals = GatherConditionalTotals(E, RC, Source);


	Damage = ApplyHitZoneCoefficients(E, Source, Ctx, Totals, Damage);
	Damage = ApplyOffenseCoefficients(E, Source, Damage);
	Damage = FMath::Max(0.f, Damage * Totals.DealtMultiplier + Totals.DealtFlat);
	Damage = ApplyDefense(E, Source, Totals, Damage);
	const FRogueDamageSplit Split = SplitByShield(E, Source, Damage);

	if (Split.ShieldDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			URogueVitalAttributeSet::GetIncomingShieldDamageAttribute(),
			EGameplayModOp::Additive, Split.ShieldDamage));
	}

	if (Split.HealthDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			URogueVitalAttributeSet::GetIncomingDamageAttribute(),
			EGameplayModOp::Additive, Split.HealthDamage));
	}
}
