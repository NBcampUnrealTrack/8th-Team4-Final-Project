// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Abilities/RogueDashAbility.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tags/GameplayTags.h"

URogueDashAbility::URogueDashAbility()
{
	// 클라이언트 예측
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	CooldownTag = RogueTags::Cooldown_Augment_Dash;
	BaseCooldown = 12.f;

	// 사망 태그가 존재하면 사용 불가
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_IsDead);
}

void URogueDashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 쿨타임 확인과 적용. 실패하면 쿨타임 중인 것
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 Level = GetAbilityLevel(Handle, ActorInfo);

	static const FString DistanceContext(TEXT("URogueDashAbility::DashDistance"));
	static const FString DurationContext(TEXT("URogueDashAbility::DashDuration"));

	const float Distance = DashDistance.GetValueAtLevel(Level, &DistanceContext);
	const float Duration = FMath::Max(0.01f, DashDuration.GetValueAtLevel(Level, &DurationContext));

	const FVector TargetLocation =
		Character->GetActorLocation() + ResolveDashDirection(*Character) * Distance;

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	UAbilityTask_ApplyRootMotionMoveToForce* Task =
		UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
			this, FName(TEXT("AugmentDash")), TargetLocation, Duration,
			/*bSetNewMovementMode*/ false, MOVE_None,
			/*bRestrictSpeedToExpected*/ false,
			/*PathOffsetCurve*/ nullptr,

			// 대시가 끝난 뒤 가속은 0으로 고정
			ERootMotionFinishVelocityMode::ClampVelocity,
			FVector::ZeroVector,
			Movement ? Movement->MaxWalkSpeed : 0.f);

	if (!Task)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 목표에 도달하거나, 시간이 끝나면 대시 종료
	Task->OnTimedOutAndDestinationReached.AddDynamic(this, &URogueDashAbility::HandleDashFinished);
	Task->OnTimedOut.AddDynamic(this, &URogueDashAbility::HandleDashFinished);

	Task->ReadyForActivation();
}

FVector URogueDashAbility::ResolveDashDirection(const ACharacter& Character) const
{
	const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();

	FVector Direction = Movement ? Movement->GetCurrentAcceleration() : FVector::ZeroVector;
	Direction.Z = 0.f;

	if (Direction.IsNearlyZero())
	{
		// 제자리에서 눌렀으면 바라보는 쪽으로
		Direction = Character.GetActorForwardVector();
		Direction.Z = 0.f;
	}

	return Direction.GetSafeNormal();
}

void URogueDashAbility::HandleDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
