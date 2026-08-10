#include "GameFlow/Bomb/DataJack.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFlow/Bomb/BombGameMode.h"
#include "GameFlow/Bomb/DataJackSiteVolume.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFlow/Round/BaseRoundGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADataJack::ADataJack()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(20.0f);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DataJackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DataJackMesh"));
	DataJackMesh->SetupAttachment(SceneRoot);
	DataJackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DataJackMesh->SetGenerateOverlapEvents(false);
	DataJackMesh->SetCanEverAffectNavigation(false);

	InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionPoint"));
	InteractionPoint->SetupAttachment(SceneRoot);
	InteractionPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));

	PickupArea = CreateDefaultSubobject<USphereComponent>(TEXT("PickupArea"));
	PickupArea->SetupAttachment(SceneRoot);
	PickupArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupArea->SetCollisionObjectType(ECC_WorldDynamic);
	PickupArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupArea->SetGenerateOverlapEvents(true);
	PickupArea->SetCanEverAffectNavigation(false);
	PickupArea->OnComponentBeginOverlap.AddDynamic(this, &ADataJack::HandlePickupAreaBeginOverlap);
	PickupArea->OnComponentEndOverlap.AddDynamic(this, &ADataJack::HandlePickupAreaEndOverlap);
}

void ADataJack::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (IsValid(PickupArea))
	{
		PickupArea->SetSphereRadius(PickupRadius);
	}
}

void ADataJack::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	APlayerController* ActiveInteractionController = InteractionController.Get();
	ClearInteractionTimers();
	ClearDropTimer();
	ClearDetonationTimer();
	InteractionController.Reset();
	InteractionStartPawnLocation = FVector::ZeroVector;
	ActivePlantSite.Reset();

	if (HasAuthority())
	{
		NotifyInteractionStopped(ActiveInteractionController);
	}

	Super::EndPlay(EndPlayReason);
}

void ADataJack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADataJack, DataJackState);
	DOREPLIFETIME(ADataJack, CarrierPlayerState);
	DOREPLIFETIME(ADataJack, InteractingPlayerState);
	DOREPLIFETIME(ADataJack, PlantedSiteId);
	DOREPLIFETIME(ADataJack, bHasBeenDropped);
	DOREPLIFETIME(ADataJack, InteractionStartServerTime);
	DOREPLIFETIME(ADataJack, InteractionEndServerTime);
	DOREPLIFETIME(ADataJack, DetonationStartServerTime);
	DOREPLIFETIME(ADataJack, DetonationEndServerTime);
}

EDataJackState ADataJack::GetDataJackState() const
{
	return DataJackState;
}

ABaseTeamPlayerState* ADataJack::GetCarrierPlayerState() const
{
	return CarrierPlayerState;
}

ABaseTeamPlayerState* ADataJack::GetInteractingPlayerState() const
{
	return InteractingPlayerState;
}

FName ADataJack::GetPlantedSiteId() const
{
	return PlantedSiteId;
}

bool ADataJack::HasBeenDropped() const
{
	return bHasBeenDropped;
}

float ADataJack::GetInteractionTimeRemaining() const
{
	return FMath::Max(0.0f, InteractionEndServerTime - GetSynchronizedServerTime());
}

float ADataJack::GetInteractionProgress() const
{
	const float InteractionDuration = InteractionEndServerTime - InteractionStartServerTime;
	if (InteractionDuration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp((GetSynchronizedServerTime() - InteractionStartServerTime) / InteractionDuration, 0.0f, 1.0f);
}

float ADataJack::GetDetonationTimeRemaining() const
{
	return FMath::Max(0.0f, DetonationEndServerTime - GetSynchronizedServerTime());
}

float ADataJack::GetDetonationProgress() const
{
	const float DetonationDuration = DetonationEndServerTime - DetonationStartServerTime;
	if (DetonationDuration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp((GetSynchronizedServerTime() - DetonationStartServerTime) / DetonationDuration, 0.0f, 1.0f);
}

FVector ADataJack::GetTrackedWorldLocation() const
{
	if (DataJackState == EDataJackState::Carried || DataJackState == EDataJackState::Planting)
	{
		const APawn* ReplicatedCarrierPawn = IsValid(CarrierPlayerState) ? CarrierPlayerState->GetPawn() : nullptr;
		if (IsValid(ReplicatedCarrierPawn))
		{
			return ReplicatedCarrierPawn->GetActorLocation();
		}

		if (CarrierPawn.IsValid())
		{
			return CarrierPawn->GetActorLocation();
		}
	}

	return GetActorLocation();
}

FVector ADataJack::GetInteractionWorldLocation() const
{
	return IsValid(InteractionPoint) ? InteractionPoint->GetComponentLocation() : GetActorLocation();
}

bool ADataJack::IsCarriedBy(const ABaseTeamPlayerState* PlayerState) const
{
	return IsValid(PlayerState) && CarrierPlayerState == PlayerState;
}

bool ADataJack::IsInteractingPlayer(const ABaseTeamPlayerState* PlayerState) const
{
	return IsValid(PlayerState) && InteractingPlayerState == PlayerState;
}

bool ADataJack::IsRoundActive() const
{
	return bRoundActive;
}

void ADataJack::ConfigureForMode(
	const float NewPlantDuration,
	const float NewCountdownDuration,
	const float NewDefuseDuration,
	const float NewDropHoldDuration,
	const float NewValidationInterval)
{
	if (!HasAuthority())
	{
		return;
	}

	PlantDuration = FMath::Max(0.01f, NewPlantDuration);
	CountdownDuration = FMath::Max(0.01f, NewCountdownDuration);
	DefuseDuration = FMath::Max(0.01f, NewDefuseDuration);
	DropHoldDuration = FMath::Max(0.01f, NewDropHoldDuration);
	InteractionValidationInterval = FMath::Max(0.02f, NewValidationInterval);
}

void ADataJack::InitializeForRound(const FTransform& SpawnTransform)
{
	if (!HasAuthority())
	{
		return;
	}

	ClearInteractionTimers();
	ClearDropTimer();
	ClearDetonationTimer();

	bRoundActive = false;
	InteractionStartPawnLocation = FVector::ZeroVector;
	CarrierPawn.Reset();
	InteractionController.Reset();
	DropController.Reset();
	DropPlayerState.Reset();
	TemporarilyBlockedPickupPawn.Reset();
	ActivePlantSite.Reset();
	SetOwner(nullptr);
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);

	DataJackState = EDataJackState::Idle;
	CarrierPlayerState = nullptr;
	InteractingPlayerState = nullptr;
	PlantedSiteId = NAME_None;
	bHasBeenDropped = false;
	InteractionStartServerTime = 0.0f;
	InteractionEndServerTime = 0.0f;
	DetonationStartServerTime = 0.0f;
	DetonationEndServerTime = 0.0f;
	UpdateMeshVisibility();
	UpdatePickupCollision();
	OnDataJackChanged.Broadcast();
	ForceNetUpdate();
}

void ADataJack::ActivateForRound()
{
	if (!HasAuthority())
	{
		return;
	}

	bRoundActive = true;
	UpdatePickupCollision();
	GetWorldTimerManager().SetTimerForNextTick(this, &ADataJack::EvaluatePickupOverlaps);
}

void ADataJack::DeactivateForRound()
{
	if (!HasAuthority())
	{
		return;
	}

	bRoundActive = false;
	ClearDropTimer();
	ClearDetonationTimer();

	APlayerController* PreviousInteractionController = InteractionController.Get();
	EDataJackState RestoredState = DataJackState;
	if (DataJackState == EDataJackState::Planting)
	{
		RestoredState = EDataJackState::Carried;
	}
	else if (DataJackState == EDataJackState::Defusing)
	{
		RestoredState = EDataJackState::Planted;
	}

	ClearInteractionTimers();
	InteractionController.Reset();
	InteractionStartPawnLocation = FVector::ZeroVector;
	ActivePlantSite.Reset();
	SetInteractingPlayerState(nullptr);
	InteractionStartServerTime = 0.0f;
	InteractionEndServerTime = 0.0f;
	SetDataJackState(RestoredState);
	NotifyInteractionStopped(PreviousInteractionController);
	UpdatePickupCollision();
	OnRep_DataJackData();
	ForceNetUpdate();
}

bool ADataJack::TryPickup(APawn* NewCarrierPawn, ABaseTeamPlayerState* NewCarrierPlayerState)
{
	if (!HasAuthority()
		|| !bRoundActive
		|| DataJackState != EDataJackState::Idle
		|| !IsValid(NewCarrierPawn)
		|| !IsValid(NewCarrierPlayerState)
		|| TemporarilyBlockedPickupPawn.Get() == NewCarrierPawn)
	{
		return false;
	}

	ClearDropTimer();
	CarrierPawn = NewCarrierPawn;
	SetOwner(NewCarrierPawn);
	TemporarilyBlockedPickupPawn.Reset();
	SetCarrierPlayerState(NewCarrierPlayerState);
	SetDataJackState(EDataJackState::Carried);
	return true;
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

	ClearDropTimer();
	InteractionController = PlayerController;
	InteractionStartPawnLocation = PlayerPawn->GetActorLocation();
	ActivePlantSite = SiteVolume;
	SetInteractingPlayerState(PlayerState);
	InteractionStartServerTime = GetSynchronizedServerTime();
	InteractionEndServerTime = InteractionStartServerTime + PlantDuration;
	SetDataJackState(EDataJackState::Planting);
	OnRep_DataJackData();

	GetWorldTimerManager().SetTimer(InteractionCompletionTimerHandle, this, &ADataJack::CompleteActiveInteraction, PlantDuration, false);
	GetWorldTimerManager().SetTimer(InteractionValidationTimerHandle, this, &ADataJack::ValidateActiveInteraction, InteractionValidationInterval, true);
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
	OnRep_DataJackData();

	GetWorldTimerManager().SetTimer(InteractionCompletionTimerHandle, this, &ADataJack::CompleteActiveInteraction, DefuseDuration, false);
	GetWorldTimerManager().SetTimer(InteractionValidationTimerHandle, this, &ADataJack::ValidateActiveInteraction, InteractionValidationInterval, true);
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

	const EDataJackState RestoredState = DataJackState == EDataJackState::Planting ? EDataJackState::Carried : EDataJackState::Planted;
	ClearInteractionTimers();
	InteractionController.Reset();
	InteractionStartPawnLocation = FVector::ZeroVector;
	ActivePlantSite.Reset();
	SetInteractingPlayerState(nullptr);
	InteractionStartServerTime = 0.0f;
	InteractionEndServerTime = 0.0f;
	SetDataJackState(RestoredState);
	OnRep_DataJackData();
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

	ClearDropTimer();
	DropController = PlayerController;
	DropPlayerState = PlayerState;
	GetWorldTimerManager().SetTimer(DropHoldTimerHandle, this, &ADataJack::CompleteDropHold, DropHoldDuration, false);
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

	ClearDropTimer();
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

void ADataJack::OnRep_DataJackState(const EDataJackState PreviousState)
{
	UpdateMeshVisibility();
	UpdatePickupCollision();
	HandleStateTransition(PreviousState);
}

void ADataJack::OnRep_DataJackData()
{
	OnDataJackChanged.Broadcast();
}

void ADataJack::HandlePickupAreaBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	if (!HasAuthority() || !bRoundActive || DataJackState != EDataJackState::Idle)
	{
		return;
	}

	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr;
	if (IsValid(OverlappingPawn) && IsValid(BombGameMode))
	{
		BombGameMode->TryPickupDataJack(OverlappingPawn);
	}
}

void ADataJack::HandlePickupAreaEndOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32)
{
	if (HasAuthority() && TemporarilyBlockedPickupPawn.Get() == OtherActor)
	{
		TemporarilyBlockedPickupPawn.Reset();
	}
}

void ADataJack::SetDataJackState(const EDataJackState NewState)
{
	if (!HasAuthority() || DataJackState == NewState)
	{
		return;
	}

	const EDataJackState PreviousState = DataJackState;
	DataJackState = NewState;
	OnRep_DataJackState(PreviousState);
	ForceNetUpdate();
}

void ADataJack::SetCarrierPlayerState(ABaseTeamPlayerState* NewCarrierPlayerState)
{
	if (!HasAuthority() || CarrierPlayerState == NewCarrierPlayerState)
	{
		return;
	}

	CarrierPlayerState = NewCarrierPlayerState;
	OnRep_DataJackData();
	ForceNetUpdate();
}

void ADataJack::SetInteractingPlayerState(ABaseTeamPlayerState* NewInteractingPlayerState)
{
	if (!HasAuthority() || InteractingPlayerState == NewInteractingPlayerState)
	{
		return;
	}

	InteractingPlayerState = NewInteractingPlayerState;
	OnRep_DataJackData();
	ForceNetUpdate();
}

void ADataJack::HandleStateTransition(const EDataJackState PreviousState)
{
	switch (DataJackState)
	{
	case EDataJackState::Idle:
		if (PreviousState == EDataJackState::Carried || PreviousState == EDataJackState::Planting)
		{
			BP_OnDropped();
		}
		break;
	case EDataJackState::Carried:
		if (PreviousState == EDataJackState::Idle)
		{
			BP_OnPickedUp();
		}
		break;
	case EDataJackState::Planting:
		BP_OnPlantingStarted();
		break;
	case EDataJackState::Planted:
		if (PreviousState == EDataJackState::Planting)
		{
			BP_OnPlanted();
		}
		break;
	case EDataJackState::Defusing:
		BP_OnDefuseStarted();
		break;
	case EDataJackState::Detonated:
		BP_OnDetonated();
		break;
	case EDataJackState::Defused:
		BP_OnDefused();
		break;
	default:
		break;
	}

	OnDataJackChanged.Broadcast();
}

void ADataJack::UpdateMeshVisibility()
{
	if (!IsValid(DataJackMesh))
	{
		return;
	}

	const bool bMeshVisible = DataJackState != EDataJackState::Carried
		&& DataJackState != EDataJackState::Planting;
	DataJackMesh->SetVisibility(bMeshVisible, true);
	DataJackMesh->SetHiddenInGame(!bMeshVisible, true);
}

void ADataJack::UpdatePickupCollision()
{
	if (!IsValid(PickupArea))
	{
		return;
	}

	const bool bEnablePickup = HasAuthority() && bRoundActive && DataJackState == EDataJackState::Idle;
	PickupArea->SetCollisionEnabled(bEnablePickup ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ADataJack::EvaluatePickupOverlaps()
{
	if (!HasAuthority() || !bRoundActive || DataJackState != EDataJackState::Idle || !IsValid(PickupArea))
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	PickupArea->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		APawn* OverlappingPawn = Cast<APawn>(OverlappingActor);
		ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr;
		if (IsValid(OverlappingPawn) && IsValid(BombGameMode) && BombGameMode->TryPickupDataJack(OverlappingPawn))
		{
			return;
		}
	}
}

bool ADataJack::CanPawnPlant(const APawn* PlayerPawn, const ADataJackSiteVolume* SiteVolume) const
{
	if (!IsValid(PlayerPawn) || !IsValid(SiteVolume) || !SiteVolume->ContainsPawn(PlayerPawn))
	{
		return false;
	}

	FHitResult PlantSurfaceHit;
	return SiteVolume->FindPlantSurface(PlayerPawn, PlantSurfaceHit);
}

bool ADataJack::CanPawnDefuse(const APawn* PlayerPawn) const
{
	UWorld* World = GetWorld();
	if (!IsValid(PlayerPawn) || !IsValid(World))
	{
		return false;
	}

	const FVector InteractionLocation = GetInteractionWorldLocation();
	if (FVector::DistSquared(PlayerPawn->GetActorLocation(), InteractionLocation) > FMath::Square(DefuseRadius))
	{
		return false;
	}

	if (!bRequireDefuseLineOfSight)
	{
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DataJackDefuseVisibility), false, PlayerPawn);
	QueryParams.AddIgnoredActor(this);

	FHitResult VisibilityHit;
	return !World->LineTraceSingleByChannel(
		VisibilityHit,
		PlayerPawn->GetPawnViewLocation(),
		InteractionLocation,
		DefuseTraceChannel,
		QueryParams);
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
	const bool bHasValidPlayer = IsValid(PlayerController) && IsValid(PlayerState) && PlayerState->IsAlive() && IsValid(PlayerPawn);
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
	if (DataJackState != EDataJackState::Planting && DataJackState != EDataJackState::Defusing)
	{
		return;
	}

	APlayerController* CompletedInteractionController = InteractionController.Get();
	APawn* PlayerPawn = IsValid(CompletedInteractionController) ? CompletedInteractionController->GetPawn() : nullptr;

	if (DataJackState == EDataJackState::Planting)
	{
		ADataJackSiteVolume* SiteVolume = ActivePlantSite.Get();
		const ABaseRoundGameState* RoundGameState = GetWorld() != nullptr ? GetWorld()->GetGameState<ABaseRoundGameState>() : nullptr;
		if (!IsValid(SiteVolume)
			|| !IsValid(PlayerPawn)
			|| !IsValid(RoundGameState)
			|| RoundGameState->GetGameFlowPhase() != EGameFlowPhase::RoundInProgress
			|| RoundGameState->GetRoundTimeRemaining() <= 0.0f)
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
		ClearInteractionTimers();
		InteractionController.Reset();
		InteractionStartPawnLocation = FVector::ZeroVector;
		ActivePlantSite.Reset();
		SetInteractingPlayerState(nullptr);
		InteractionStartServerTime = 0.0f;
		InteractionEndServerTime = 0.0f;
		SetOwner(nullptr);
		CarrierPawn.Reset();
		SetActorTransform(PlantTransform, false, nullptr, ETeleportType::TeleportPhysics);
		SetCarrierPlayerState(nullptr);
		PlantedSiteId = SiteVolume->GetSiteId();
		DetonationStartServerTime = GetSynchronizedServerTime();
		DetonationEndServerTime = DetonationStartServerTime + CountdownDuration;
		SetDataJackState(EDataJackState::Planted);
		OnRep_DataJackData();
		GetWorldTimerManager().SetTimer(DetonationTimerHandle, this, &ADataJack::Detonate, CountdownDuration, false);
		NotifyInteractionStopped(CompletedInteractionController);

		if (ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr)
		{
			BombGameMode->NotifyDataJackPlanted(this);
		}
		return;
	}

	if (DetonationEndServerTime > 0.0f && GetSynchronizedServerTime() >= DetonationEndServerTime)
	{
		Detonate();
		return;
	}

	ClearInteractionTimers();
	ClearDetonationTimer();
	InteractionController.Reset();
	InteractionStartPawnLocation = FVector::ZeroVector;
	SetInteractingPlayerState(nullptr);
	InteractionStartServerTime = 0.0f;
	InteractionEndServerTime = 0.0f;
	DetonationStartServerTime = 0.0f;
	DetonationEndServerTime = 0.0f;
	SetDataJackState(EDataJackState::Defused);
	OnRep_DataJackData();
	NotifyInteractionStopped(CompletedInteractionController);

	if (ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr)
	{
		BombGameMode->NotifyDataJackDefused(this);
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
	}

	ClearDropTimer();
}

void ADataJack::Detonate()
{
	if (!HasAuthority() || (DataJackState != EDataJackState::Planted && DataJackState != EDataJackState::Defusing))
	{
		return;
	}

	APlayerController* ActiveInteractionController = InteractionController.Get();
	ClearInteractionTimers();
	ClearDetonationTimer();
	InteractionController.Reset();
	InteractionStartPawnLocation = FVector::ZeroVector;
	SetInteractingPlayerState(nullptr);
	InteractionStartServerTime = 0.0f;
	InteractionEndServerTime = 0.0f;
	SetDataJackState(EDataJackState::Detonated);
	OnRep_DataJackData();
	NotifyInteractionStopped(ActiveInteractionController);

	if (ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr)
	{
		BombGameMode->NotifyDataJackDetonated(this);
	}
}

void ADataJack::ClearInteractionTimers()
{
	GetWorldTimerManager().ClearTimer(InteractionCompletionTimerHandle);
	GetWorldTimerManager().ClearTimer(InteractionValidationTimerHandle);
}

void ADataJack::ClearDropTimer()
{
	GetWorldTimerManager().ClearTimer(DropHoldTimerHandle);
	DropController.Reset();
	DropPlayerState.Reset();
}

void ADataJack::ClearDetonationTimer()
{
	GetWorldTimerManager().ClearTimer(DetonationTimerHandle);
}

void ADataJack::ClearPickupBlockIfNotOverlapping()
{
	APawn* BlockedPickupPawn = TemporarilyBlockedPickupPawn.Get();
	if (!IsValid(BlockedPickupPawn) || !IsValid(PickupArea) || !PickupArea->IsOverlappingActor(BlockedPickupPawn))
	{
		TemporarilyBlockedPickupPawn.Reset();
	}

	EvaluatePickupOverlaps();
}

void ADataJack::NotifyInteractionStopped(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr)
	{
		BombGameMode->NotifyDataJackInteractionStopped(PlayerController);
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
		return;
	}

	APawn* PreviousCarrierPawn = CarrierPawn.Get();
	const FTransform DropTransform = BuildDropTransform(PreviousCarrierPawn);

	ClearDropTimer();
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
	OnRep_DataJackData();

	GetWorldTimerManager().SetTimerForNextTick(this, &ADataJack::ClearPickupBlockIfNotOverlapping);
}

FTransform ADataJack::BuildDropTransform(const APawn* PreviousCarrierPawn) const
{
	FVector DropLocation = IsValid(PreviousCarrierPawn) ? PreviousCarrierPawn->GetActorLocation() : GetActorLocation();
	const FRotator DropRotation = IsValid(PreviousCarrierPawn) ? PreviousCarrierPawn->GetActorRotation() : GetActorRotation();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DataJackDrop), false, this);
	if (IsValid(PreviousCarrierPawn))
	{
		QueryParams.AddIgnoredActor(PreviousCarrierPawn);
		const FVector DesiredDropLocation = DropLocation + PreviousCarrierPawn->GetActorForwardVector() * DropForwardDistance;
		FHitResult ObstacleHit;
		if (GetWorld() != nullptr
			&& GetWorld()->LineTraceSingleByChannel(ObstacleHit, DropLocation, DesiredDropLocation, DropPlacementTraceChannel, QueryParams))
		{
			DropLocation = ObstacleHit.ImpactPoint + ObstacleHit.ImpactNormal * DropObstacleClearance;
		}
		else
		{
			DropLocation = DesiredDropLocation;
		}
	}

	const FVector TraceStart = DropLocation + FVector::UpVector * DropFloorTraceHeight;
	const FVector TraceEnd = DropLocation - FVector::UpVector * DropFloorTraceDepth;
	FHitResult FloorHit;
	if (GetWorld() != nullptr && GetWorld()->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, DropPlacementTraceChannel, QueryParams))
	{
		return BuildGroundedTransform(FloorHit, PreviousCarrierPawn);
	}

	return FTransform(FRotator(0.0f, DropRotation.Yaw, 0.0f), DropLocation, GetActorScale3D());
}

FTransform ADataJack::BuildGroundedTransform(const FHitResult& SurfaceHit, const APawn* OrientationSource) const
{
	FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		SurfaceNormal = FVector::UpVector;
	}

	FVector Forward = IsValid(OrientationSource) ? OrientationSource->GetActorForwardVector() : GetActorForwardVector();
	Forward = FVector::VectorPlaneProject(Forward, SurfaceNormal).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::VectorPlaneProject(FVector::ForwardVector, SurfaceNormal).GetSafeNormal();
	}
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::CrossProduct(FVector::RightVector, SurfaceNormal).GetSafeNormal();
	}

	FTransform GroundedTransform(
		FRotationMatrix::MakeFromXZ(Forward, SurfaceNormal).Rotator(),
		SurfaceHit.ImpactPoint,
		GetActorScale3D());

	const UStaticMesh* StaticMesh = IsValid(DataJackMesh) ? DataJackMesh->GetStaticMesh() : nullptr;
	if (!IsValid(StaticMesh))
	{
		GroundedTransform.AddToTranslation(SurfaceNormal * SurfaceClearance);
		return GroundedTransform;
	}

	const FBoxSphereBounds LocalBounds = StaticMesh->GetBounds();
	const FVector BoundsMin = LocalBounds.Origin - LocalBounds.BoxExtent;
	const FVector BoundsMax = LocalBounds.Origin + LocalBounds.BoxExtent;
	const FVector Corners[8] =
	{
		FVector(BoundsMin.X, BoundsMin.Y, BoundsMin.Z),
		FVector(BoundsMin.X, BoundsMin.Y, BoundsMax.Z),
		FVector(BoundsMin.X, BoundsMax.Y, BoundsMin.Z),
		FVector(BoundsMin.X, BoundsMax.Y, BoundsMax.Z),
		FVector(BoundsMax.X, BoundsMin.Y, BoundsMin.Z),
		FVector(BoundsMax.X, BoundsMin.Y, BoundsMax.Z),
		FVector(BoundsMax.X, BoundsMax.Y, BoundsMin.Z),
		FVector(BoundsMax.X, BoundsMax.Y, BoundsMax.Z)
	};

	const FTransform MeshRelativeTransform = DataJackMesh->GetRelativeTransform();
	float MinimumSurfaceDistance = BIG_NUMBER;
	for (const FVector& Corner : Corners)
	{
		const FVector ActorSpaceCorner = MeshRelativeTransform.TransformPosition(Corner);
		const FVector WorldCorner = GroundedTransform.TransformPosition(ActorSpaceCorner);
		MinimumSurfaceDistance = FMath::Min(
			MinimumSurfaceDistance,
			FVector::DotProduct(WorldCorner - SurfaceHit.ImpactPoint, SurfaceNormal));
	}

	GroundedTransform.AddToTranslation(SurfaceNormal * (SurfaceClearance - MinimumSurfaceDistance));
	return GroundedTransform;
}

float ADataJack::GetSynchronizedServerTime() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return 0.0f;
	}

	const AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	return IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}
