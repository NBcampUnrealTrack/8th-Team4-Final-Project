#include "Abilities/GA_DominationGimmickHold.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Actor/DominationPointGimmick.h"
#include "Combat/CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Player/ShooterPlayerController.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

// 기믹 상호작용 실패 원인 진단용
DEFINE_LOG_CATEGORY_STATIC(LogDominationGimmickHold, Log, All);

UGA_DominationGimmickHold::UGA_DominationGimmickHold()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RogueTags::State_Gameplay_InputBlocked);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_IsDead);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_ActionBlocked);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_DataJack_Interacting);
	ActivationBlockedTags.AddTag(RogueTags::State_Gameplay_ActionInProgress);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_IsReloading);

	// 상호작용 중 사격 등 다른 행동을 막는 기존 목표 상호작용 태그를 재사용한다.
	ActivationOwnedTags.AddTag(RogueTags::State_Combat_DataJack_Interacting);
}

bool UGA_DominationGimmickHold::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AShooterPlayerController* PlayerController = ResolvePlayerController(ActorInfo);
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->CanProcessActionInput() || !IsValid(AvatarPawn))
	{
		UE_LOG(LogDominationGimmickHold, Verbose, TEXT("[GimmickHold] 거절: 컨트롤러/폰 없음 또는 액션 입력 불가"));
		return false;
	}

	// GA_DataJackInteract와 같이 무기 동작이 끝난 뒤에만 시작한다.
	const UCombatComponent* CombatComponent = AvatarPawn->FindComponentByClass<UCombatComponent>();
	const AWeaponBase* EquippedWeapon = IsValid(CombatComponent)
		? CombatComponent->GetEquippedWeapon()
		: nullptr;
	if ((IsValid(CombatComponent) && !CombatComponent->IsWeaponReady())
		|| (IsValid(EquippedWeapon) && EquippedWeapon->IsReloading()))
	{
		UE_LOG(LogDominationGimmickHold, Verbose, TEXT("[GimmickHold] 거절: 무기 준비 중 또는 재장전 중"));
		return false;
	}

	if (IsValid(ADominationPointGimmick::FindUsableGimmick(AvatarPawn)))
	{
		return true;
	}

	// 진단: 겹친 기믹이 있는데 쓸 수 없는지, 아예 겹친 기믹이 없는지 구분한다.
	TArray<AActor*> OverlappingGimmicks;
	AvatarPawn->GetOverlappingActors(OverlappingGimmicks, ADominationPointGimmick::StaticClass());
	UE_LOG(LogDominationGimmickHold, Verbose,
		TEXT("[GimmickHold] 거절: 사용 가능한 기믹 없음 (%s, 겹친 기믹 %d개)"),
		AvatarPawn->HasAuthority() ? TEXT("서버") : TEXT("클라"),
		OverlappingGimmicks.Num());
	return false;
}

void UGA_DominationGimmickHold::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	AShooterPlayerController* PlayerController = ResolvePlayerController(ActorInfo);
	if (!IsValid(PlayerController) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	OperationController = PlayerController;
	bServerInteractionStarted = false;

	if (HasAuthority(&ActivationInfo))
	{
		ADominationPointGimmick* Gimmick = ADominationPointGimmick::FindUsableGimmick(PlayerController->GetPawn());
		if (!IsValid(Gimmick) || !Gimmick->BeginInteraction(PlayerController))
		{
			OperationController.Reset();
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		BoundGimmick = Gimmick;
		InteractionEndedDelegateHandle = Gimmick->OnInteractionEnded.AddUObject(
			this,
			&ThisClass::HandleInteractionEnded);
		bServerInteractionStarted = true;
	}

	if (APawn* Pawn = PlayerController->GetPawn())
	{
		if (UPawnMovementComponent* MovementComponent = Pawn->GetMovementComponent())
		{
			MovementComponent->StopMovementImmediately();
		}
	}

	UAbilityTask_WaitInputRelease* WaitInputRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (!IsValid(WaitInputRelease))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitInputRelease->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	WaitInputRelease->ReadyForActivation();
}

void UGA_DominationGimmickHold::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	APlayerController* PlayerController = OperationController.Get();
	ADominationPointGimmick* Gimmick = BoundGimmick.Get();
	const bool bCancelServerInteraction = HasAuthority(&ActivationInfo)
		&& bServerInteractionStarted
		&& IsValid(PlayerController)
		&& IsValid(Gimmick);

	// 기믹의 종료 통보가 다시 EndAbility로 들어오지 않도록 먼저 해제한다.
	UnbindInteractionEnded();
	bServerInteractionStarted = false;
	OperationController.Reset();

	if (bCancelServerInteraction)
	{
		Gimmick->CancelInteraction(PlayerController);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

AShooterPlayerController* UGA_DominationGimmickHold::ResolvePlayerController(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	if (AShooterPlayerController* PlayerController = Cast<AShooterPlayerController>(ActorInfo->PlayerController.Get()))
	{
		return PlayerController;
	}

	const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	return IsValid(AvatarPawn)
		? Cast<AShooterPlayerController>(AvatarPawn->GetController())
		: nullptr;
}

void UGA_DominationGimmickHold::UnbindInteractionEnded()
{
	if (BoundGimmick.IsValid() && InteractionEndedDelegateHandle.IsValid())
	{
		BoundGimmick->OnInteractionEnded.Remove(InteractionEndedDelegateHandle);
	}

	InteractionEndedDelegateHandle.Reset();
	BoundGimmick.Reset();
}

void UGA_DominationGimmickHold::HandleInputReleased(float)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_DominationGimmickHold::HandleInteractionEnded(
	APlayerController* PlayerController,
	const bool bCompleted)
{
	if (!IsValid(PlayerController) || OperationController.Get() != PlayerController)
	{
		return;
	}

	// 기믹이 이미 상태를 정리했으므로 CancelInteraction을 다시 부르지 않는다.
	bServerInteractionStarted = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bCompleted);
}
