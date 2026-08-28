// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/RogueEffectApplier.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GAS/Damage/RogueDamageSubsystem.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogEffectApplier, Log, All);

void URogueEffectApplier::ResolveTargets(const FRogueEffectPayload& Payload, TArray<AActor*>& OutTargets)
{
	AActor* Instigator = Payload.Instigator.Get();

	if (Payload.Scope == ERogueTargetScope::Single)
	{
		if (AActor* Single = Payload.SingleTarget.Get())
		{
			OutTargets.Add(Single);
		}
		return;
	}

	if (Payload.Query.bIncludeSelf && IsValid(Instigator))
	{
		OutTargets.Add(Instigator);
	}

	FRogueTargetQuery Query = Payload.Query;
	Query.bIncludeSelf = false;

	if (Payload.Scope == ERogueTargetScope::TeamWide)
	{
		URogueTargetFilter::GatherTeamWide(Instigator, Query, OutTargets);
		return;
	}

	if (Payload.Scope == ERogueTargetScope::AimSweep)
	{
		URogueTargetFilter::GatherInAimSweep(Instigator, Payload.Origin, Payload.Direction,
			Payload.Length, Payload.Radius, Query, OutTargets);
		return;
	}

	URogueTargetFilter::GatherInRadius(Instigator, Payload.Origin, Payload.Radius, Query, OutTargets);
}

void URogueEffectApplier::Apply(const FRogueEffectPayload& Payload)
{
	AActor* Instigator = Payload.Instigator.Get();

	// 폰이 파괴돼도 효과 적용되어야 함
	UAbilitySystemComponent* SourceASC = Payload.InstigatorASC.Get();
	if (!SourceASC)
	{
		SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
	}

	const bool bHasDamage = Payload.FlatDamage > 0.f;
	if (!SourceASC || (Payload.EffectClasses.IsEmpty() && !bHasDamage))
	{
		return;
	}

	TArray<AActor*> Targets;
	ResolveTargets(Payload, Targets);

	for (AActor* TargetActor : Targets)
	{
		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

		if (!TargetASC)
		{
			continue;
		}

		for (const TSubclassOf<UGameplayEffect>& EffectClass : Payload.EffectClasses)
		{
			if (!EffectClass)
			{
				continue;
			}

			FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
			Context.AddInstigator(Instigator, Instigator);

			const FGameplayEffectSpecHandle SpecHandle =
				SourceASC->MakeOutgoingSpec(EffectClass, Payload.Level, Context);

			if (!SpecHandle.IsValid())
			{
				UE_LOG(LogEffectApplier, Error,
					TEXT("[EffectApplier] %s 스펙 생성에 실패했습니다"), *GetNameSafe(EffectClass));
				continue;
			}

			for (const TPair<FGameplayTag, float>& Magnitude : Payload.Magnitudes)
			{
				SpecHandle.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
			}

			if (Payload.bSplitIntoIntegerTicks)
			{
				SplitIntoIntegerTicks(SpecHandle, EffectClass, Payload.Level);
			}

			SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
		}

		if (bHasDamage)
		{
			ApplyDamage(Payload, Instigator, TargetActor);
		}
	}
}

void URogueEffectApplier::ApplyDamage(const FRogueEffectPayload& Payload,
                                      AActor* Instigator, AActor* TargetActor)
{
	const UWorld* World = IsValid(TargetActor) ? TargetActor->GetWorld() : nullptr;
	URogueDamageSubsystem* Subsystem = World ? World->GetSubsystem<URogueDamageSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogEffectApplier, Error, TEXT("[EffectApplier] RogueDamageSubsystem 을 얻지 못했습니다"));
		return;
	}

	// 원점에서 멀수록 감쇄. 범위가 아니면 전부 원본 대미지
	float Scale = 1.f;
	if (Payload.Scope == ERogueTargetScope::Radius && Payload.Radius > 0.f)
	{
		const float Distance = FVector::Dist(Payload.Origin, TargetActor->GetActorLocation());
		const float Alpha = FMath::Clamp(Distance / Payload.Radius, 0.f, 1.f);

		Scale = FMath::Lerp(1.f, FMath::Clamp(Payload.FalloffAtEdge, 0.f, 1.f), Alpha);
	}

	const float Damage = Payload.FlatDamage * Scale;
	if (Damage <= 0.f)
	{
		return;
	}

	FRogueDamageParams Params;
	Params.Instigator = Instigator;
	Params.Causer     = Instigator;
	Params.SourceTag  = Payload.DamageSourceTag.IsValid()
		? Payload.DamageSourceTag
		: RogueTags::Damage_Source_Skill;

	Subsystem->ApplyFlatDamage(Params, TargetActor, Damage);
}

void URogueEffectApplier::SplitIntoIntegerTicks(const FGameplayEffectSpecHandle& SpecHandle,
                                                UClass* EffectClass, const int32 Level)
{
	const UGameplayEffect* EffectCDO = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (!EffectCDO)
	{
		return;
	}

	static const FString PeriodContext(TEXT("URogueEffectApplier::Period"));
	const float Period = EffectCDO->Period.GetValueAtLevel(Level, &PeriodContext);

	// 주기가 없으면 원본 값 적용
	if (Period <= 0.f)
	{
		return;
	}

	const float TotalAmount =
		SpecHandle.Data->GetSetByCallerMagnitude(RogueTags::Data_Magnitude_Primary, false, 0.f);
	const float TargetDuration =
		SpecHandle.Data->GetSetByCallerMagnitude(RogueTags::Data_Magnitude_Duration, false, 0.f);

	if (TotalAmount <= 0.f || TargetDuration <= 0.f)
	{
		UE_LOG(LogEffectApplier, Warning,
			TEXT("[EffectApplier] %s 를 틱으로 나누지 못했습니다. ")
			TEXT("레벨 수치에 Primary 와 Duration 이 있는지 확인하세요"), *GetNameSafe(EffectClass));
		return;
	}

	const int32 Amount = FMath::RoundToInt(TotalAmount);
	const int32 MaxTicks = FMath::Max(1, FMath::FloorToInt(TargetDuration / Period));

	// 틱당 1 이 기본, 나누어 떨어지는 값까지 1씩 증가
	int32 PerTick = FMath::Max(1, FMath::DivideAndRoundUp(Amount, MaxTicks));

	while (PerTick < Amount && Amount % PerTick != 0)
	{
		++PerTick;
	}

	const int32 Ticks = FMath::Max(1, Amount / PerTick);

	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Magnitude_Primary, PerTick);
	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Magnitude_Duration, Period * (Ticks + 0.5f));

	// 실제 Duration 과 목표 Duration 이 10% 이상 차이나면 경고
	const float TickSpan = Period * Ticks;
	if (!FMath::IsNearlyEqual(TickSpan, TargetDuration, TargetDuration * 0.1f))
	{
		UE_LOG(LogEffectApplier, Warning,
			TEXT("[EffectApplier] 총량 %d 을 틱당 %d 로 %d 틱에 나누면 %.2f 초가 되어 목표 %.2f 초와 어긋납니다. ")
			TEXT("%s 의 Period(%.4f) 나 레벨 수치를 조정하세요"),
			Amount, PerTick, Ticks, TickSpan, TargetDuration, *GetNameSafe(EffectClass), Period);
	}
}
