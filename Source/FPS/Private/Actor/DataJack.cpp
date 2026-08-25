#include "Actor/DataJack.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameMode/BombObjectiveManager.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"
#include "TimerManager.h"

namespace
{
	// DataJack 보유 상태 태그 관리 함수
	void SetDataJackCarryingTag(ABaseTeamPlayerState* PlayerState, const bool bCarrying)
	{
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		URogueAbilitySystemComponent* AbilitySystem = IsValid(ShooterPlayerState)
			? ShooterPlayerState->GetRogueAbilitySystemComponent()
			: nullptr;
		if (IsValid(AbilitySystem))
		{
			AbilitySystem->SetLooseGameplayTagCount(
				RogueTags::State_Combat_DataJack_Carrying,
				bCarrying ? 1 : 0,
				EGameplayTagReplicationState::TagOnly);
		}
	}
}

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
	if (HasAuthority())
	{
		APlayerController* ActiveInteractionController = ResetInteractionState();
		FinishDropHold();
		NotifyInteractionStopped(ActiveInteractionController);
		SetDataJackCarryingTag(CarrierPlayerState, false);
	}
	else
	{
		ClearInteractionTimers();
		GetWorldTimerManager().ClearTimer(DropHoldTimerHandle);
	}

	ClearDetonationTimer();
	ObjectiveManager.Reset();
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

void ADataJack::InitializeForRound(const FTransform& SpawnTransform, UBombObjectiveManager* InObjectiveManager)
{
	if (!HasAuthority())
	{
		return;
	}

	APlayerController* PreviousInteractionController = ResetInteractionState();
	FinishDropHold();
	NotifyInteractionStopped(PreviousInteractionController);
	ClearDetonationTimer();
	ObjectiveManager = InObjectiveManager;

	bRoundActive = false;
	CarrierPawn.Reset();
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
	FinishDropHold();
	ClearDetonationTimer();

	EDataJackState RestoredState = DataJackState;
	if (DataJackState == EDataJackState::Planting)
	{
		RestoredState = EDataJackState::Carried;
	}
	else if (DataJackState == EDataJackState::Defusing)
	{
		RestoredState = EDataJackState::Planted;
	}

	APlayerController* PreviousInteractionController = ResetInteractionState();
	SetDataJackState(RestoredState);
	NotifyInteractionStopped(PreviousInteractionController);
	UpdatePickupCollision();
	BroadcastDataChanged();
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

	FinishDropHold();
	CarrierPawn = NewCarrierPawn;
	SetOwner(NewCarrierPawn);
	TemporarilyBlockedPickupPawn.Reset();
	SetCarrierPlayerState(NewCarrierPlayerState);
	SetDataJackState(EDataJackState::Carried);
	return true;
}

void ADataJack::OnRep_DataJackState(const EDataJackState PreviousState)
{
	UpdateMeshVisibility();
	UpdatePickupCollision();
	HandleStateTransition(PreviousState);
}

void ADataJack::OnRep_DataJackData()
{
	BroadcastDataChanged();
}

void ADataJack::BroadcastDataChanged()
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
	if (UBombObjectiveManager* BombObjectiveManager = GetObjectiveManager())
	{
		BombObjectiveManager->TryPickup(OverlappingPawn);
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
	UpdateMeshVisibility();
	UpdatePickupCollision();
	HandleStateTransition(PreviousState);
	ForceNetUpdate();
}

void ADataJack::SetCarrierPlayerState(ABaseTeamPlayerState* NewCarrierPlayerState)
{
	if (!HasAuthority() || CarrierPlayerState == NewCarrierPlayerState)
	{
		return;
	}

	SetDataJackCarryingTag(CarrierPlayerState, false);
	CarrierPlayerState = NewCarrierPlayerState;
	SetDataJackCarryingTag(CarrierPlayerState, true);
	BroadcastDataChanged();
	ForceNetUpdate();
}

void ADataJack::SetInteractingPlayerState(ABaseTeamPlayerState* NewInteractingPlayerState)
{
	if (!HasAuthority() || InteractingPlayerState == NewInteractingPlayerState)
	{
		return;
	}

	InteractingPlayerState = NewInteractingPlayerState;
	BroadcastDataChanged();
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

	UBombObjectiveManager* BombObjectiveManager = GetObjectiveManager();
	if (!IsValid(BombObjectiveManager))
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	PickupArea->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		APawn* OverlappingPawn = Cast<APawn>(OverlappingActor);
		if (IsValid(OverlappingPawn) && BombObjectiveManager->TryPickup(OverlappingPawn))
		{
			return;
		}
	}
}

UBombObjectiveManager* ADataJack::GetObjectiveManager() const
{
	return ObjectiveManager.Get();
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
