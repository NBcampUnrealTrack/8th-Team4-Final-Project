#include "Actor/DataJack.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameMode/BombObjectiveManager.h"
#include "Actor/DataJackSiteVolume.h"
#include "Actor/DataJackWorldQueries.h"
#include "Player/BaseTeamPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

FVector ADataJack::GetInteractionWorldLocation() const
{
	return IsValid(InteractionPoint) ? InteractionPoint->GetComponentLocation() : GetActorLocation();
}

bool ADataJack::BeginPlanting(
	APlayerController* PlayerController,
	ABaseTeamPlayerState* PlayerState,
	ADataJackSiteVolume* SiteVolume)
{
	const APawn* PlayerPawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
	if (!HasAuthority()
		|| !bRoundActive
		|| DataJackState != EDataJackState::Carried
		|| !IsValid(PlayerController)
		|| !IsValid(PlayerState)
		|| !IsCarriedBy(PlayerState)
		|| CarrierPawn.Get() != PlayerPawn
		|| !CanPawnPlant(PlayerPawn, SiteVolume))
	{
		return false;
	}

	FinishDropHold();
	InteractionController = PlayerController;
	InteractionStartPawnLocation = PlayerPawn->GetActorLocation();
	ActivePlantSite = SiteVolume;
	SetInteractingPlayerState(PlayerState);
	InteractionStartServerTime = GetSynchronizedServerTime();
	InteractionEndServerTime = InteractionStartServerTime + PlantDuration;
	SetDataJackState(EDataJackState::Planting);
	BroadcastDataChanged();

	GetWorldTimerManager().SetTimer(
		InteractionCompletionTimerHandle,
		this,
		&ADataJack::CompleteActiveInteraction,
		PlantDuration,
		false);
	GetWorldTimerManager().SetTimer(
		InteractionValidationTimerHandle,
		this,
		&ADataJack::ValidateActiveInteraction,
		InteractionValidationInterval,
		true);
	return true;
}

bool ADataJack::BeginDefusing(APlayerController* PlayerController, ABaseTeamPlayerState* PlayerState)
{
	const APawn* PlayerPawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
	if (!HasAuthority()
		|| !bRoundActive
		|| DataJackState != EDataJackState::Planted
		|| !IsValid(PlayerController)
		|| !IsValid(PlayerState)
		|| !CanPawnDefuse(PlayerPawn))
	{
		return false;
	}

	InteractionController = PlayerController;
	InteractionStartPawnLocation = PlayerPawn->GetActorLocation();
	SetInteractingPlayerState(PlayerState);
	InteractionStartServerTime = GetSynchronizedServerTime();
	InteractionEndServerTime = InteractionStartServerTime + DefuseDuration;
	SetDataJackState(EDataJackState::Defusing);
	BroadcastDataChanged();

	GetWorldTimerManager().SetTimer(
		InteractionCompletionTimerHandle,
		this,
		&ADataJack::CompleteActiveInteraction,
		DefuseDuration,
		false);
	GetWorldTimerManager().SetTimer(
		InteractionValidationTimerHandle,
		this,
		&ADataJack::ValidateActiveInteraction,
		InteractionValidationInterval,
		true);
	return true;
}

void ADataJack::CancelInteraction(APlayerController* PlayerController)
{
	if (!HasAuthority() || (DataJackState != EDataJackState::Planting && DataJackState != EDataJackState::Defusing))
	{
		return;
	}

	APlayerController* ActiveInteractionController = InteractionController.Get();
	if (IsValid(PlayerController) && ActiveInteractionController != PlayerController)
	{
		return;
	}

	const EDataJackState RestoredState = DataJackState == EDataJackState::Planting
		? EDataJackState::Carried
		: EDataJackState::Planted;
	ResetInteractionState();
	SetDataJackState(RestoredState);
	BroadcastDataChanged();
	NotifyInteractionStopped(ActiveInteractionController);
}

bool ADataJack::BeginDropHold(APlayerController* PlayerController, ABaseTeamPlayerState* PlayerState)
{
	if (!HasAuthority()
		|| !bRoundActive
		|| DataJackState != EDataJackState::Carried
		|| !IsValid(PlayerController)
		|| !IsValid(PlayerState)
		|| !IsCarriedBy(PlayerState)
		|| CarrierPawn.Get() != PlayerController->GetPawn())
	{
		return false;
	}

	if (DropController.Get() == PlayerController
		&& DropPlayerState.Get() == PlayerState
		&& GetWorldTimerManager().IsTimerActive(DropHoldTimerHandle))
	{
		return true;
	}

	FinishDropHold();
	DropController = PlayerController;
	DropPlayerState = PlayerState;
	GetWorldTimerManager().SetTimer(
		DropHoldTimerHandle,
		this,
		&ADataJack::CompleteDropHold,
		DropHoldDuration,
		false);
	return true;
}

void ADataJack::CancelDropHold(APlayerController* PlayerController)
{
	if (!HasAuthority())
	{
		return;
	}

	if (IsValid(PlayerController) && DropController.Get() != PlayerController)
	{
		return;
	}

	FinishDropHold();
}

void ADataJack::HandlePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState)
{
	if (!HasAuthority() || !IsValid(EliminatedPlayerState))
	{
		return;
	}

	if (IsInteractingPlayer(EliminatedPlayerState))
	{
		CancelInteraction();
	}

	if (IsCarriedBy(EliminatedPlayerState))
	{
		DropFromCarrier(false);
	}
}

bool ADataJack::CanPawnPlant(const APawn* PlayerPawn, const ADataJackSiteVolume* SiteVolume) const
{
	return DataJackWorldQueries::CanPawnPlant(PlayerPawn, SiteVolume);
}

bool ADataJack::CanPawnDefuse(const APawn* PlayerPawn) const
{
	DataJackWorldQueries::FDefuseSettings Settings;
	Settings.Radius = DefuseRadius;
	Settings.bRequireLineOfSight = bRequireDefuseLineOfSight;
	Settings.TraceChannel = DefuseTraceChannel;
	return DataJackWorldQueries::CanPawnDefuse(
		GetWorld(),
		*this,
		GetInteractionWorldLocation(),
		PlayerPawn,
		Settings);
}

void ADataJack::ValidateActiveInteraction()
{
	if (!HasAuthority())
	{
		return;
	}

	APlayerController* PlayerController = InteractionController.Get();
	ABaseTeamPlayerState* PlayerState = InteractingPlayerState;
	APawn* PlayerPawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
	const bool bHasValidPlayer = IsValid(PlayerController)
		&& IsValid(PlayerState)
		&& PlayerState->IsAlive()
		&& IsValid(PlayerPawn);
	const bool bStayedAtInteractionStart = bHasValidPlayer
		&& FVector::DistSquared(PlayerPawn->GetActorLocation(), InteractionStartPawnLocation)
		<= FMath::Square(InteractionMovementTolerance);

	bool bInteractionValid = bHasValidPlayer && bStayedAtInteractionStart;
	if (bInteractionValid && DataJackState == EDataJackState::Planting)
	{
		bInteractionValid = IsCarriedBy(PlayerState)
			&& CarrierPawn.Get() == PlayerPawn
			&& CanPawnPlant(PlayerPawn, ActivePlantSite.Get());
	}
	else if (bInteractionValid && DataJackState == EDataJackState::Defusing)
	{
		bInteractionValid = CanPawnDefuse(PlayerPawn);
	}
	else if (DataJackState != EDataJackState::Planting && DataJackState != EDataJackState::Defusing)
	{
		bInteractionValid = false;
	}

	if (!bInteractionValid)
	{
		CancelInteraction();
	}
}

void ADataJack::CompleteActiveInteraction()
{
	if (!HasAuthority())
	{
		return;
	}

	ValidateActiveInteraction();
	switch (DataJackState)
	{
	case EDataJackState::Planting:
		CompletePlanting();
		break;
	case EDataJackState::Defusing:
		CompleteDefusing();
		break;
	default:
		break;
	}
}

void ADataJack::CompletePlanting()
{
	APlayerController* CompletedInteractionController = InteractionController.Get();
	APawn* PlayerPawn = IsValid(CompletedInteractionController) ? CompletedInteractionController->GetPawn() : nullptr;
	ADataJackSiteVolume* SiteVolume = ActivePlantSite.Get();
	UBombObjectiveManager* BombObjectiveManager = GetObjectiveManager();
	if (!IsValid(SiteVolume)
		|| !IsValid(PlayerPawn)
		|| !IsValid(BombObjectiveManager)
		|| !BombObjectiveManager->CanCompletePlanting(this))
	{
		CancelInteraction();
		return;
	}

	FHitResult PlantSurfaceHit;
	if (!SiteVolume->FindPlantSurface(PlayerPawn, PlantSurfaceHit))
	{
		CancelInteraction();
		return;
	}

	const FTransform PlantTransform = BuildGroundedTransform(PlantSurfaceHit, PlayerPawn);
	ResetInteractionState();
	SetOwner(nullptr);
	CarrierPawn.Reset();
	SetActorTransform(PlantTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetCarrierPlayerState(nullptr);
	PlantedSiteId = SiteVolume->GetSiteId();
	DetonationStartServerTime = GetSynchronizedServerTime();
	DetonationEndServerTime = DetonationStartServerTime + CountdownDuration;
	SetDataJackState(EDataJackState::Planted);
	BroadcastDataChanged();
	GetWorldTimerManager().SetTimer(
		DetonationTimerHandle,
		this,
		&ADataJack::Detonate,
		CountdownDuration,
		false);
	NotifyInteractionStopped(CompletedInteractionController);
	BombObjectiveManager->NotifyDataJackPlanted(this);
}

void ADataJack::CompleteDefusing()
{
	if (DetonationEndServerTime > 0.0f && GetSynchronizedServerTime() >= DetonationEndServerTime)
	{
		Detonate();
		return;
	}

	APlayerController* CompletedInteractionController = InteractionController.Get();
	ResetInteractionState();
	ClearDetonationTimer();
	DetonationStartServerTime = 0.0f;
	DetonationEndServerTime = 0.0f;
	SetDataJackState(EDataJackState::Defused);
	BroadcastDataChanged();
	NotifyInteractionStopped(CompletedInteractionController);

	if (UBombObjectiveManager* BombObjectiveManager = GetObjectiveManager())
	{
		BombObjectiveManager->NotifyDataJackDefused(this);
	}
}

void ADataJack::CompleteDropHold()
{
	if (!HasAuthority())
	{
		return;
	}

	APlayerController* PlayerController = DropController.Get();
	ABaseTeamPlayerState* PlayerState = DropPlayerState.Get();
	if (DataJackState == EDataJackState::Carried
		&& IsValid(PlayerController)
		&& IsValid(PlayerState)
		&& PlayerState->IsAlive()
		&& IsCarriedBy(PlayerState)
		&& CarrierPawn.Get() == PlayerController->GetPawn())
	{
		DropFromCarrier(true);
		return;
	}

	FinishDropHold();
}

void ADataJack::Detonate()
{
	if (!HasAuthority() || (DataJackState != EDataJackState::Planted && DataJackState != EDataJackState::Defusing))
	{
		return;
	}

	APlayerController* ActiveInteractionController = ResetInteractionState();
	ClearDetonationTimer();
	SetDataJackState(EDataJackState::Detonated);
	BroadcastDataChanged();
	NotifyInteractionStopped(ActiveInteractionController);

	if (UBombObjectiveManager* BombObjectiveManager = GetObjectiveManager())
	{
		BombObjectiveManager->NotifyDataJackDetonated(this);
	}
}

void ADataJack::ClearInteractionTimers()
{
	GetWorldTimerManager().ClearTimer(InteractionCompletionTimerHandle);
	GetWorldTimerManager().ClearTimer(InteractionValidationTimerHandle);
}

APlayerController* ADataJack::ResetInteractionState()
{
	APlayerController* PreviousController = InteractionController.Get();
	ClearInteractionTimers();
	InteractionController.Reset();
	InteractionStartPawnLocation = FVector::ZeroVector;
	ActivePlantSite.Reset();
	SetInteractingPlayerState(nullptr);
	InteractionStartServerTime = 0.0f;
	InteractionEndServerTime = 0.0f;
	return PreviousController;
}

APlayerController* ADataJack::ResetDropHold()
{
	APlayerController* PreviousController = DropController.Get();
	GetWorldTimerManager().ClearTimer(DropHoldTimerHandle);
	DropController.Reset();
	DropPlayerState.Reset();
	return PreviousController;
}

void ADataJack::FinishDropHold()
{
	NotifyDropStopped(ResetDropHold());
}

void ADataJack::ClearDetonationTimer()
{
	GetWorldTimerManager().ClearTimer(DetonationTimerHandle);
}

void ADataJack::ClearPickupBlockIfNotOverlapping()
{
	APawn* BlockedPickupPawn = TemporarilyBlockedPickupPawn.Get();
	if (!IsValid(BlockedPickupPawn)
		|| !IsValid(PickupArea)
		|| !PickupArea->IsOverlappingActor(BlockedPickupPawn))
	{
		TemporarilyBlockedPickupPawn.Reset();
	}

	EvaluatePickupOverlaps();
}

void ADataJack::NotifyInteractionStopped(APlayerController* PlayerController)
{
	if (UBombObjectiveManager* BombObjectiveManager = GetObjectiveManager())
	{
		BombObjectiveManager->NotifyInteractionStopped(PlayerController);
	}
}

void ADataJack::NotifyDropStopped(APlayerController* PlayerController)
{
	if (UBombObjectiveManager* BombObjectiveManager = GetObjectiveManager())
	{
		BombObjectiveManager->NotifyDropStopped(PlayerController);
	}
}

void ADataJack::DropFromCarrier(const bool bBlockPreviousCarrier)
{
	if (!HasAuthority())
	{
		return;
	}

	if (DataJackState == EDataJackState::Planting)
	{
		CancelInteraction();
	}

	if (DataJackState != EDataJackState::Carried)
	{
		FinishDropHold();
		return;
	}

	APawn* PreviousCarrierPawn = CarrierPawn.Get();
	APlayerController* CompletedDropController = ResetDropHold();
	const FTransform DropTransform = BuildDropTransform(PreviousCarrierPawn);

	SetOwner(nullptr);
	CarrierPawn.Reset();
	SetActorTransform(DropTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetCarrierPlayerState(nullptr);
	bHasBeenDropped = true;

	if (bBlockPreviousCarrier && IsValid(PreviousCarrierPawn))
	{
		TemporarilyBlockedPickupPawn = PreviousCarrierPawn;
	}

	SetDataJackState(EDataJackState::Idle);
	BroadcastDataChanged();
	NotifyDropStopped(CompletedDropController);

	GetWorldTimerManager().SetTimerForNextTick(this, &ADataJack::ClearPickupBlockIfNotOverlapping);
}

FTransform ADataJack::BuildDropTransform(const APawn* SourceCarrierPawn) const
{
	DataJackWorldQueries::FDropSettings DropSettings;
	DropSettings.ForwardDistance = DropForwardDistance;
	DropSettings.ObstacleClearance = DropObstacleClearance;
	DropSettings.FloorTraceHeight = DropFloorTraceHeight;
	DropSettings.FloorTraceDepth = DropFloorTraceDepth;
	DropSettings.TraceChannel = DropPlacementTraceChannel;

	DataJackWorldQueries::FGroundingSettings GroundingSettings;
	GroundingSettings.SurfaceClearance = SurfaceClearance;

	return DataJackWorldQueries::BuildDropTransform(
		GetWorld(),
		*this,
		DataJackMesh,
		SourceCarrierPawn,
		DropSettings,
		GroundingSettings);
}

FTransform ADataJack::BuildGroundedTransform(const FHitResult& SurfaceHit, const APawn* OrientationSource) const
{
	DataJackWorldQueries::FGroundingSettings Settings;
	Settings.SurfaceClearance = SurfaceClearance;
	return DataJackWorldQueries::BuildGroundedTransform(
		*this,
		DataJackMesh,
		SurfaceHit,
		OrientationSource,
		Settings);
}
