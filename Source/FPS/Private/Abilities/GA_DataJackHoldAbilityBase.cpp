#include "Abilities/GA_DataJackHoldAbilityBase.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Engine/World.h"
#include "GameMode/BombGameMode.h"
#include "GameMode/BombObjectiveManager.h"
#include "Player/BaseTeamPlayerState.h"
#include "GameMode/BaseRoundGameState.h"
#include "Player/BaseRoundPlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataJackAbility, Log, All);

UGA_DataJackHoldAbilityBase::UGA_DataJackHoldAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(RogueTags::State_Gameplay_InputBlocked);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_IsDead);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_DataJack_Interacting);
	ActivationBlockedTags.AddTag(RogueTags::State_Combat_DataJack_Dropping);
}

bool UGA_DataJackHoldAbilityBase::CanActivateAbility(
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

	ABaseRoundPlayerController* PlayerController = ResolveRoundPlayerController(ActorInfo);
	const ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	const ABaseRoundGameState* RoundGameState = IsValid(PlayerController) && IsValid(PlayerController->GetWorld())
		? PlayerController->GetWorld()->GetGameState<ABaseRoundGameState>()
		: nullptr;
	if (!IsValid(PlayerController)
		|| !PlayerController->CanProcessActionInput()
		|| !IsValid(PlayerController->GetPawn())
		|| !IsValid(PlayerState)
		|| !PlayerState->IsAlive()
		|| !IsValid(RoundGameState)
		|| RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress)
	{
		return false;
	}

	// 클라이언트는 GameMode를 가지지 않으므로 서버에서만 Manager 존재를 검증한다.
	return !PlayerController->HasAuthority()
		|| IsValid(ResolveBombObjectiveManager(PlayerController));
}

void UGA_DataJackHoldAbilityBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData*)
{
	ABaseRoundPlayerController* PlayerController = ResolveRoundPlayerController(ActorInfo);
	if (!IsValid(PlayerController) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	OperationController = PlayerController;
	bServerOperationStarted = false;

	if (HasAuthority(&ActivationInfo))
	{
		UBombObjectiveManager* ObjectiveManager = ResolveBombObjectiveManager(PlayerController);
		if (!IsValid(ObjectiveManager))
		{
			OperationController.Reset();
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		BindOperationEnded(*ObjectiveManager);
		if (!BeginDataJackOperation(*ObjectiveManager, *PlayerController))
		{
			UE_LOG(LogDataJackAbility, Warning,
				TEXT("[DataJackAbility] %s was rejected for %s. "
				     "Check the player role, carrier/objective state, and interaction site."),
				*GetNameSafe(GetClass()),
				*GetNameSafe(PlayerController));
			UnbindOperationEnded();
			OperationController.Reset();
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}

		bServerOperationStarted = true;
	}

	HandleDataJackOperationStarted(*PlayerController);

	UAbilityTask_WaitInputRelease* WaitInputRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (!IsValid(WaitInputRelease))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	WaitInputRelease->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	WaitInputRelease->ReadyForActivation();
}

void UGA_DataJackHoldAbilityBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	APlayerController* PlayerController = OperationController.Get();
	UBombObjectiveManager* ObjectiveManager = BoundObjectiveManager.Get();
	const bool bCancelServerOperation = HasAuthority(&ActivationInfo)
		&& bServerOperationStarted
		&& IsValid(PlayerController)
		&& IsValid(ObjectiveManager);

	UnbindOperationEnded();
	bServerOperationStarted = false;
	OperationController.Reset();

	if (bCancelServerOperation)
	{
		CancelDataJackOperation(*ObjectiveManager, *PlayerController);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

EDataJackOperation UGA_DataJackHoldAbilityBase::GetDataJackOperation() const
{
	return EDataJackOperation::None;
}

bool UGA_DataJackHoldAbilityBase::BeginDataJackOperation(
	UBombObjectiveManager&,
	APlayerController&)
{
	return false;
}

void UGA_DataJackHoldAbilityBase::CancelDataJackOperation(
	UBombObjectiveManager&,
	APlayerController&)
{
}

void UGA_DataJackHoldAbilityBase::HandleDataJackOperationStarted(APlayerController&)
{
}

ABaseRoundPlayerController* UGA_DataJackHoldAbilityBase::ResolveRoundPlayerController(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	if (ABaseRoundPlayerController* PlayerController = Cast<ABaseRoundPlayerController>(ActorInfo->PlayerController.Get()))
	{
		return PlayerController;
	}

	const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	return IsValid(AvatarPawn)
		? Cast<ABaseRoundPlayerController>(AvatarPawn->GetController())
		: nullptr;
}

UBombObjectiveManager* UGA_DataJackHoldAbilityBase::ResolveBombObjectiveManager(
	APlayerController* PlayerController) const
{
	UWorld* World = IsValid(PlayerController) ? PlayerController->GetWorld() : nullptr;
	ABombGameMode* BombGameMode = IsValid(World) ? World->GetAuthGameMode<ABombGameMode>() : nullptr;
	return IsValid(BombGameMode) ? BombGameMode->GetBombObjectiveManager() : nullptr;
}

void UGA_DataJackHoldAbilityBase::BindOperationEnded(UBombObjectiveManager& ObjectiveManager)
{
	UnbindOperationEnded();
	BoundObjectiveManager = &ObjectiveManager;
	OperationEndedDelegateHandle = ObjectiveManager.GetOperationEndedDelegate().AddUObject(
		this,
		&ThisClass::HandleDataJackOperationEnded);
}

void UGA_DataJackHoldAbilityBase::UnbindOperationEnded()
{
	if (BoundObjectiveManager.IsValid() && OperationEndedDelegateHandle.IsValid())
	{
		BoundObjectiveManager->GetOperationEndedDelegate().Remove(OperationEndedDelegateHandle);
	}

	OperationEndedDelegateHandle.Reset();
	BoundObjectiveManager.Reset();
}

void UGA_DataJackHoldAbilityBase::HandleInputReleased(float)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_DataJackHoldAbilityBase::HandleDataJackOperationEnded(
	const EDataJackOperation Operation,
	APlayerController* PlayerController)
{
	if (Operation != GetDataJackOperation()
		|| !IsValid(PlayerController)
		|| OperationController.Get() != PlayerController)
	{
		return;
	}

	bServerOperationStarted = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
