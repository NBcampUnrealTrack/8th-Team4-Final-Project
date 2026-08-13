// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/Damage/RogueDamageSubsystem.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "GAS/RogueGASSettings.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Tags/GameplayTags.h"


// ----------------------------------------------------------------
// 레거시 코드 ApplyHitscanDamage / ApplyFlatDamage 로 대체 후 삭제

namespace
{
	constexpr int32 MaxContextHitZoneParts = 16;

	float ResolveHeadshotEventMagnitude(const FRogueDamageRequest& Request)
	{
		float HeadshotDamage = 0.0f;
		for (const FRogueHitZoneDamagePart& Part : Request.HitZoneDamageParts)
		{
			if (!Part.bHeadshot
				|| !FMath::IsFinite(Part.BaseDamage)
				|| Part.BaseDamage <= 0.0f)
			{
				continue;
			}

			// 이벤트는 헤드 적중량만 알린다. 부위 배율 적용은 ExecCalc_Damage 단일 책임으로 유지한다.
			HeadshotDamage += Part.BaseDamage;
		}

		return HeadshotDamage > 0.0f ? HeadshotDamage : FMath::Max(0.0f, Request.HeadshotDamage);
	}
}

FRogueDamageResult URogueDamageSubsystem::ApplyDamage(const FRogueDamageRequest& Request)
{
	// 검증
	FRogueDamageTarget Target;
	if (!ResolveDamageTarget(Request, Target))
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
	UAbilitySystemComponent* SourceASC = ResolveASC(Request.InstigatorActor);
	UAbilitySystemComponent* ContextASC = SourceASC ? SourceASC : Target.ASC;

	const FGameplayEffectContextHandle ContextHandle = MakeDamageContext(Request, *ContextASC, Target.Vital);

	// 스펙 생성
	const FGameplayEffectSpecHandle SpecHandle = ContextASC->MakeOutgoingSpec(DamageGEClass, 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return FRogueDamageResult();
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Damage_Base, Request.BaseDamage);

	// 대미지 적용 전 체력/실드 수집
	const float ShieldBefore = Target.Vital->GetShield();
	const float HealthBefore = Target.Vital->GetHealth();

	Target.ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// 결과 수집
	const FRogueDamageResult Result = MeasureDamageResult(*Target.Vital, ShieldBefore, HealthBefore);

	// 이벤트 발신
	BroadcastCombatEvents(Request, Result, ContextHandle);

	return Result;
}
// ----------------------------------------------------------------


UAbilitySystemComponent* URogueDamageSubsystem::ResolveASC(AActor* Actor)
{
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
}

// 신규 히트스캔 경로
// 미구현 스텁 구현 후 ApplyDamage 를 대체
FRogueDamageResult URogueDamageSubsystem::ApplyHitscanDamage(const FRogueDamageParams& Params,
                                                             const FRogueHitContext& Hit)
{

	return FRogueDamageResult();
}

// 탄착 기하가 없는 피해, 스킬, 폭발, 자가피해
// 미구현 스텁
FRogueDamageResult URogueDamageSubsystem::ApplyFlatDamage(const FRogueDamageParams& Params,
                                                          AActor* Target,
                                                          float Damage)
{

	return FRogueDamageResult();
}

bool URogueDamageSubsystem::ResolveDamageTarget(const FRogueDamageRequest& Request,
                                                FRogueDamageTarget& OutTarget) const
{
	if (!Request.TargetActor
		|| !FMath::IsFinite(Request.BaseDamage)
		|| Request.BaseDamage <= 0.f)
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = ResolveASC(Request.TargetActor);
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

FGameplayEffectContextHandle URogueDamageSubsystem::MakeDamageContext(const FRogueDamageRequest& Request,
                                                                     UAbilitySystemComponent& ContextASC,
                                                                     const URogueVitalAttributeSet* TargetVital) const
{
	FGameplayEffectContextHandle ContextHandle = ContextASC.MakeEffectContext();
	ContextHandle.AddInstigator(Request.InstigatorActor, Request.CauserActor);

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

	Ctx->SourceTag           = Request.SourceTag;
	Ctx->WeaponTag           = Request.WeaponTag;
	Ctx->HitZoneTag          = Request.HitZoneTag;
	Ctx->HitDistanceMeters   = Request.DistanceMeters;
	Ctx->PelletsHit          = FMath::Max(1, Request.PelletsHit);
	Ctx->ShotIndexInMagazine = Request.ShotIndexInMagazine;

	Ctx->HitZoneDamageParts.Reset(FMath::Min(Request.HitZoneDamageParts.Num(), MaxContextHitZoneParts));
	for (const FRogueHitZoneDamagePart& Part : Request.HitZoneDamageParts)
	{
		if (Ctx->HitZoneDamageParts.Num() >= MaxContextHitZoneParts)
		{
			break;
		}
		if (!FMath::IsFinite(Part.BaseDamage)
			|| !FMath::IsFinite(Part.HitZoneMultiplier)
			|| Part.BaseDamage <= 0.0f
			|| Part.HitZoneMultiplier < 0.0f)
		{
			continue;
		}

		FRogueHitZoneDamagePart& Sanitized = Ctx->HitZoneDamageParts.AddDefaulted_GetRef();
		Sanitized = Part;
		Sanitized.BaseDamage = FMath::Max(0.0f, Part.BaseDamage);
		Sanitized.HitZoneMultiplier = FMath::Max(0.0f, Part.HitZoneMultiplier);
		Sanitized.PelletsHit = FMath::Max(1, Part.PelletsHit);
	}

	// 기존 비-Hitscan 호출자 호환용 경로.
	Ctx->HeadshotDamage = FMath::Max(0.0f, Request.HeadshotDamage);
	Ctx->AppliedHeadshotMultiplier = FMath::Max(KINDA_SMALL_NUMBER, Request.AppliedHeadshotMultiplier);

	BuildContextTags(Request, TargetVital, Ctx->ContextTags);

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

void URogueDamageSubsystem::BuildContextTags(const FRogueDamageRequest& Request,
                                             const URogueVitalAttributeSet* TargetVital,
                                             FGameplayTagContainer& OutTags) const
{
	if (Request.HitZoneTag.IsValid())
	{
		OutTags.AddTag(Request.HitZoneTag);
	}

	if (Request.SourceTag.IsValid())
	{
		OutTags.AddTag(Request.SourceTag);
	}

	if (Request.bIsBackstab)
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

void URogueDamageSubsystem::BroadcastCombatEvents(const FRogueDamageRequest& Request,
                                                  const FRogueDamageResult& Result,
                                                  const FGameplayEffectContextHandle& ContextHandle) const
{
	if (!Result.bApplied)
	{
		return;
	}

	auto Send = [&ContextHandle](AActor* To, const FGameplayTag& Tag, AActor* Other, float Magnitude)
	{
		if (!To || !Tag.IsValid())
		{
			return;
		}

		FGameplayEventData Payload;
		Payload.EventTag       = Tag;
		Payload.Instigator     = To;
		Payload.Target         = Other;
		Payload.EventMagnitude = Magnitude;
		Payload.ContextHandle = ContextHandle;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(To, Tag, Payload);
	};

	AActor* Src = Request.InstigatorActor;
	AActor* Tgt = Request.TargetActor;

	Send(Src, RogueTags::Event_Combat_DamageDealt, Tgt, Result.HealthDamage);
	Send(Tgt, RogueTags::Event_Combat_DamageTaken, Src, Result.HealthDamage);

	const float HeadshotEventMagnitude = ResolveHeadshotEventMagnitude(Request);
	if (HeadshotEventMagnitude > 0.0f)
	{
		Send(Src, RogueTags::Event_Combat_Headshot, Tgt, HeadshotEventMagnitude);
	}

	if (Result.bShieldBroken)
	{
		Send(Tgt, RogueTags::Event_Combat_ShieldBroken,      Src, Result.ShieldDamage);
		Send(Src, RogueTags::Event_Combat_EnemyShieldBroken, Tgt, Result.ShieldDamage);
	}

	// 사망 이벤트 발신
	if (Result.bHealthDepleted)
	{
		Send(Src, RogueTags::Event_Combat_Kill,  Tgt, Result.HealthDamage);
		Send(Tgt, RogueTags::Event_Combat_Death, Src, Result.HealthDamage);

		// Event.Team.AllyDeath 는 팀 구성을 아는 GameState 가 처리
	}
}

