#include "GameMode/DominationRespawnManager.h"

#include "Actor/DominationCapturePoint.h"
#include "Actor/DominationSpawnVolume.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/DominationGameState.h"
#include "GameMode/DominationObjectiveManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationRespawn, Log, All);

bool UDominationRespawnManager::Initialize(
	ADominationGameMode* InOwnerGameMode,
	UDominationObjectiveManager* InObjectiveManager,
	const float InRespawnDelay)
{
	OwnerGameMode = InOwnerGameMode;
	ObjectiveManager = InObjectiveManager;
	RespawnDelay = InRespawnDelay;
	if (!CacheSpawnVolumes())
	{
		return false;
	}

	ADominationGameState* GameState = InOwnerGameMode->GetGameState<ADominationGameState>();
	if (!IsValid(GameState))
	{
		return false;
	}

	// 최초 참가자는 활성 거점 공개 전에 중앙 C를 기준으로 스폰한다.
	if (!UpdateTeamSpawnVolumes(EDominationPointId::C))
	{
		return false;
	}

	DominationGameState = GameState;
	GameState->OnActivePointChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleActivePointChanged);
	return true;
}

void UDominationRespawnManager::Shutdown()
{
	CancelPendingRespawns();
	if (ADominationGameState* GameState = DominationGameState.Get())
	{
		GameState->OnActivePointChanged.RemoveDynamic(
			this,
			&ThisClass::HandleActivePointChanged);
	}

	TeamASpawnVolume.Reset();
	TeamBSpawnVolume.Reset();
	SpawnObjectivePointId = EDominationPointId::None;
	SpawnVolumes.Reset();
	DominationGameState.Reset();
	ObjectiveManager.Reset();
	OwnerGameMode.Reset();
}

void UDominationRespawnManager::CancelPendingRespawns()
{
	if (ADominationGameMode* GameMode = OwnerGameMode.Get())
	{
		for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& PendingRespawn : PendingRespawnTimers)
		{
			GameMode->GetWorldTimerManager().ClearTimer(PendingRespawn.Value);
		}
	}

	PendingRespawnTimers.Reset();
}

void UDominationRespawnManager::ScheduleRespawn(AController* EliminatedController)
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	const TWeakObjectPtr<AController> ControllerKey(EliminatedController);
	if (!IsValid(GameMode)
		|| !IsValid(EliminatedController)
		|| PendingRespawnTimers.Contains(ControllerKey))
	{
		return;
	}

	FTimerHandle& RespawnTimer = PendingRespawnTimers.Add(ControllerKey);
	GameMode->GetWorldTimerManager().SetTimer(
		RespawnTimer,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleRespawnTimer,
			ControllerKey),
		RespawnDelay,
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
	if (!IsValid(SpawnVolume) || !IsValid(ActiveCapturePoint))
	{
		return false;
	}

	OutSpawnTransform = SpawnVolume->GetNextSpawnTransform();
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
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	UDominationObjectiveManager* Manager = ObjectiveManager.Get();
	if (!IsValid(GameMode) || !IsValid(Manager))
	{
		return false;
	}

	SpawnVolumes.Reset();
	for (TActorIterator<ADominationSpawnVolume> Iterator(GameMode->GetWorld()); Iterator; ++Iterator)
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
	const TWeakObjectPtr<AController> EliminatedController)
{
	PendingRespawnTimers.Remove(EliminatedController);

	ADominationGameMode* GameMode = OwnerGameMode.Get();
	if (IsValid(GameMode) && EliminatedController.IsValid())
	{
		GameMode->RespawnEliminatedPlayer(EliminatedController.Get());
	}
}
