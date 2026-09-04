// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Damage/ExecCalc_Damage.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentTypes.h"
#include "Data/WeaponDamageData.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Tags/GameplayTags.h"

namespace
{
	// 연산이 성립하기 위한 정의역 제한.
	// 수식이 깨지지 않게 하는 최소 조건
	constexpr float MaxPenetration    = 1.f;     // 비율이므로 0~1
	constexpr float MaxReductionRate  = 0.95f;   // 1.0 이면 무적
	constexpr float MinPenetrationMul = 1.f;     // 관통력 감소 증강은 없음
	constexpr float TorsoMultiplier   = 1.f;     // 미등록 부위의 기준값이자 헤드 초과분의 기준선
	constexpr float MaxShopDamageAmplification = 0.25f; // 상점 기본 대미지 증폭 상한. +5% x 5회 = +25%
	constexpr float MaxLifestealRate = 1.f;      // 준 피해보다 많이 회복하는 것은 허용하지 않음

	// ------------------------------------------------------------
	// GAS 연동
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
		FGameplayEffectAttributeCaptureDefinition SourceShopDamageAmplificationDef;

		// Augment / 공격 측
		FGameplayEffectAttributeCaptureDefinition SourceHeadshotDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceSkillDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceShieldPenetrationRateDef;
		FGameplayEffectAttributeCaptureDefinition SourceShieldDamageMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceMaxHealthPercentDamageDef;
		FGameplayEffectAttributeCaptureDefinition SourceEffectiveRangeBonusDef;
		FGameplayEffectAttributeCaptureDefinition SourcePenetrationMultiplierDef;
		FGameplayEffectAttributeCaptureDefinition SourceLifestealRateDef;

		// Augment / 방어 측
		FGameplayEffectAttributeCaptureDefinition TargetHeadshotDamageTakenRateDef;
		FGameplayEffectAttributeCaptureDefinition TargetDamageReductionRateDef;
		FGameplayEffectAttributeCaptureDefinition TargetFlatDamageReductionDef;
		FGameplayEffectAttributeCaptureDefinition TargetEffectiveRangeBonusDef;

		FRogueDamageCapture()
		{
			TargetShieldDef    = MakeDef(URogueVitalAttributeSet::GetShieldAttribute(),    ECaptureSource::Target);
			TargetHealthDef    = MakeDef(URogueVitalAttributeSet::GetHealthAttribute(),    ECaptureSource::Target);
			TargetMaxHealthDef = MakeDef(URogueVitalAttributeSet::GetMaxHealthAttribute(), ECaptureSource::Target);
			SourceHealthDef    = MakeDef(URogueVitalAttributeSet::GetHealthAttribute(),    ECaptureSource::Source);
			SourceMaxHealthDef = MakeDef(URogueVitalAttributeSet::GetMaxHealthAttribute(), ECaptureSource::Source);

			SourceShopDamageAmplificationDef =
				MakeDef(URogueVitalAttributeSet::GetShopDamageAmplificationAttribute(), ECaptureSource::Source);

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
			SourceEffectiveRangeBonusDef =
				MakeDef(URogueAugmentAttributeSet::GetEffectiveRangeBonusAttribute(),    ECaptureSource::Source);
			SourcePenetrationMultiplierDef =
				MakeDef(URogueAugmentAttributeSet::GetPenetrationMultiplierAttribute(),  ECaptureSource::Source);
			SourceLifestealRateDef =
				MakeDef(URogueAugmentAttributeSet::GetLifestealRateAttribute(),          ECaptureSource::Source);

			TargetHeadshotDamageTakenRateDef =
				MakeDef(URogueAugmentAttributeSet::GetHeadshotDamageTakenRateAttribute(), ECaptureSource::Target);
			TargetDamageReductionRateDef =
				MakeDef(URogueAugmentAttributeSet::GetDamageReductionRateAttribute(),    ECaptureSource::Target);
			TargetFlatDamageReductionDef =
				MakeDef(URogueAugmentAttributeSet::GetFlatDamageReductionAttribute(),    ECaptureSource::Target);

			// 받는 피해 규칙의 거리 조건은 피해자 자신의 사거리 증강을 봐야 한다
			TargetEffectiveRangeBonusDef =
				MakeDef(URogueAugmentAttributeSet::GetEffectiveRangeBonusAttribute(),    ECaptureSource::Target);
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

	// ------------------------------------------------------------
	// 공용 자료구조

	// 이 히트의 성격
	struct FRogueDamageSource
	{
		FGameplayTag Tag;

		// 자가 피해 여부
		bool bIsSelfCost = false;

		// 적의 공격 여부
		bool bIsOffensive = false;
	};

	// 조건부 증강 누산
	struct FRogueRuleTotals
	{
		float BonusRateSum     = 0.f;
		float Flat             = 0.f;
		float HeadBonusRateSum = 0.f;
		float HeadFlat         = 0.f;
		float LifestealRate	   = 0.f; // 공격 측만 적용. 방어 측은 흡혈 없음
	};

	// 공격 / 방어 조건부 규칙 결과 누산
	struct FRogueConditionalTotals
	{
		FRogueRuleTotals Dealt;
		FRogueRuleTotals Taken;
	};

	// 규칙 판정에 필요한 히트 정보
	struct FRogueRuleContext
	{
		const FRogueGameplayEffectContext* Ctx = nullptr;

		float TargetHealthPct = 100.f;
		float SourceHealthPct = 100.f;
		bool  bTargetHealthKnown = false;
		bool  bSourceHealthKnown = false;

		// 사거리 증강. 규칙 Side 로 골라 쓴다
		float SourceRangeBonusMeters = 0.f;
		float TargetRangeBonusMeters = 0.f;
	};

	// 보호막, 체력 피해량
	struct FRogueDamageSplit
	{
		float ShieldDamage = 0.f;
		float HealthDamage = 0.f;
	};

	// 대미지 누산 구조체
	struct FRogueDamageAccumulator
	{
		// 몸통 기준 대미지 누적
		float Base = 0.f;

		// 헤드 배율로 늘어난 몫
		float HeadshotBonus = 0.f;

		void Scale(const float M)           { Base *= M; HeadshotBonus *= M; }
		void ScaleHeadshot(const float M)   { HeadshotBonus *= M; }
		void AddFlat(const float A)         { Base += A; }
		void AddHeadshotFlat(const float A) { HeadshotBonus += A; }

		float Total() const { return FMath::Max(0.f, Base + HeadshotBonus); }
	};

	// ------------------------------------------------------------
	// 증강 규칙 판정
	bool IsInRange(float Value, float Min, float Max)
	{
		return Value >= Min && Value <= Max;
	}

	// 규칙 적용 여부 판정
	bool DoesRuleApply(const FAugmentConditionalDamageRule& Rule, const FRogueRuleContext& RC)
	{
		// 컨텍스트가 없는 경우
		if (!RC.Ctx)
		{
			return false;
		}

		// 필수 태그가 없는 경우
		if (!Rule.RequiredContextTags.IsEmpty() && !RC.Ctx->ContextTags.HasAll(Rule.RequiredContextTags))
		{
			return false;
		}

		// 차단 태그가 존재하는 경우
		if (!Rule.BlockedContextTags.IsEmpty() && RC.Ctx->ContextTags.HasAny(Rule.BlockedContextTags))
		{
			return false;
		}

		// 필요한 무기 태그가 없는 경우
		if (!Rule.RequiredWeaponTags.IsEmpty() && !RC.Ctx->WeaponTag.MatchesAny(Rule.RequiredWeaponTags))
		{
			return false;
		}

		// 사거리 증강을 적용한 교전 거리가 규정 범위 밖인 경우
		if (Rule.bUseDistanceRange)
		{
			// 히트스캔이 아닌 공격은 거리 조건 적용 X
			if (RC.Ctx->HitGroups.IsEmpty())
			{
				return false;
			}

			const float RangeBonus = Rule.bDistanceScalesWithRangeBonus
				? ((Rule.Side == EAugmentDamageRuleSide::Dealt)
					? RC.SourceRangeBonusMeters
					: RC.TargetRangeBonusMeters)
				: 0.f;

			if (!IsInRange(RC.Ctx->HitDistanceMeters,
				Rule.MinDistanceMeters, Rule.MaxDistanceMeters + RangeBonus))
			{
				return false;
			}
		}

		// 피해자의 체력 구간이 밖인 경우, 체력을 읽을 수 없는 경우
		if (Rule.bUseTargetHealthRange
			&& (!RC.bTargetHealthKnown
				|| !IsInRange(RC.TargetHealthPct, Rule.TargetHealthMinPct, Rule.TargetHealthMaxPct)))
		{
			return false;
		}

		// 가해자의 체력 구간이 밖인 경우, 체력을 읽을 수 없는 경우
		if (Rule.bUseSourceHealthRange
			&& (!RC.bSourceHealthKnown
				|| !IsInRange(RC.SourceHealthPct, Rule.SourceHealthMinPct, Rule.SourceHealthMaxPct)))
		{
			return false;
		}

		// 탄창 순번이 규정 범위 밖인 경우
		if (Rule.bUseShotIndexRange
			&& !IsInRange(RC.Ctx->ShotIndexInMagazine, Rule.MinShotIndexInMagazine, Rule.MaxShotIndexInMagazine))
		{
			return false;
		}

		// 켜져있는 조건을 모두 통과하면
		return true;
	}

	// 한쪽 인벤토리의 조건부 규칙을 훑어 발동한 증강을 누적
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

			// Side 는 위에서 이미 걸렀다. 남은 선택은 몸통 몫이냐 헤드 몫이냐뿐
			FRogueRuleTotals& Bucket = (Side == EAugmentDamageRuleSide::Dealt) ? Totals.Dealt : Totals.Taken;

			// 규칙끼리 합산. 하한은 배율로 환산할 때
			if (Rule.bAppliesToHeadshotBonusOnly)
			{
				Bucket.HeadBonusRateSum += Rule.DamageBonusRate;
				Bucket.HeadFlat         += Rule.FlatDamageBonus;
			}
			else
			{
				Bucket.BonusRateSum += Rule.DamageBonusRate;
				Bucket.Flat         += Rule.FlatDamageBonus;
			}

			// 흡혈은 공격 측만 적용
			Bucket.LifestealRate += FMath::Max(0.f, Rule.LifestealRate);
		}
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

		RC.SourceRangeBonusMeters = FMath::Max(0.f, E.Read(E.Capture.SourceEffectiveRangeBonusDef, 0.f));
		RC.TargetRangeBonusMeters = FMath::Max(0.f, E.Read(E.Capture.TargetEffectiveRangeBonusDef, 0.f));

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

	// ------------------------------------------------------------
	// 파이프라인

	// 파이프라인 진입 전 대미지의 출처 판별
	FRogueDamageSource ResolveSource(const FRogueGameplayEffectContext* Ctx)
	{
		FRogueDamageSource Source;
		Source.Tag = Ctx ? Ctx->SourceTag : RogueTags::Damage_Source_Weapon;

		Source.bIsSelfCost  = (Source.Tag == RogueTags::Damage_Source_SelfCost);
		Source.bIsOffensive = (Source.Tag == RogueTags::Damage_Source_Weapon)
			|| (Source.Tag == RogueTags::Damage_Source_Skill);

		return Source;
	}

	// 부위별 배율 읽기, 미등록 부위는 몸통 기준값 반환
	float ReadHitZoneMultiplier(const FWeaponDamageEntry& Entry, const FGameplayTag HitZoneTag)
	{
		const float* Multiplier = Entry.HitZoneMultipliers.Find(HitZoneTag);
		if (!Multiplier || !FMath::IsFinite(*Multiplier))
		{
			return TorsoMultiplier;
		}

		return FMath::Max(0.f, *Multiplier);
	}

	// 1. 거리 감쇠 대미지 적용
	float ResolveDamagePerPellet(const FWeaponDamageEntry& Entry,
	                             const float               DistanceMeters,
	                             const float               EffectiveRangeBonusMeters)
	{
		const float Base = Entry.BaseDamagePerPellet;

		// 감쇠가 적용되지 않는 무기는 기본 피해량 반환
		if (Entry.MaximumRangeMeters <= KINDA_SMALL_NUMBER)
		{
			return Base;
		}

		// 증강 사거리 보정, 유효/최대에 증강 값을 더해줌
		const float Bonus     = FMath::Max(0.f, EffectiveRangeBonusMeters);
		const float Effective = Entry.EffectiveRangeMeters + Bonus;
		const float Maximum   = FMath::Max(Entry.MaximumRangeMeters, Entry.EffectiveRangeMeters) + Bonus;

		const float Distance = FMath::Max(0.f, DistanceMeters);

		// 하한값이 기본 피해량 보다 큰 경우 클램핑
		const float Minimum = FMath::Clamp(Entry.MinimumDamage, 0.f, Base);

		// 유효 사거리 이내는 기본 피해량
		if (Distance <= Effective)
		{
			return Base;
		}

		// 최대 사거리 이상은 하한값
		if (Distance >= Maximum)
		{
			return Minimum;
		}

		// 유효~최대 구간은 거리 감쇠 적용
		const float Reduced = Base - (Distance - Effective) * Entry.DamageFalloffPerMeter;
		return FMath::Max(Minimum, Reduced);
	}

	// 2. 관통력 증강 적용
	float ResolveEffectivePellets(const FRogueHitGroup& Group, const float PenetrationMultiplier)
	{
		const float PelletCount = static_cast<float>(FMath::Max(0, Group.PelletCount));
		if (PelletCount <= 0.f)
		{
			return 0.f;
		}

		const float Retention = FMath::Clamp(Group.WallRetentionSum, 0.f, PelletCount);
		if (Retention <= 0.f)
		{
			return 0.f;
		}

		const float Penetration = FMath::Max(MinPenetrationMul, PenetrationMultiplier);

		return FMath::Clamp(PelletCount - (PelletCount - Retention) / Penetration, 0.f, PelletCount);
	}

	// 3. 부위별 대미지 적용
	void AccumulateHitGroup(const FWeaponDamageEntry& Entry,
	                        const FRogueHitGroup&     Group,
	                        const float               DamagePerPellet,
	                        const float               PenetrationMultiplier,
	                        FRogueDamageAccumulator&  Acc)
	{
		// 관통력 적용 후 대미지
		const float GroupDamage = DamagePerPellet * ResolveEffectivePellets(Group, PenetrationMultiplier);
		if (GroupDamage <= 0.f || !FMath::IsFinite(GroupDamage))
		{
			return;
		}
		// 부위별 배율 계산
		const float ZoneMultiplier = ReadHitZoneMultiplier(Entry, Group.HitZoneTag);

		// 헤드샷 배율은 몸통 배율을 기준으로 초과분만 적용
		const float BonusShare = RogueTags::IsHeadZone(Group.HitZoneTag)
			? FMath::Max(0.f, ZoneMultiplier - TorsoMultiplier)
			: 0.f;

		// Base는 몸통 기준 대미지, HeadshotBonus는 몸통 초과분만 누적
		Acc.Base          += GroupDamage * (ZoneMultiplier - BonusShare);
		Acc.HeadshotBonus += GroupDamage * BonusShare;
	}

	// 1~3단계를 묶은 무기 대미지
	FRogueDamageAccumulator CalculateWeaponDamage(
		const FWeaponDamageEntry&             Entry,
		const float                           DistanceMeters,
		const TConstArrayView<FRogueHitGroup> HitGroups,
		const float                           EffectiveRangeBonusMeters,
		const float                           PenetrationMultiplier)
	{
		FRogueDamageAccumulator Accumulator;

		const float DamagePerPellet = ResolveDamagePerPellet(Entry, DistanceMeters, EffectiveRangeBonusMeters);
		if (DamagePerPellet <= 0.f)
		{
			return Accumulator;
		}

		for (const FRogueHitGroup& Group : HitGroups)
		{
			AccumulateHitGroup(Entry, Group, DamagePerPellet, PenetrationMultiplier, Accumulator);
		}

		return Accumulator;
	}

	// 1~3단계 진입
	FRogueDamageAccumulator ResolveIncomingDamage(const FRogueDamageEval&            E,
	                                              const FRogueGameplayEffectContext* Ctx,
	                                              const FGameplayEffectSpec&         Spec)
	{
		if (Ctx && !Ctx->HitGroups.IsEmpty())
		{
			// 미등록 무기는 FindEntry 가 경고를 남긴다. 그 경우 아래 SetByCaller 로 폴백한다.
			if (const FWeaponDamageEntry* Entry = UWeaponDamageData::FindEntry(Ctx->WeaponTag))
			{
				return CalculateWeaponDamage(
					*Entry,
					Ctx->HitDistanceMeters,
					Ctx->HitGroups,
					E.Read(E.Capture.SourceEffectiveRangeBonusDef,   0.f),
					E.Read(E.Capture.SourcePenetrationMultiplierDef, 1.f));
			}
		}

		FRogueDamageAccumulator Accumulator;
		Accumulator.Base = Spec.GetSetByCallerMagnitude(RogueTags::Data_Damage_Base, false, 0.f);

		return Accumulator;
	}

	// 4. 공격 측 상시 증강
	void ApplyOffenseStats(const FRogueDamageEval& E, const FRogueDamageSource& Source, FRogueDamageAccumulator& Acc)
	{
		// 자가 피해나 환경 피해는 공격자 증강 적용 X
		if (!Source.bIsOffensive)
		{
			return;
		}

		// 무기 피해, 스킬 피해 구분
		const FGameplayEffectAttributeCaptureDefinition& MultiplierDef =
			(Source.Tag == RogueTags::Damage_Source_Skill)
				? E.Capture.SourceSkillDamageMultiplierDef
				: E.Capture.SourceDamageMultiplierDef;

		// 전체 피해량 배율
		Acc.Scale(FMath::Max(0.f, E.Read(MultiplierDef, 1.f)));

		// 상점 기본 대미지 증폭
		const float ShopAmplification = FMath::Clamp(
			E.Read(E.Capture.SourceShopDamageAmplificationDef, 0.f), 0.f, MaxShopDamageAmplification);
		Acc.Scale(1.f + ShopAmplification);

		// 헤드샷 대미지 배율
		Acc.ScaleHeadshot(FMath::Max(0.f, E.Read(E.Capture.SourceHeadshotDamageMultiplierDef, 1.f)));

		// 최대 체력 비례 추가 피해
		const float MaxHealthPercent = E.Read(E.Capture.SourceMaxHealthPercentDamageDef, 0.f);
		if (MaxHealthPercent > 0.f)
		{
			Acc.AddFlat(FMath::Max(0.f, E.Read(E.Capture.TargetMaxHealthDef, 0.f)) * MaxHealthPercent);
		}
	}

	// 증감률 합을 배율로 환산
	float ResolveRuleScale(const float BonusRateSum, const float MinScale)
	{
		return FMath::Max(MinScale, 1.f + BonusRateSum);
	}

	// 한쪽 조건부 규칙 누산 결과를 몸통 몫과 헤드 몫에 각각 반영
	void ApplyRuleTotals(const FRogueRuleTotals& Totals, const float MinScale, FRogueDamageAccumulator& Acc)
	{
		Acc.Scale(ResolveRuleScale(Totals.BonusRateSum, MinScale));
		Acc.AddFlat(Totals.Flat);

		Acc.ScaleHeadshot(ResolveRuleScale(Totals.HeadBonusRateSum, MinScale));
		Acc.AddHeadshotFlat(Totals.HeadFlat);
	}

	// 5. 공격 측 조건부 증강
	void ApplyDealtRules(const FRogueConditionalTotals& Totals, FRogueDamageAccumulator& Acc)
	{
		// 공격 측 하한 0.f
		ApplyRuleTotals(Totals.Dealt, 0.f, Acc);
	}

	// 6. 방어 측 상시 증강
	void ApplyDefenseStats(const FRogueDamageEval& E, const FRogueDamageSource& Source, FRogueDamageAccumulator& Acc)
	{
		// 자가 피해는 감소 X
		if (Source.bIsSelfCost)
		{
			return;
		}

		// 비율 감소. 1.0 을 허용하면 무적이 되므로 상한
		const float ReductionRate =
			FMath::Clamp(E.Read(E.Capture.TargetDamageReductionRateDef, 0.f), 0.f, MaxReductionRate);
		Acc.Scale(1.f - ReductionRate);

		// 헤드샷 대미지 배율 감소
		Acc.ScaleHeadshot(FMath::Max(0.f, E.Read(E.Capture.TargetHeadshotDamageTakenRateDef, 1.f)));

		// 고정 피해 감소
		const float FlatReduction = FMath::Max(0.f, E.Read(E.Capture.TargetFlatDamageReductionDef, 0.f));
		Acc.AddFlat(-FlatReduction);
	}

	// 7. 방어 측 조건부 증강
	void ApplyTakenRules(const FRogueDamageSource& Source,
	                     const FRogueConditionalTotals& Totals,
	                     FRogueDamageAccumulator& Acc)
	{
		if (Source.bIsSelfCost)
		{
			return;
		}

		// 방어 측 하한 1.f - MaxReductionRate
		ApplyRuleTotals(Totals.Taken, 1.f - MaxReductionRate, Acc);
	}

	// 8. 최종 대미지를 보호막 대미지와 체력 대미지로 분리
	FRogueDamageSplit SplitByShield(const FRogueDamageEval& E, const FRogueDamageSource& Source, const float RawDamage)
	{
		FRogueDamageSplit Split;

		if (RawDamage <= 0.f)
		{
			return Split;
		}

		// 자가 피해는 전부 체력 대미지
		if (Source.bIsSelfCost)
		{
			Split.HealthDamage = FMath::FloorToFloat(RawDamage);
			return Split;
		}

		// 보호막 관통 증강 적용
		const float Penetration =
			FMath::Clamp(E.Read(E.Capture.SourceShieldPenetrationRateDef, 0.f), 0.f, MaxPenetration);
		const float ShieldPortion = RawDamage * (1.f - Penetration); // 보호막에 적용되는 대미지

		// 공격자 보호막 피해 배율
		const float ShieldMultiplier = Source.bIsOffensive
			? FMath::Max(0.f, E.Read(E.Capture.SourceShieldDamageMultiplierDef, 1.f))
			: 1.f;

		const float CurrentShield = FMath::Max(0.f, E.Read(E.Capture.TargetShieldDef, 0.f));

		// 보호막이 실제로 잃는 양
		Split.ShieldDamage = FMath::Min(FMath::FloorToFloat(ShieldPortion * ShieldMultiplier), CurrentShield);

		// 보호막이 대신 받아낸 몫을 원시 대미지 단위로 환산
		const float RawAbsorbedDamage = (Split.ShieldDamage > 0.f)
			? (Split.ShieldDamage / ShieldMultiplier)
			: 0.f;

		// 남은 원시 대미지를 체력 대미지로 전환하고 최종 정수화
		Split.HealthDamage = FMath::Max(0.f, FMath::FloorToFloat(RawDamage - RawAbsorbedDamage));

		return Split;
	}

	// 9. 최종 흡혈 계수 계산
	float ResolveLifestealRate(const FRogueDamageEval&        E,
	                           const FRogueDamageSource&      Source,
	                           const FRogueConditionalTotals& Totals)
	{
		// 자가 피해나 환경 피해는 흡혈 X
		if (!Source.bIsOffensive)
		{
			return 0.f;
		}

		// 상시, 패시브 흡혈
		const float Global = FMath::Max(0.f, E.Read(E.Capture.SourceLifestealRateDef, 0.f));

		// 조건부 흡혈
		return FMath::Clamp(Global + Totals.Dealt.LifestealRate, 0.f, MaxLifestealRate);
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
	RelevantAttributesToCapture.Add(C.SourceShopDamageAmplificationDef);

	RelevantAttributesToCapture.Add(C.SourceHeadshotDamageMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceDamageMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceSkillDamageMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceShieldPenetrationRateDef);
	RelevantAttributesToCapture.Add(C.SourceShieldDamageMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceMaxHealthPercentDamageDef);
	RelevantAttributesToCapture.Add(C.SourceEffectiveRangeBonusDef);
	RelevantAttributesToCapture.Add(C.SourcePenetrationMultiplierDef);
	RelevantAttributesToCapture.Add(C.SourceLifestealRateDef);

	RelevantAttributesToCapture.Add(C.TargetHeadshotDamageTakenRateDef);
	RelevantAttributesToCapture.Add(C.TargetDamageReductionRateDef);
	RelevantAttributesToCapture.Add(C.TargetFlatDamageReductionDef);
	RelevantAttributesToCapture.Add(C.TargetEffectiveRangeBonusDef);
}

void UExecCalc_Damage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

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

	// 1~3단계. 거리 감쇠 -> 관통력 증강 -> 부위 대미지
	FRogueDamageAccumulator Accumulator = ResolveIncomingDamage(E, Ctx, Spec);
	if (Accumulator.Total() <= 0.f)
	{
		return;
	}

	const FRogueRuleContext      RC     = MakeRuleContext(E, Ctx);
	const FRogueConditionalTotals Totals = GatherConditionalTotals(E, RC, Source);

	// 4~5단계. 공격 측 상시 -> 조건부
	ApplyOffenseStats(E, Source, Accumulator);
	ApplyDealtRules(Totals, Accumulator);

	// 6~7단계. 방어 측 상시 -> 조건부
	ApplyDefenseStats(E, Source, Accumulator);
	ApplyTakenRules(Source, Totals, Accumulator);

	// 8단계. 방어막 대미지와 체력 대미지로 분리
	const FRogueDamageSplit Split = SplitByShield(E, Source, Accumulator.Total());

	// 최종 대미지 적용
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

	// 흡혈 계수를 컨텍스트에 실어 보냄
	FGameplayEffectContextHandle MutableHandle = Spec.GetContext();
	if (FRogueGameplayEffectContext* MutableCtx =
			FRogueGameplayEffectContext::ExtractMutableFrom(MutableHandle))
	{
		MutableCtx->LifestealRate = ResolveLifestealRate(E, Source, Totals);
	}
}
