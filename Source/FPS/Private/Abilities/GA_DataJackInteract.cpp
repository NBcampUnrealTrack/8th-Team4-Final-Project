#include "Abilities/GA_DataJackInteract.h"

#include "Actor/DataJack.h"
#include "Actor/DataJackWorldQueries.h"
#include "Combat/CombatComponent.h"
#include "Engine/World.h"
#include "GameMode/BombGameState.h"
#include "GameMode/BombObjectiveManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/BaseTeamPlayerState.h"
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

	if ((IsValid(CombatComponent) && !CombatComponent->IsWeaponReady())
		|| (IsValid(EquippedWeapon) && EquippedWeapon->IsReloading()))
	{
		return false;
	}

	return CanStartInteraction(AvatarPawn);
}

bool UGA_DataJackInteract::CanStartInteraction(const APawn* AvatarPawn) const
{
	if (!IsValid(AvatarPawn))
	{
		return false;
	}

	UWorld* World = AvatarPawn->GetWorld();
	const ABombGameState* BombGameState = IsValid(World) ? World->GetGameState<ABombGameState>() : nullptr;
	ADataJack* DataJack = IsValid(BombGameState) ? BombGameState->GetActiveDataJack() : nullptr;
	const ABaseTeamPlayerState* PlayerState = AvatarPawn->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(DataJack) || !IsValid(PlayerState))
	{
		return false;
	}

	if (DataJack->IsInteractingPlayer(PlayerState))
	{
		return true;
	}

	const EDataJackState ObjectiveState = DataJack->GetDataJackState();
	const ETeamRole PlayerRole = BombGameState->GetTeamRole(PlayerState->GetTeam());
	if (PlayerRole == ETeamRole::Attackers)
	{
		return ObjectiveState == EDataJackState::Carried
			&& DataJack->IsCarriedBy(PlayerState)
			&& DataJack->CanPawnPlant(AvatarPawn, DataJackWorldQueries::FindSiteContainingPawn(World, AvatarPawn));
	}

	return PlayerRole == ETeamRole::Defenders
		&& ObjectiveState == EDataJackState::Planted
		&& DataJack->CanPawnDefuse(AvatarPawn);
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
