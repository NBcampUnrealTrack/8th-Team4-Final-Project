// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Abilities/RogueReconDroneAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GAS/RogueTargetFilter.h"
#include "Tags/GameplayTags.h"

URogueReconDroneAbility::URogueReconDroneAbility()
{
	// 어빌리티는 클라이언트에서 예측
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	CooldownTag = RogueTags::Cooldown_Augment_ReconDrone;
	BaseCooldown = 25.f;

	// 동시 행동 정책은 공통 증강 부모가 아니라 각 증강에서 명시한다.
	ActivationOwnedTags.AddTag(RogueTags::State_Gameplay_ActionInProgress);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_DataJack_Interacting);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_IsDead);
}

void URogueReconDroneAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                             const FGameplayAbilityActorInfo* ActorInfo,
                                             const FGameplayAbilityActivationInfo ActivationInfo,
                                             const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar || !GetWorld())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 트레이스는 서버에서만 실행
	if (!HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const int32 Level = GetAbilityLevel(Handle, ActorInfo);

	static const FString RangeContext(TEXT("URogueReconDroneAbility::ScanRange"));
	static const FString RadiusContext(TEXT("URogueReconDroneAbility::ScanRadius"));
	static const FString DurationContext(TEXT("URogueReconDroneAbility::RevealDuration"));
	static const FString TargetsContext(TEXT("URogueReconDroneAbility::MaxTargets"));

	const float Range = FMath::Max(1.f, ScanRange.GetValueAtLevel(Level, &RangeContext));
	const float Radius = FMath::Max(1.f, ScanRadius.GetValueAtLevel(Level, &RadiusContext));
	const float Duration = FMath::Max(0.01f, RevealDuration.GetValueAtLevel(Level, &DurationContext));
	const int32 MaxCount = FMath::Max(1, MaxTargets.AsInteger(Level, &TargetsContext));

	FVector ViewLocation;
	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams Params(FName(TEXT("RogueReconDrone")), false, Avatar);

	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(
		Hits,
		ViewLocation,
		ViewLocation + ViewRotation.Vector() * Range,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		Params);

	TArray<AActor*> Targets;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();

		const ERogueTargetRelation Relation = URogueTargetFilter::GetRelation(Avatar, HitActor);


		// 게임플로우 팀 판정이 연결된 대상 중 적만 감지한다.
		const bool bHostile = Relation == ERogueTargetRelation::Hostile;

		if (!bHostile || !URogueTargetFilter::IsAlive(HitActor))
		{
			continue;
		}

		Targets.AddUnique(HitActor);

		if (Targets.Num() >= MaxCount)
		{
			break;
		}
	}

	for (AActor* Target : Targets)
	{
		ApplyReveal(Handle, ActorInfo, ActivationInfo, Target, Duration, Level);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void URogueReconDroneAbility::ApplyReveal(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo,
                                          AActor* Target, float Duration, int32 Level) const
{
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);

	if (!SourceASC || !TargetASC)
	{
		return;
	}

	UClass* StatusEffectClass = GetStatusEffectClass();
	if (!StatusEffectClass)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo, StatusEffectClass, Level);

	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Magnitude_Duration, Duration);

	// 감지 태그 부여
	SpecHandle.Data->DynamicGrantedTags.AddTag(RogueTags::State_Combat_Revealed);

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
}
