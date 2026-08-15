#include "Abilities/GA_DataJackInteract.h"

#include "Combat/CombatComponent.h"
#include "GameFlow/Bomb/BombObjectiveManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

UGA_DataJackInteract::UGA_DataJackInteract()
{
	ActivationOwnedTags.AddTag(RogueTags::State_Combat_DataJack_Interacting);
	ActivationBlockedTags.AddTag(RogueTags::State_Gameplay_ActionInProgress);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_IsReloading);
}

bool UGA_DataJackInteract::CanActivateAbility(
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

	// 기존 행동을 강제로 취소하지 않는다. GAS Action 상태와 무기 상태가 모두 끝난 뒤에만
	// 설치/해체를 시작하고, 시작된 이후에는 DataJack.Interacting 태그가 새 행동을 차단한다.
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCombatComponent* CombatComponent = IsValid(AvatarPawn)
		? AvatarPawn->FindComponentByClass<UCombatComponent>()
		: nullptr;
	const AWeaponBase* EquippedWeapon = IsValid(CombatComponent)
		? CombatComponent->GetEquippedWeapon()
		: nullptr;

	return (!IsValid(CombatComponent) || CombatComponent->IsWeaponReady())
		&& (!IsValid(EquippedWeapon) || !EquippedWeapon->IsReloading());
}

EDataJackOperation UGA_DataJackInteract::GetDataJackOperation() const
{
	return EDataJackOperation::Interaction;
}

bool UGA_DataJackInteract::BeginDataJackOperation(
	UBombObjectiveManager& ObjectiveManager,
	APlayerController& PlayerController)
{
	return ObjectiveManager.BeginInteraction(&PlayerController);
}

void UGA_DataJackInteract::CancelDataJackOperation(
	UBombObjectiveManager& ObjectiveManager,
	APlayerController& PlayerController)
{
	ObjectiveManager.CancelInteraction(&PlayerController);
}

void UGA_DataJackInteract::HandleDataJackOperationStarted(APlayerController& PlayerController)
{
	APawn* ControlledPawn = PlayerController.GetPawn();
	if (!IsValid(ControlledPawn))
	{
		return;
	}

	if (UPawnMovementComponent* MovementComponent = ControlledPawn->GetMovementComponent())
	{
		MovementComponent->StopMovementImmediately();
	}
}
