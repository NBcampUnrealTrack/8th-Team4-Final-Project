#include "GameMode/Manager/DominationRespawnManager.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Actor/DominationCapturePoint.h"
#include "Actor/DominationSpawnVolume.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/State/DominationGameState.h"
#include "GameMode/Manager/DominationObjectiveManager.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationRespawn, Log, All);

bool UDominationRespawnManager::Initialize(
	ADominationGameMode* InOwnerGameMode,
	UDominationObjectiveManager* InObjectiveManager,
	const float InRespawnDelay,
	const float InRespawnProtectionDuration)
{
	if (!InitializeManager(InOwnerGameMode))
	{
		return false;
	}

	if (!IsValid(InObjectiveManager))
	{
		Shutdown();
		return false;
	}

	ObjectiveManager = InObjectiveManager;
	RespawnDelay = InRespawnDelay;
	RespawnProtectionDuration = InRespawnProtectionDuration;
	if (!CacheSpawnVolumes())
	{
		Shutdown();
		return false;
	}

	ADominationGameState* GameState = GetDominationGameState();
	if (!IsValid(GameState))
	{
		Shutdown();
		return false;
	}

	// 최초 참가자는 활성 거점 공개 전에 중앙 C를 기준으로 스폰한다.
	if (!UpdateTeamSpawnVolumes(EDominationPointId::C))
	{
		Shutdown();
		return false;
	}

	BoundGameState = GameState;
	GameState->OnActivePointChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleActivePointChanged);
	return true;
}

void UDominationRespawnManager::Shutdown()
{
	CancelPendingRespawns();
	ClearAllRespawnProtections();
	if (ADominationGameState* GameState = BoundGameState.Get())
	{
		GameState->OnActivePointChanged.RemoveDynamic(
			this,
			&ThisClass::HandleActivePointChanged);
	}

	TeamASpawnVolume.Reset();
	TeamBSpawnVolume.Reset();
	SpawnObjectivePointId = EDominationPointId::None;
	NextSpawnCandidateIndices.Reset();
	SpawnVolumes.Reset();
	BoundGameState.Reset();
	ObjectiveManager.Reset();
	Super::Shutdown();
}

void UDominationRespawnManager::CancelPendingRespawns()
{
	if (FTimerManager* TimerManager = GetTimerManager())
	{
		for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& PendingRespawn : PendingRespawnTimers)
		{
			TimerManager->ClearTimer(PendingRespawn.Value);
		}
	}

	PendingRespawnTimers.Reset();
}

void UDominationRespawnManager::ClearAllRespawnProtections()
{
	for (TPair<TWeakObjectPtr<AShooterPlayerState>, FActiveRespawnProtection>& Entry
		: ActiveRespawnProtections)
	{
		ReleaseRespawnProtection(Entry.Value);
	}

	ActiveRespawnProtections.Reset();
}

void UDominationRespawnManager::ScheduleRespawn(AController* WaitingController)
{
	const TWeakObjectPtr<AController> ControllerKey(WaitingController);
	if (IsValid(WaitingController))
	{
		EndRespawnProtection(
			WaitingController->GetPlayerState<AShooterPlayerState>());
	}

	FTimerManager* TimerManager = GetTimerManager();
	if (TimerManager == nullptr
		|| !IsValid(WaitingController)
		|| PendingRespawnTimers.Contains(ControllerKey))
	{
		return;
	}

	FTimerHandle& RespawnTimer = PendingRespawnTimers.Add(ControllerKey);
	TimerManager->SetTimer(
		RespawnTimer,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleRespawnTimer,
			ControllerKey),
		RespawnDelay,
		false);
}

void UDominationRespawnManager::BeginRespawnProtection(
	AShooterPlayerState* PlayerState)
{
	FTimerManager* TimerManager = GetTimerManager();
	if (TimerManager == nullptr
		|| !IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| RespawnProtectionDuration <= 0.0f)
	{
		return;
	}

	EndRespawnProtection(PlayerState);

	UAbilitySystemComponent* AbilitySystem =
		PlayerState->GetAbilitySystemComponent();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	FActiveRespawnProtection& Protection =
		ActiveRespawnProtections.Add(PlayerState);
	Protection.AbilitySystem = AbilitySystem;
	Protection.WeaponFiredEventTags.AddTag(RogueTags::Event_Weapon_Fired);
	Protection.WeaponFiredDelegate =
		AbilitySystem->AddGameplayEventTagContainerDelegate(
			Protection.WeaponFiredEventTags,
			FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(
				this,
				&ThisClass::HandleProtectedPlayerWeaponFired,
				TWeakObjectPtr<AShooterPlayerState>(PlayerState)));

	AbilitySystem->AddLooseGameplayTag(
		RogueTags::State_Combat_Invulnerable);
	TimerManager->SetTimer(
		Protection.ExpirationTimer,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleRespawnProtectionExpired,
			TWeakObjectPtr<AShooterPlayerState>(PlayerState)),
		RespawnProtectionDuration,
		false);
}

bool UDominationRespawnManager::TryGetSpawnTransform(
	const EGameTeam Team,
	const EDominationPointId ActivePointId,
	FTransform& OutSpawnTransform)
{
	ADominationSpawnVolume* SpawnVolume = GetTeamSpawnVolume(Team);
	UDominationObjectiveManager* Manager = ObjectiveManager.Get();
	ADominationCapturePoint* ActiveCapturePoint = IsValid(Manager)
		? Manager->GetCapturePoint(ActivePointId)
		: nullptr;
	ADominationGameMode* GameMode = GetOwnerGameMode();
	if (!IsValid(SpawnVolume)
		|| !IsValid(ActiveCapturePoint)
		|| !IsValid(GameMode))
	{
		return false;
	}

	TArray<FTransform> CandidateTransforms;
	SpawnVolume->GetSpawnCandidateTransforms(CandidateTransforms);
	const TWeakObjectPtr<ADominationSpawnVolume> SpawnVolumeKey(SpawnVolume);
	int32& NextCandidateIndex = NextSpawnCandidateIndices.FindOrAdd(SpawnVolumeKey);
	int32 SelectedCandidateIndex = INDEX_NONE;
	if (!GameMode->TrySelectAvailableSpawnTransform(
		CandidateTransforms,
		NextCandidateIndex,
		OutSpawnTransform,
		SelectedCandidateIndex))
	{
		return false;
	}

	NextCandidateIndex = (SelectedCandidateIndex + 1) % CandidateTransforms.Num();
	FVector FacingDirection = ActiveCapturePoint->GetActorLocation() - OutSpawnTransform.GetLocation();
	FacingDirection.Z = 0.0;
	OutSpawnTransform.SetRotation(FacingDirection.Rotation().Quaternion());
	return true;
}

bool UDominationRespawnManager::UpdateTeamSpawnVolumes(
	const EDominationPointId ActivePointId)
{
	if (SpawnObjectivePointId == ActivePointId
		&& TeamASpawnVolume.IsValid()
		&& TeamBSpawnVolume.IsValid())
	{
		return true;
	}

	const int32 ActivePointIndex = DominationRules::GetPointIndex(ActivePointId);
	const int32 TeamAVolumeIndex = DominationRules::GetSpawnBoundaryIndex(
		EGameTeam::TeamA,
		ActivePointIndex);
	const int32 TeamBVolumeIndex = DominationRules::GetSpawnBoundaryIndex(
		EGameTeam::TeamB,
		ActivePointIndex);
	if (!SpawnVolumes.IsValidIndex(TeamAVolumeIndex)
		|| !SpawnVolumes.IsValidIndex(TeamBVolumeIndex))
	{
		return false;
	}

	TeamASpawnVolume = SpawnVolumes[TeamAVolumeIndex];
	TeamBSpawnVolume = SpawnVolumes[TeamBVolumeIndex];
	SpawnObjectivePointId = ActivePointId;

	UE_LOG(
		LogDominationRespawn,
		Log,
		TEXT("Updated spawn volumes for active point %d: TeamA boundary %d, TeamB boundary %d."),
		static_cast<uint8>(ActivePointId),
		TeamAVolumeIndex,
		TeamBVolumeIndex);
	return true;
}

ADominationSpawnVolume* UDominationRespawnManager::GetTeamSpawnVolume(
	const EGameTeam Team) const
{
	switch (Team)
	{
	case EGameTeam::TeamA:
		return TeamASpawnVolume.Get();
	case EGameTeam::TeamB:
		return TeamBSpawnVolume.Get();
	default:
		return nullptr;
	}
}

void UDominationRespawnManager::HandleActivePointChanged(
	const EDominationPointId ActivePointId)
{
	if (ActivePointId == EDominationPointId::None)
	{
		TeamASpawnVolume.Reset();
		TeamBSpawnVolume.Reset();
		SpawnObjectivePointId = EDominationPointId::None;
		return;
	}

	if (!UpdateTeamSpawnVolumes(ActivePointId))
	{
		UE_LOG(
			LogDominationRespawn,
			Error,
			TEXT("Could not update team spawn volumes for active point %d."),
			static_cast<uint8>(ActivePointId));
	}
}

bool UDominationRespawnManager::CacheSpawnVolumes()
{
	UWorld* World = GetWorld();
	UDominationObjectiveManager* Manager = ObjectiveManager.Get();
	if (!IsValid(World) || !IsValid(Manager))
	{
		return false;
	}

	SpawnVolumes.Reset();
	NextSpawnCandidateIndices.Reset();
	for (TActorIterator<ADominationSpawnVolume> Iterator(World); Iterator; ++Iterator)
	{
		SpawnVolumes.Add(*Iterator);
	}

	if (SpawnVolumes.Num() != DominationRules::SpawnBoundaryCount)
	{
		UE_LOG(LogDominationRespawn, Error,
			TEXT("Domination requires exactly %d SpawnVolumes, but found %d."),
			DominationRules::SpawnBoundaryCount,
			SpawnVolumes.Num());
		return false;
	}

	const ADominationCapturePoint* PointA = Manager->GetCapturePoint(EDominationPointId::A);
	const ADominationCapturePoint* PointE = Manager->GetCapturePoint(EDominationPointId::E);
	const FVector PointALocation = PointA->GetActorLocation();
	const FVector SpawnAxis = (PointE->GetActorLocation() - PointALocation).GetSafeNormal();
	if (SpawnAxis.IsNearlyZero())
	{
		UE_LOG(LogDominationRespawn, Error,
			TEXT("Capture points A and E must not share the same location."));
		return false;
	}

	SpawnVolumes.Sort(
		[PointALocation, SpawnAxis](
			const TWeakObjectPtr<ADominationSpawnVolume>& Left,
			const TWeakObjectPtr<ADominationSpawnVolume>& Right)
		{
			const double LeftDistance = FVector::DotProduct(
				Left->GetActorLocation() - PointALocation,
				SpawnAxis);
			const double RightDistance = FVector::DotProduct(
				Right->GetActorLocation() - PointALocation,
				SpawnAxis);
			return LeftDistance < RightDistance;
		});

	UE_LOG(LogDominationRespawn, Log,
		TEXT("Registered six Domination SpawnVolumes from the A-side boundary to the E-side boundary."));
	return true;
}

void UDominationRespawnManager::HandleRespawnTimer(
	const TWeakObjectPtr<AController> WaitingController)
{
	PendingRespawnTimers.Remove(WaitingController);

	ADominationGameMode* GameMode = GetOwnerGameMode();
	if (IsValid(GameMode) && WaitingController.IsValid())
	{
		GameMode->RespawnPlayer(WaitingController.Get());
	}
}

void UDominationRespawnManager::HandleRespawnProtectionExpired(
	const TWeakObjectPtr<AShooterPlayerState> PlayerState)
{
	EndRespawnProtection(PlayerState.Get());
}

void UDominationRespawnManager::HandleProtectedPlayerWeaponFired(
	const FGameplayTag,
	const FGameplayEventData*,
	const TWeakObjectPtr<AShooterPlayerState> PlayerState)
{
	EndRespawnProtection(PlayerState.Get());
}

void UDominationRespawnManager::EndRespawnProtection(
	AShooterPlayerState* PlayerState)
{
	FActiveRespawnProtection Protection;
	if (!ActiveRespawnProtections.RemoveAndCopyValue(
		TWeakObjectPtr<AShooterPlayerState>(PlayerState),
		Protection))
	{
		return;
	}

	ReleaseRespawnProtection(Protection);
}

void UDominationRespawnManager::ReleaseRespawnProtection(
	FActiveRespawnProtection& Protection)
{
	if (FTimerManager* TimerManager = GetTimerManager())
	{
		TimerManager->ClearTimer(Protection.ExpirationTimer);
	}

	if (UAbilitySystemComponent* AbilitySystem = Protection.AbilitySystem.Get())
	{
		AbilitySystem->RemoveGameplayEventTagContainerDelegate(
			Protection.WeaponFiredEventTags,
			Protection.WeaponFiredDelegate);
		AbilitySystem->RemoveLooseGameplayTag(
			RogueTags::State_Combat_Invulnerable);
	}
}
