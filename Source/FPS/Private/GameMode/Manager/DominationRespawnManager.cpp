#include "GameMode/Manager/DominationRespawnManager.h"

#include "GameMode/Manager/PlayerRespawnProtection.h"
#include "Actor/DominationCapturePoint.h"
#include "Actor/DominationSpawnVolume.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/State/DominationGameState.h"
#include "GameMode/Manager/DominationObjectiveManager.h"
#include "Player/ShooterPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationRespawn, Log, All);

namespace
{
	constexpr float RespawnRetryDelaySeconds = 0.5f;
}

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
	RespawnDelay = FMath::Max(0.1f, InRespawnDelay);
	if (IsValid(RespawnProtection)) { RespawnProtection->Shutdown(); }
	RespawnProtection = NewObject<UPlayerRespawnProtection>(this);
	RespawnProtection->Initialize(GetWorld(), InRespawnProtectionDuration);
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
	if (!UpdateTeamSpawnVolumes(DominationRules::InitialPointId))
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
	if (IsValid(RespawnProtection)) { RespawnProtection->Shutdown(); }
	RespawnProtection = nullptr;
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
    if (IsValid(RespawnProtection))
    {
        RespawnProtection->ClearAll();
    }
}

void UDominationRespawnManager::ScheduleRespawn(AController* WaitingController)
{
	ScheduleRespawnInternal(WaitingController, RespawnDelay);
}

void UDominationRespawnManager::ScheduleRespawnRetry(AController* WaitingController)
{
	// 스폰 지점이 잠깐 막혀도 관전 상태에 고정되지 않도록 재시도한다.
	ScheduleRespawnInternal(WaitingController, RespawnRetryDelaySeconds);
}

void UDominationRespawnManager::ScheduleRespawnInternal(
	AController* WaitingController,
	const float DelaySeconds)
{
	const TWeakObjectPtr<AController> ControllerKey(WaitingController);
	if (IsValid(WaitingController))
	{
		EndRespawnProtection(
			WaitingController->GetPlayerState<AShooterPlayerState>());
	}

	FTimerManager* TimerManager = GetTimerManager();
	if (TimerManager == nullptr || !IsValid(WaitingController))
	{
		return;
	}

	if (FTimerHandle* ExistingTimer = PendingRespawnTimers.Find(ControllerKey))
	{
		if (TimerManager->TimerExists(*ExistingTimer))
		{
			return;
		}

		// 타이머는 끝났는데 키만 남아 있으면 이후 사망/재시도 예약이 영구 차단된다.
		PendingRespawnTimers.Remove(ControllerKey);
	}

	FTimerHandle& RespawnTimer = PendingRespawnTimers.Add(ControllerKey);
	TimerManager->SetTimer(
		RespawnTimer,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleRespawnTimer,
			ControllerKey),
		FMath::Max(0.1f, DelaySeconds),
		false);
}

void UDominationRespawnManager::BeginRespawnProtection(AShooterPlayerState* PlayerState)
{
    if (IsValid(RespawnProtection))
    {
        RespawnProtection->BeginProtection(PlayerState);
    }
}

bool UDominationRespawnManager::TryGetSpawnTransform(
	const EGameTeam Team,
	const EDominationPointId ActivePointId,
	FTransform& OutSpawnTransform)
{
	// 리스폰 타이머와 거점 전환 이벤트의 실행 순서에 의존하지 않는다.
	// 실제 스폰을 시도하는 순간의 활성 거점 기준으로 캐시를 동기화한다.
	if (ActivePointId == EDominationPointId::None
		|| !UpdateTeamSpawnVolumes(ActivePointId))
	{
		return false;
	}

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





void UDominationRespawnManager::EndRespawnProtection(AShooterPlayerState* PlayerState)
{
    if (IsValid(RespawnProtection))
    {
        RespawnProtection->EndProtection(PlayerState);
    }
}


