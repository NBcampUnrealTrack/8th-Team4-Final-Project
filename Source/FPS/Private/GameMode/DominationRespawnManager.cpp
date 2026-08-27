#include "GameMode/DominationRespawnManager.h"

#include "Actor/DominationCapturePoint.h"
#include "Actor/DominationSpawnVolume.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameMode/DominationGameMode.h"
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
	return CacheSpawnVolumes();
}

void UDominationRespawnManager::Shutdown()
{
	CancelPendingRespawns();
	SpawnVolumes.Reset();
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
	const int32 ActivePointIndex = DominationRules::GetPointIndex(ActivePointId);
	const int32 SpawnVolumeIndex = DominationRules::GetSpawnBoundaryIndex(
		Team,
		ActivePointIndex);
	if (!SpawnVolumes.IsValidIndex(SpawnVolumeIndex))
	{
		return false;
	}

	ADominationSpawnVolume* SpawnVolume = SpawnVolumes[SpawnVolumeIndex].Get();
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
