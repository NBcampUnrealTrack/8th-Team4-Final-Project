// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Damage/RogueDamageSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "GAS/RogueGASSettings.h"
#include "GAS/RogueTargetFilter.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Tags/GameplayTags.h"

UAbilitySystemComponent* URogueDamageSubsystem::ResolveASC(AActor* Actor)
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
}

// 신규 히트스캔 경로
FRogueDamageResult URogueDamageSubsystem::ApplyHitscanDamage(const FRogueDamageParams& Params,
                                                             const FRogueHitContext& Hit)
{
	// 유효 팰릿이 없는 경우
	if (Hit.HitGroups.IsEmpty())
	{
		return FRogueDamageResult();
	}

	// 총 관통력 합산
	float TotalRetention = 0.f;
	for (const FRogueHitGroup& Group : Hit.HitGroups)
	{
		if (FMath::IsFinite(Group.WallRetentionSum))
		{
			TotalRetention += FMath::Max(0.f, Group.WallRetentionSum);
		}
	}

	// 관통력이 0 이하이면 대미지 적용 X
	if (TotalRetention <= 0.f)
	{
		return FRogueDamageResult();
	}

	return ApplyDamageInternal(Params, Hit.Target, &Hit, 0.f);
}

// 탄착 기하가 없는 피해, 스킬, 폭발, 자가피해
FRogueDamageResult URogueDamageSubsystem::ApplyFlatDamage(const FRogueDamageParams& Params,
                                                          AActor* Target,
                                                          const float Damage)
{
	if (!FMath::IsFinite(Damage) || Damage <= 0.f)
	{
		return FRogueDamageResult();
	}

	return ApplyDamageInternal(Params, Target, nullptr, Damage);
}

FRogueDamageResult URogueDamageSubsystem::ApplyDamageInternal(const FRogueDamageParams& Params,
                                                              AActor*                   TargetActor,
                                                              const FRogueHitContext*   Hit,
                                                              const float               FlatDamage)
{
	FRogueDamageTarget DamageTarget;
	if (!ResolveDamageTarget(TargetActor, DamageTarget))
	{
		return FRogueDamageResult();
	}

	// 무적 태그 검사, 자가 피해는 무적 태그를 무시
	if (Params.SourceTag != RogueTags::Damage_Source_SelfCost
		&& DamageTarget.ASC->HasMatchingGameplayTag(RogueTags::State_Combat_Invulnerable))
	{
		return FRogueDamageResult();
	}

	// 팀킬 차단
	if (URogueTargetFilter::GetRelation(Params.Instigator, TargetActor) == ERogueTargetRelation::Friendly)
	{
		return FRogueDamageResult();
	}

	// 대미지 GE 확보
	UClass* DamageGEClass = ResolveDamageEffectClass();
	if (!DamageGEClass)
	{
		return FRogueDamageResult();
	}

	// 컨텍스트 조립
	// 가해자 ASC 가 없으면 타깃으로 대체
	UAbilitySystemComponent* SourceASC  = ResolveASC(Params.Instigator);
	UAbilitySystemComponent* ContextASC = SourceASC ? SourceASC : DamageTarget.ASC;

	const FGameplayEffectContextHandle ContextHandle =
		MakeDamageContext(Params, Hit, *ContextASC, DamageTarget.Vital);

	// 스펙 생성
	const FGameplayEffectSpecHandle SpecHandle = ContextASC->MakeOutgoingSpec(DamageGEClass, 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return FRogueDamageResult();
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Damage_Base, FlatDamage);

	// 대미지 적용 전 체력/실드 수집
	const float ShieldBefore = DamageTarget.Vital->GetShield();
	const float HealthBefore = DamageTarget.Vital->GetHealth();

	DamageTarget.ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// 결과 수집
	FRogueDamageResult Result = MeasureDamageResult(*DamageTarget.Vital, ShieldBefore, HealthBefore);

	// ExecCalc 이 컨텍스트에 실어 보낸 흡혈 계수를 결과에 저장
	if (const FRogueGameplayEffectContext* Ctx = FRogueGameplayEffectContext::ExtractFrom(ContextHandle))
	{
		Result.LifestealRate = Ctx->LifestealRate;
	}

	// 흡혈 적용
	ApplyLifesteal(SourceASC, DamageTarget, Result);

	// 이벤트 발신
	BroadcastCombatEvents(Params, TargetActor, Result, ContextHandle);

	return Result;
}

bool URogueDamageSubsystem::ResolveDamageTarget(AActor* TargetActor, FRogueDamageTarget& OutTarget)
{
	if (!TargetActor)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = ResolveASC(TargetActor);
	if (!TargetASC)
	{
		return false;
	}

	// 클라이언트는 무시
	if (!TargetASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	const URogueVitalAttributeSet* Vital = TargetASC->GetSet<URogueVitalAttributeSet>();
	if (!Vital)
	{
		return false;
	}

	// 이미 사망한 상태면 무시
	if (Vital->IsDeathDeclared())
	{
		return false;
	}

	OutTarget.ASC   = TargetASC;
	OutTarget.Vital = Vital;
	return true;
}

UClass* URogueDamageSubsystem::ResolveDamageEffectClass()
{
	UClass* DamageGEClass = GetDefault<URogueGASSettings>()->DamageEffectClass.LoadSynchronous();
	if (!DamageGEClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[RogueDamage] DamageEffectClass 가 설정되지 않았습니다. "
			     "프로젝트 설정 > 게임 > Rogue GAS 를 확인하세요."));
	}

	return DamageGEClass;
}

FGameplayEffectContextHandle URogueDamageSubsystem::MakeDamageContext(const FRogueDamageParams& Params,
                                                                     const FRogueHitContext* Hit,
                                                                     UAbilitySystemComponent& ContextASC,
                                                                     const URogueVitalAttributeSet* TargetVital) const
{
	FGameplayEffectContextHandle ContextHandle = ContextASC.MakeEffectContext();
	ContextHandle.AddInstigator(Params.Instigator, Params.Causer);

	FGameplayEffectContext* RawContext = ContextHandle.Get();
	if (!RawContext || RawContext->GetScriptStruct() != FRogueGameplayEffectContext::StaticStruct())
	{
		// 대미지 자체는 들어가지만 증강이 무시됨
		UE_LOG(LogTemp, Error,
			TEXT("[RogueDamage] 커스텀 EffectContext 가 생성되지 않았습니다. "
			     "DefaultGame.ini 의 AbilitySystemGlobalsClassName 등록을 확인하세요."));
		return ContextHandle;
	}

	FRogueGameplayEffectContext* Ctx = static_cast<FRogueGameplayEffectContext*>(RawContext);

	Ctx->SourceTag           = Params.SourceTag;
	Ctx->WeaponTag           = Params.WeaponTag;
	Ctx->ShotIndexInMagazine = Params.ShotIndexInMagazine;

	// 히트스캔 데이터를 컨텍스트에 담음
	if (Hit)
	{
		Ctx->HitDistanceMeters = FMath::Max(0.f, Hit->DistanceMeters);
		Ctx->HitShotDirection  = Hit->ShotDirection.GetSafeNormal();
		Ctx->HitBoneName       = Hit->HitBoneName;
		Ctx->HitGroups.Reset(FMath::Min(Hit->HitGroups.Num(), RogueMaxHitGroups));

		for (const FRogueHitGroup& Group : Hit->HitGroups)
		{
			if (Ctx->HitGroups.Num() >= RogueMaxHitGroups)
			{
				break;
			}
			if (Group.PelletCount <= 0 || !FMath::IsFinite(Group.WallRetentionSum))
			{
				continue;
			}

			FRogueHitGroup& Sanitized = Ctx->HitGroups.AddDefaulted_GetRef();
			Sanitized.HitZoneTag       = Group.HitZoneTag;
			Sanitized.PelletCount      = Group.PelletCount;
			Sanitized.WallRetentionSum = FMath::Clamp(
				Group.WallRetentionSum, 0.f, static_cast<float>(Group.PelletCount));

			if (RogueTags::IsHeadZone(Sanitized.HitZoneTag))
			{
				Ctx->bHeadshot = true;
			}
		}
	}

	BuildContextTags(Params, Hit, TargetVital, Ctx->ContextTags);

	return ContextHandle;
}

FRogueDamageResult URogueDamageSubsystem::MeasureDamageResult(const URogueVitalAttributeSet& Vital,
                                                             float ShieldBefore,
                                                             float HealthBefore)
{
	FRogueDamageResult Result;

	Result.bApplied        = true;
	Result.ShieldDamage    = FMath::Max(0.f, ShieldBefore - Vital.GetShield());
	Result.HealthDamage    = FMath::Max(0.f, HealthBefore - Vital.GetHealth());
	Result.bShieldBroken   = (ShieldBefore > 0.f && Vital.GetShield() <= 0.f);
	Result.bHealthDepleted = Vital.IsDeathDeclared();

	return Result;
}

void URogueDamageSubsystem::BuildContextTags(const FRogueDamageParams& Params,
                                             const FRogueHitContext* Hit,
                                             const URogueVitalAttributeSet* TargetVital,
                                             FGameplayTagContainer& OutTags)
{
	if (Hit)
	{
		for (const FRogueHitGroup& Group : Hit->HitGroups)
		{
			if (Group.HitZoneTag.IsValid() && Group.PelletCount > 0)
			{
				OutTags.AddTag(Group.HitZoneTag);
			}
		}
	}

	if (Params.SourceTag.IsValid())
	{
		OutTags.AddTag(Params.SourceTag);
	}

	if (Hit && Hit->bBackstab)
	{
		OutTags.AddTag(RogueTags::Hit_Angle_Back);
	}

	// 쉴드 파괴 여부
	if (TargetVital)
	{
		OutTags.AddTag(TargetVital->GetShield() > 0.f
			? RogueTags::Target_State_HasShield
			: RogueTags::Target_State_NoShield);
	}
}

void URogueDamageSubsystem::ApplyLifesteal(UAbilitySystemComponent* SourceASC,
                                           const FRogueDamageTarget& DamageTarget,
                                           const FRogueDamageResult& Result) const
{
	if (!SourceASC || Result.LifestealRate <= 0.f)
	{
		return;
	}

	// 흡혈량 = (실제 체력 감소 + 실제 보호막 감소) * 흡혈 계수
	// 체력/보호막은 정수로 유지한다. SplitByShield 가 대미지를 버리는 것과 같은 규약
	const float Amount = FMath::FloorToFloat(
		(Result.HealthDamage + Result.ShieldDamage) * Result.LifestealRate);
	if (Amount <= 0.f)
	{
		return;
	}

	// 흡혈은 아군, 자가 피해는 적용 X
	const ERogueTargetRelation Relation =
		URogueTargetFilter::GetRelation(SourceASC->GetAvatarActor(), DamageTarget.ASC->GetAvatarActor());
	if (Relation == ERogueTargetRelation::Self || Relation == ERogueTargetRelation::Friendly)
	{
		return;
	}

	const URogueVitalAttributeSet* SourceVital = SourceASC->GetSet<URogueVitalAttributeSet>();
	if (!SourceVital)
	{
		return;
	}

	// 체력 회복, 보호막 회복 분할 및 정수화 (내림)
	const float ToHealth = FMath::FloorToFloat(FMath::Min(
		Amount, FMath::Max(0.f, SourceVital->GetMaxHealth() - SourceVital->GetHealth())));
	const float ToShield = Amount - ToHealth;

	const URogueGASSettings* Settings = GetDefault<URogueGASSettings>();

	if (ToHealth > 0.f)
	{
		ApplyInstantMagnitude(*SourceASC, Settings->HealEffectClass, ToHealth);
	}

	// 이미 보호막이 가득 차 있으면 회복 X
	if (ToShield > 0.f && SourceVital->GetShield() < SourceVital->GetMaxShield())
	{
		ApplyInstantMagnitude(*SourceASC, Settings->ShieldHealEffectClass, ToShield);
	}
}

void URogueDamageSubsystem::ApplyInstantMagnitude(UAbilitySystemComponent& ASC,
                                                  const TSoftClassPtr<UGameplayEffect>& EffectClass,
                                                  const float Magnitude)
{
	UClass* LoadedClass = EffectClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[RogueDamage] 회복 캐리어 GE 가 설정되지 않았습니다. "
			     "프로젝트 설정 > 게임 > Rogue GAS 를 확인하세요."));
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = ASC.MakeOutgoingSpec(LoadedClass, 1.f, ASC.MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Magnitude_Primary, Magnitude);
	ASC.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void URogueDamageSubsystem::BroadcastCombatEvents(const FRogueDamageParams& Params,
                                                  AActor* TargetActor,
                                                  const FRogueDamageResult& Result,
                                                  const FGameplayEffectContextHandle& ContextHandle) const
{
	if (!Result.bApplied)
	{
		return;
	}

	AActor* Src = Params.Instigator;
	AActor* Tgt = TargetActor;

	auto Send = [&ContextHandle, Src, Tgt](AActor* To, const FGameplayTag& Tag, float Magnitude)
	{
		if (!To || !Tag.IsValid())
		{
			return;
		}

		FGameplayEventData Payload;
		Payload.EventTag       = Tag;
		Payload.Instigator     = Src;
		Payload.Target         = Tgt;
		Payload.EventMagnitude = Magnitude;
		Payload.ContextHandle  = ContextHandle;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(To, Tag, Payload);
	};

	Send(Src, RogueTags::Event_Combat_DamageDealt, Result.HealthDamage);
	Send(Tgt, RogueTags::Event_Combat_DamageTaken, Result.HealthDamage);

	const FRogueGameplayEffectContext* Ctx = FRogueGameplayEffectContext::ExtractFrom(ContextHandle);
	if (Ctx && Ctx->bHeadshot)
	{
		Send(Src, RogueTags::Event_Combat_Headshot, Result.HealthDamage);
	}

	if (Result.bShieldBroken)
	{
		Send(Tgt, RogueTags::Event_Combat_ShieldBroken,      Result.ShieldDamage);
		Send(Src, RogueTags::Event_Combat_EnemyShieldBroken, Result.ShieldDamage);
	}

	// 사망 이벤트 발신
	if (Result.bHealthDepleted)
	{
		Send(Src, RogueTags::Event_Combat_Kill,  Result.HealthDamage);
		Send(Tgt, RogueTags::Event_Combat_Death, Result.HealthDamage);

		// Event.Team.AllyDeath 는 팀 구성을 아는 GameState 가 처리
	}
}

