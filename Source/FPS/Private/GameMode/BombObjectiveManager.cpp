#include "GameMode/BombObjectiveManager.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameMode/BombGameMode.h"
#include "GameMode/BombGameState.h"
#include "Actor/DataJack.h"
#include "Actor/DataJackSiteVolume.h"
#include "Actor/DataJackSpawnPoint.h"
#include "Player/BaseTeamPlayerState.h"
#include "GameMode/BaseRoundGameState.h"
#include "Player/BaseRoundPlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogBombObjectiveManager, Log, All);

void UBombObjectiveManager::Initialize(ABombGameMode* InOwnerGameMode)
{
	Shutdown();
	OwnerGameMode = InOwnerGameMode;
}

void UBombObjectiveManager::Shutdown()
{
	DestroyObjective();
	OperationEndedDelegate.Clear();
	ClearCachedMapConfiguration();
	OwnerGameMode.Reset();
}

bool UBombObjectiveManager::PrepareForRound(const FBombObjectiveConfig& Config)
{
	if (!EnsureMapConfiguration())
	{
		return false;
	}

	DestroyObjective();
	return SpawnObjective(Config);
}

bool UBombObjectiveManager::ActivateForRound()
{
	if (!IsValid(ActiveDataJack))
	{
		return false;
	}

	ActiveDataJack->ActivateForRound();
	return ActiveDataJack->IsRoundActive();
}

void UBombObjectiveManager::DeactivateForRound()
{
	if (IsValid(ActiveDataJack))
	{
		ActiveDataJack->DeactivateForRound();
	}
}

void UBombObjectiveManager::ResetObjective()
{
	DestroyObjective();
}

bool UBombObjectiveManager::BeginInteraction(APlayerController* PlayerController)
{
	ABaseTeamPlayerState* PlayerState = nullptr;
	if (!IsRequestValid(PlayerController, PlayerState) || !IsValid(ActiveDataJack))
	{
		return false;
	}

	const ABombGameMode* GameMode = GetOwnerGameMode();
	const ABaseRoundGameState* RoundGameState = IsValid(GameMode)
		? GameMode->GetGameState<ABaseRoundGameState>()
		: nullptr;
	const ETeamRole PlayerRole = IsValid(RoundGameState)
		? RoundGameState->GetTeamRole(PlayerState->GetTeam())
		: ETeamRole::None;
	const EDataJackState ObjectiveState = ActiveDataJack->GetDataJackState();

	if (ActiveDataJack->IsInteractingPlayer(PlayerState))
	{
		return (PlayerRole == ETeamRole::Attackers && ObjectiveState == EDataJackState::Planting)
			|| (PlayerRole == ETeamRole::Defenders && ObjectiveState == EDataJackState::Defusing);
	}

	if (PlayerRole == ETeamRole::Attackers
		&& ObjectiveState == EDataJackState::Carried
		&& ActiveDataJack->IsCarriedBy(PlayerState))
	{
		ADataJackSiteVolume* SiteVolume = FindPlantSiteForPawn(PlayerController->GetPawn());
		return IsValid(SiteVolume)
			&& ActiveDataJack->BeginPlanting(PlayerController, PlayerState, SiteVolume);
	}

	return PlayerRole == ETeamRole::Defenders
		&& ObjectiveState == EDataJackState::Planted
		&& ActiveDataJack->BeginDefusing(PlayerController, PlayerState);
}

void UBombObjectiveManager::CancelInteraction(APlayerController* PlayerController)
{
	ABombGameMode* GameMode = GetOwnerGameMode();
	if (IsValid(GameMode)
		&& GameMode->HasAuthority()
		&& IsValid(ActiveDataJack)
		&& IsValid(PlayerController))
	{
		ActiveDataJack->CancelInteraction(PlayerController);
	}
}

bool UBombObjectiveManager::BeginDrop(APlayerController* PlayerController)
{
	ABaseTeamPlayerState* PlayerState = nullptr;
	return IsRequestValid(PlayerController, PlayerState)
		&& IsValid(ActiveDataJack)
		&& ActiveDataJack->BeginDropHold(PlayerController, PlayerState);
}

void UBombObjectiveManager::CancelDrop(APlayerController* PlayerController)
{
	ABombGameMode* GameMode = GetOwnerGameMode();
	if (IsValid(GameMode)
		&& GameMode->HasAuthority()
		&& IsValid(ActiveDataJack)
		&& IsValid(PlayerController))
	{
		ActiveDataJack->CancelDropHold(PlayerController);
	}
}

bool UBombObjectiveManager::TryPickup(APawn* PlayerPawn)
{
	ABombGameMode* GameMode = GetOwnerGameMode();
	if (!IsValid(GameMode)
		|| !GameMode->HasAuthority()
		|| !IsValid(PlayerPawn)
		|| !IsValid(ActiveDataJack))
	{
		return false;
	}

	AController* PlayerController = PlayerPawn->GetController();
	ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	const ABaseRoundGameState* RoundGameState = GameMode->GetGameState<ABaseRoundGameState>();
	if (!IsValid(PlayerState)
		|| !PlayerState->IsAlive()
		|| !IsValid(RoundGameState)
		|| RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress
		|| RoundGameState->GetTeamRole(PlayerState->GetTeam()) != ETeamRole::Attackers)
	{
		return false;
	}

	return ActiveDataJack->TryPickup(PlayerPawn, PlayerState);
}

void UBombObjectiveManager::NotifyInteractionStopped(APlayerController* PlayerController)
{
	if (IsValid(PlayerController))
	{
		OperationEndedDelegate.Broadcast(EDataJackOperation::Interaction, PlayerController);
	}
}

void UBombObjectiveManager::NotifyDropStopped(APlayerController* PlayerController)
{
	if (IsValid(PlayerController))
	{
		OperationEndedDelegate.Broadcast(EDataJackOperation::Drop, PlayerController);
	}
}

FOnDataJackOperationEnded& UBombObjectiveManager::GetOperationEndedDelegate()
{
	return OperationEndedDelegate;
}

void UBombObjectiveManager::HandlePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState)
{
	if (IsValid(ActiveDataJack))
	{
		ActiveDataJack->HandlePlayerEliminated(EliminatedPlayerState);
	}
}

bool UBombObjectiveManager::IsActiveDataJack(const ADataJack* DataJack) const
{
	return IsValid(DataJack) && DataJack == ActiveDataJack.Get();
}

bool UBombObjectiveManager::CanCompletePlanting(const ADataJack* DataJack) const
{
	const ABombGameMode* GameMode = GetOwnerGameMode();
	const ABaseRoundGameState* RoundGameState = IsValid(GameMode)
		? GameMode->GetGameState<ABaseRoundGameState>()
		: nullptr;
	return IsActiveDataJack(DataJack)
		&& IsValid(GameMode)
		&& GameMode->HasAuthority()
		&& IsValid(RoundGameState)
		&& RoundGameState->GetGameFlowPhase() == EGameFlowPhase::RoundInProgress
		&& RoundGameState->GetRoundTimeRemaining() > 0.0f;
}

void UBombObjectiveManager::NotifyDataJackPlanted(ADataJack* DataJack)
{
	ABombGameMode* GameMode = GetOwnerGameMode();
	if (IsValid(GameMode) && GameMode->HasAuthority() && IsActiveDataJack(DataJack))
	{
		GameMode->NotifyDataJackPlanted(DataJack);
	}
}

void UBombObjectiveManager::NotifyDataJackDefused(ADataJack* DataJack)
{
	ABombGameMode* GameMode = GetOwnerGameMode();
	if (IsValid(GameMode) && GameMode->HasAuthority() && IsActiveDataJack(DataJack))
	{
		GameMode->NotifyDataJackDefused(DataJack);
	}
}

void UBombObjectiveManager::NotifyDataJackDetonated(ADataJack* DataJack)
{
	ABombGameMode* GameMode = GetOwnerGameMode();
	if (IsValid(GameMode) && GameMode->HasAuthority() && IsActiveDataJack(DataJack))
	{
		GameMode->NotifyDataJackDetonated(DataJack);
	}
}

bool UBombObjectiveManager::IsPlantedOrDefusing() const
{
	const EDataJackState State = GetObjectiveState();
	return State == EDataJackState::Planted || State == EDataJackState::Defusing;
}

EDataJackState UBombObjectiveManager::GetObjectiveState() const
{
	return IsValid(ActiveDataJack) ? ActiveDataJack->GetDataJackState() : EDataJackState::Idle;
}

bool UBombObjectiveManager::SpawnObjective(const FBombObjectiveConfig& Config)
{
	ABombGameMode* GameMode = GetOwnerGameMode();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	ADataJackSpawnPoint* SpawnPoint = CachedSpawnPoint.Get();
	if (!IsValid(World)
		|| !IsValid(SpawnPoint)
		|| !Config.DataJackClass
		|| Config.DataJackClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveDataJack = World->SpawnActor<ADataJack>(Config.DataJackClass, SpawnPoint->GetActorTransform(), SpawnParameters);
	if (!IsValid(ActiveDataJack))
	{
		UE_LOG(LogBombObjectiveManager, Error, TEXT("Failed to spawn the Data Jack actor."));
		return false;
	}

	ActiveDataJack->ConfigureForMode(
		Config.PlantDuration,
		Config.CountdownDuration,
		Config.DefuseDuration,
		Config.DropHoldDuration,
		Config.InteractionValidationInterval);
	ActiveDataJack->InitializeForRound(SpawnPoint->GetActorTransform(), this);

	ABombGameState* BombGameState = GameMode->GetGameState<ABombGameState>();
	if (!IsValid(BombGameState))
	{
		UE_LOG(LogBombObjectiveManager, Error, TEXT("BombGameState is unavailable; the objective cannot be published."));
		DestroyObjective();
		return false;
	}

	BombGameState->SetActiveDataJack(ActiveDataJack);
	return true;
}

void UBombObjectiveManager::DestroyObjective()
{
	ABombGameMode* GameMode = GetOwnerGameMode();
	if (IsValid(GameMode))
	{
		if (ABombGameState* BombGameState = GameMode->GetGameState<ABombGameState>())
		{
			BombGameState->SetActiveDataJack(nullptr);
		}
	}

	if (IsValid(ActiveDataJack))
	{
		ActiveDataJack->DeactivateForRound();
		ActiveDataJack->Destroy();
	}

	ActiveDataJack = nullptr;
}

bool UBombObjectiveManager::EnsureMapConfiguration()
{
	return IsCachedMapConfigurationValid() || RefreshMapConfiguration();
}

bool UBombObjectiveManager::RefreshMapConfiguration()
{
	ClearCachedMapConfiguration();

	const ABombGameMode* GameMode = GetOwnerGameMode();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return false;
	}

	int32 SpawnPointCount = 0;
	for (TActorIterator<ADataJackSpawnPoint> Iterator(World); Iterator; ++Iterator)
	{
		ADataJackSpawnPoint* SpawnPoint = *Iterator;
		if (!IsValid(SpawnPoint))
		{
			continue;
		}

		CachedSpawnPoint = SpawnPoint;
		++SpawnPointCount;
	}

	if (SpawnPointCount != 1)
	{
		UE_LOG(LogBombObjectiveManager, Error,
			TEXT("Bomb mode requires exactly one DataJackSpawnPoint, but found %d."),
			SpawnPointCount);
		ClearCachedMapConfiguration();
		return false;
	}

	TSet<FName> SiteIds;
	for (TActorIterator<ADataJackSiteVolume> Iterator(World); Iterator; ++Iterator)
	{
		ADataJackSiteVolume* SiteVolume = *Iterator;
		if (!IsValid(SiteVolume))
		{
			continue;
		}

		const FName SiteId = SiteVolume->GetSiteId();
		if (SiteId.IsNone() || SiteIds.Contains(SiteId))
		{
			UE_LOG(LogBombObjectiveManager, Error, TEXT("Every DataJackSiteVolume requires a unique, non-empty SiteId."));
			ClearCachedMapConfiguration();
			return false;
		}

		SiteIds.Add(SiteId);
		CachedPlantSites.Add(SiteVolume);
	}

	if (CachedPlantSites.IsEmpty())
	{
		UE_LOG(LogBombObjectiveManager, Error, TEXT("Bomb mode requires at least one DataJackSiteVolume."));
		ClearCachedMapConfiguration();
		return false;
	}

	return true;
}

bool UBombObjectiveManager::IsCachedMapConfigurationValid() const
{
	if (!CachedSpawnPoint.IsValid() || CachedPlantSites.IsEmpty())
	{
		return false;
	}

	for (const TWeakObjectPtr<ADataJackSiteVolume>& SiteVolume : CachedPlantSites)
	{
		if (!SiteVolume.IsValid())
		{
			return false;
		}
	}

	return true;
}

void UBombObjectiveManager::ClearCachedMapConfiguration()
{
	CachedSpawnPoint.Reset();
	CachedPlantSites.Reset();
}

ADataJackSiteVolume* UBombObjectiveManager::FindPlantSiteForPawn(const APawn* PlayerPawn) const
{
	if (!IsValid(PlayerPawn))
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<ADataJackSiteVolume>& SiteReference : CachedPlantSites)
	{
		ADataJackSiteVolume* SiteVolume = SiteReference.Get();
		if (IsValid(SiteVolume) && SiteVolume->ContainsPawn(PlayerPawn))
		{
			return SiteVolume;
		}
	}

	return nullptr;
}

bool UBombObjectiveManager::IsRequestValid(
	const APlayerController* PlayerController,
	ABaseTeamPlayerState*& OutPlayerState) const
{
	OutPlayerState = nullptr;
	const ABombGameMode* GameMode = GetOwnerGameMode();
	const ABaseRoundPlayerController* RoundPlayerController = Cast<ABaseRoundPlayerController>(PlayerController);
	const ABaseRoundGameState* RoundGameState = IsValid(GameMode)
		? GameMode->GetGameState<ABaseRoundGameState>()
		: nullptr;
	// 서버 Ability가 ActivationOwnedTag를 이미 가진 상태에서 들어오므로
	// Action 게이트가 아니라 공통 라운드/사망/전체 차단 게이트만 확인한다.
	if (!IsValid(GameMode)
		|| !GameMode->HasAuthority()
		|| !IsValid(RoundPlayerController)
		|| !RoundPlayerController->HasAuthority()
		|| RoundPlayerController->GetWorld() != GameMode->GetWorld()
		|| !RoundPlayerController->CanProcessGameplayInput()
		|| !IsValid(RoundGameState)
		|| RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress
		|| !IsValid(RoundPlayerController->GetPawn()))
	{
		return false;
	}

	OutPlayerState = RoundPlayerController->GetPlayerState<ABaseTeamPlayerState>();
	return IsValid(OutPlayerState)
		&& OutPlayerState->IsAlive()
		&& OutPlayerState->GetTeam() != EGameTeam::None;
}

ABombGameMode* UBombObjectiveManager::GetOwnerGameMode() const
{
	return OwnerGameMode.Get();
}
