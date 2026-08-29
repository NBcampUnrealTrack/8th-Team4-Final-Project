#include "GameMode/DominationObjectiveManager.h"

#include "Actor/DominationCapturePoint.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/DominationGameState.h"
#include "GameFramework/Pawn.h"
#include "Player/BaseTeamPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationObjective, Log, All);

namespace
{
	constexpr int32 RequiredCapturePointCount = DominationRules::PointCount;
}

bool UDominationObjectiveManager::Initialize(
	ADominationGameMode* InOwnerGameMode,
	const FDominationCaptureRules& InCaptureRules,
	const float InPointTransitionDuration)
{
	OwnerGameMode = InOwnerGameMode;
	CaptureRules = InCaptureRules;
	PointTransitionDuration = InPointTransitionDuration;
	if (!IsValid(InOwnerGameMode) || !InOwnerGameMode->HasAuthority())
	{
		return false;
	}

	if (!CacheCapturePoints())
	{
		CapturePoints.Reset();
		OwnerGameMode.Reset();
		return false;
	}

	for (const TWeakObjectPtr<ADominationCapturePoint>& CapturePointPtr : CapturePoints)
	{
		if (ADominationCapturePoint* CapturePoint = CapturePointPtr.Get())
		{
			CapturePoint->SetObjectiveActive(false);
		}
	}

	ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(DominationGameState))
	{
		UE_LOG(LogDominationObjective, Error,
			TEXT("DominationGameState is unavailable; capture points cannot be published."));
		CapturePoints.Reset();
		OwnerGameMode.Reset();
		return false;
	}

	DominationGameState->SetObjectiveState(
		EDominationPointId::None,
		EDominationObjectivePhase::Inactive);
	UE_LOG(LogDominationObjective, Log,
		TEXT("Registered five Domination capture points. Objectives are waiting for match start."));

	return true;
}

void UDominationObjectiveManager::Shutdown()
{
	StopObjectiveFlow();

	for (const TWeakObjectPtr<ADominationCapturePoint>& CapturePointPtr : CapturePoints)
	{
		if (ADominationCapturePoint* CapturePoint = CapturePointPtr.Get())
		{
			CapturePoint->SetObjectiveActive(false);
		}
	}

	if (ADominationGameState* DominationGameState = GetDominationGameState())
	{
		DominationGameState->SetObjectiveState(
			EDominationPointId::None,
			EDominationObjectivePhase::Inactive);
	}

	CapturePoints.Reset();
	OwnerGameMode.Reset();
	OnCaptureCompleted.Clear();
}

void UDominationObjectiveManager::StopObjectiveFlow()
{
	if (ADominationGameMode* GameMode = OwnerGameMode.Get())
	{
		GameMode->GetWorldTimerManager().ClearTimer(PointTransitionTimerHandle);
	}

	StopCaptureTracking();
	TransitionTargetPointId = EDominationPointId::None;
	CapturingTeam = EGameTeam::None;
	CaptureProgress = 0.0f;
	bCaptureCompleted = false;

	if (ADominationGameState* DominationGameState = GetDominationGameState())
	{
		DominationGameState->SetCaptureState(FDominationCaptureState());
		DominationGameState->SetObjectiveState(
			DominationGameState->GetActivePointId(),
			EDominationObjectivePhase::Inactive);
	}
}

bool UDominationObjectiveManager::ActivateInitialPoint()
{
	ADominationCapturePoint* CenterPoint = GetCapturePoint(EDominationPointId::C);
	ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(CenterPoint) || !IsValid(DominationGameState))
	{
		return false;
	}

	CapturingTeam = EGameTeam::None;
	CaptureProgress = 0.0f;
	bCaptureCompleted = false;
	DominationGameState->SetObjectiveState(
		EDominationPointId::C,
		EDominationObjectivePhase::CaptureEnabled);
	StartCaptureTracking(CenterPoint);
	UE_LOG(LogDominationObjective, Log, TEXT("Domination capture point C is now active."));
	return true;
}

void UDominationObjectiveManager::StartCaptureTracking(
	ADominationCapturePoint* CapturePoint)
{
	StopCaptureTracking();
	if (!IsValid(CapturePoint))
	{
		return;
	}

	ActiveCapturePoint = CapturePoint;
	CapturePoint->OnPawnEntered.AddUObject(
		this,
		&ThisClass::HandleCapturePointPawnChanged);
	CapturePoint->OnPawnExited.AddUObject(
		this,
		&ThisClass::HandleCapturePointPawnChanged);
	CapturePoint->SetObjectiveActive(true);

	ADominationGameMode* GameMode = OwnerGameMode.Get();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return;
	}

	LastCaptureUpdateTime = World->GetTimeSeconds();
	UpdateCaptureState(0.0f);
	GameMode->GetWorldTimerManager().SetTimer(
		CaptureUpdateTimerHandle,
		this,
		&ThisClass::HandleCaptureUpdateTimer,
		CaptureUpdateInterval,
		true);
	GameMode->GetWorldTimerManager().SetTimer(
		SuddenDeathTimerHandle,
		this,
		&ThisClass::HandleSuddenDeathTimerExpired,
		CaptureRules.SuddenDeathDelay,
		false);
}

void UDominationObjectiveManager::StopCaptureTracking()
{
	if (ADominationGameMode* GameMode = OwnerGameMode.Get())
	{
		GameMode->GetWorldTimerManager().ClearTimer(CaptureUpdateTimerHandle);
		GameMode->GetWorldTimerManager().ClearTimer(SuddenDeathTimerHandle);
	}

	if (ADominationCapturePoint* CapturePoint = ActiveCapturePoint.Get())
	{
		CapturePoint->OnPawnEntered.RemoveAll(this);
		CapturePoint->OnPawnExited.RemoveAll(this);
		CapturePoint->SetObjectiveActive(false);
	}

	ActiveCapturePoint.Reset();
	bSuddenDeathActive = false;
	LastCaptureUpdateTime = 0.0;
}

void UDominationObjectiveManager::HandleCapturePointPawnChanged(APawn*)
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return;
	}

	LastCaptureUpdateTime = World->GetTimeSeconds();
	UpdateCaptureState(0.0f);
}

void UDominationObjectiveManager::HandleCaptureUpdateTimer()
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	const float DeltaSeconds = static_cast<float>(CurrentTime - LastCaptureUpdateTime);
	LastCaptureUpdateTime = CurrentTime;
	UpdateCaptureState(DeltaSeconds);
}

void UDominationObjectiveManager::HandleSuddenDeathTimerExpired()
{
	bSuddenDeathActive = true;
	UE_LOG(
		LogDominationObjective,
		Log,
		TEXT("Domination capture point %d entered sudden death. Capture time is now %.1f seconds."),
		static_cast<uint8>(ActiveCapturePoint->GetPointId()),
		CaptureRules.SuddenDeathCaptureTime);
}

void UDominationObjectiveManager::UpdateCaptureState(const float DeltaSeconds)
{
	int32 TeamAPlayerCount = 0;
	int32 TeamBPlayerCount = 0;
	CountActivePlayers(TeamAPlayerCount, TeamBPlayerCount);

	const bool bTeamAPresent = TeamAPlayerCount > 0;
	const bool bTeamBPresent = TeamBPlayerCount > 0;
	const bool bContested = bTeamAPresent && bTeamBPresent;
	if (bCaptureCompleted)
	{
		PublishCaptureState(TeamAPlayerCount, TeamBPlayerCount, false);
		return;
	}

	if (!bTeamAPresent && !bTeamBPresent)
	{
		CapturingTeam = EGameTeam::None;
		CaptureProgress = 0.0f;
		PublishCaptureState(TeamAPlayerCount, TeamBPlayerCount, false);
		return;
	}

	if (bContested)
	{
		PublishCaptureState(TeamAPlayerCount, TeamBPlayerCount, true);
		return;
	}

	const EGameTeam PresentTeam = bTeamAPresent
		? EGameTeam::TeamA
		: EGameTeam::TeamB;
	const int32 PresentPlayerCount = bTeamAPresent
		? TeamAPlayerCount
		: TeamBPlayerCount;

	if (CapturingTeam != PresentTeam)
	{
		CapturingTeam = PresentTeam;
		CaptureProgress = 0.0f;
	}

	const bool bWasComplete = CaptureProgress >= 1.0f;
	if (!bWasComplete && DeltaSeconds > 0.0f)
	{
		const float CaptureTime = CaptureRules.GetCaptureTime(
			PresentPlayerCount,
			bSuddenDeathActive);
		CaptureProgress = FMath::Min(1.0f, CaptureProgress + DeltaSeconds / CaptureTime);
	}

	PublishCaptureState(TeamAPlayerCount, TeamBPlayerCount, false);
	if (!bWasComplete && CaptureProgress >= 1.0f)
	{
		bCaptureCompleted = true;
		const ADominationCapturePoint* CapturePoint = ActiveCapturePoint.Get();
		const EDominationPointId CapturedPointId = IsValid(CapturePoint)
			? CapturePoint->GetPointId()
			: EDominationPointId::None;
		const EGameTeam CaptureTeam = CapturingTeam;
		OnCaptureCompleted.Broadcast(
			CapturedPointId,
			CaptureTeam);

		// 매치 승리가 확정된 점령이라면 다음 거점 전환을 예약하지 않는다.
		const ADominationGameState* DominationGameState = GetDominationGameState();
		if (IsValid(DominationGameState)
			&& DominationGameState->GetDominationPhase() == EDominationMatchPhase::InProgress)
		{
			BeginPointTransition(CapturedPointId, CaptureTeam);
		}
	}
}

void UDominationObjectiveManager::BeginPointTransition(
	const EDominationPointId CapturedPointId,
	const EGameTeam CaptureTeam)
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(GameMode) || !IsValid(DominationGameState))
	{
		return;
	}

	StopCaptureTracking();
	CapturingTeam = EGameTeam::None;
	CaptureProgress = 0.0f;
	bCaptureCompleted = false;
	DominationGameState->SetCaptureState(FDominationCaptureState());

	const int32 CapturedPointIndex = DominationRules::GetPointIndex(CapturedPointId);
	const int32 NextPointIndex = CapturedPointIndex + DominationRules::GetAttackDirection(CaptureTeam);
	TransitionTargetPointId = DominationRules::GetPointId(NextPointIndex);
	if (TransitionTargetPointId == EDominationPointId::None)
	{
		DominationGameState->SetObjectiveState(
			CapturedPointId,
			EDominationObjectivePhase::Inactive);
		UE_LOG(LogDominationObjective, Warning,
			TEXT("No next objective exists after team %d captured point %d. Objective flow stopped."),
			static_cast<uint8>(CaptureTeam),
			static_cast<uint8>(CapturedPointId));
		return;
	}

	if (PointTransitionDuration <= 0.0f)
	{
		HandlePointTransitionComplete();
		return;
	}

	DominationGameState->SetObjectiveState(
		TransitionTargetPointId,
		EDominationObjectivePhase::Repositioning,
		PointTransitionDuration);
	GameMode->GetWorldTimerManager().SetTimer(
		PointTransitionTimerHandle,
		this,
		&ThisClass::HandlePointTransitionComplete,
		PointTransitionDuration,
		false);
	UE_LOG(LogDominationObjective, Log,
		TEXT("Domination frontline moved to point %d. Capture unlocks in %.1f seconds."),
		static_cast<uint8>(TransitionTargetPointId),
		PointTransitionDuration);
}

void UDominationObjectiveManager::HandlePointTransitionComplete()
{
	ADominationGameState* DominationGameState = GetDominationGameState();
	ADominationCapturePoint* NextCapturePoint = GetCapturePoint(TransitionTargetPointId);
	if (!IsValid(DominationGameState) || !IsValid(NextCapturePoint))
	{
		return;
	}

	const EDominationPointId ActivatedPointId = TransitionTargetPointId;
	TransitionTargetPointId = EDominationPointId::None;
	CapturingTeam = EGameTeam::None;
	CaptureProgress = 0.0f;
	bCaptureCompleted = false;
	DominationGameState->SetCaptureState(FDominationCaptureState());
	DominationGameState->SetObjectiveState(
		ActivatedPointId,
		EDominationObjectivePhase::CaptureEnabled);
	StartCaptureTracking(NextCapturePoint);
	UE_LOG(LogDominationObjective, Log,
		TEXT("Domination capture point %d is now active."),
		static_cast<uint8>(ActivatedPointId));
}

void UDominationObjectiveManager::CountActivePlayers(
	int32& OutTeamAPlayerCount,
	int32& OutTeamBPlayerCount) const
{
	OutTeamAPlayerCount = 0;
	OutTeamBPlayerCount = 0;

	const ADominationCapturePoint* CapturePoint = ActiveCapturePoint.Get();
	if (!IsValid(CapturePoint))
	{
		return;
	}

	TArray<APawn*> OverlappingPawns;
	CapturePoint->GetOverlappingPawns(OverlappingPawns);
	TSet<const ABaseTeamPlayerState*> CountedPlayerStates;
	for (const APawn* Pawn : OverlappingPawns)
	{
		const ABaseTeamPlayerState* PlayerState = IsValid(Pawn)
			? Pawn->GetPlayerState<ABaseTeamPlayerState>()
			: nullptr;
		if (!IsValid(PlayerState)
			|| !PlayerState->IsAlive()
			|| CountedPlayerStates.Contains(PlayerState))
		{
			continue;
		}

		switch (PlayerState->GetTeam())
		{
		case EGameTeam::TeamA:
			++OutTeamAPlayerCount;
			CountedPlayerStates.Add(PlayerState);
			break;
		case EGameTeam::TeamB:
			++OutTeamBPlayerCount;
			CountedPlayerStates.Add(PlayerState);
			break;
		default:
			break;
		}
	}
}

void UDominationObjectiveManager::PublishCaptureState(
	const int32 TeamAPlayerCount,
	const int32 TeamBPlayerCount,
	const bool bContested) const
{
	ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(DominationGameState))
	{
		return;
	}

	FDominationCaptureState NewCaptureState;
	NewCaptureState.CapturingTeam = CapturingTeam;
	NewCaptureState.CaptureProgress = CaptureProgress;
	NewCaptureState.TeamAPlayerCount = TeamAPlayerCount;
	NewCaptureState.TeamBPlayerCount = TeamBPlayerCount;
	NewCaptureState.bContested = bContested;
	DominationGameState->SetCaptureState(NewCaptureState);
}

bool UDominationObjectiveManager::CacheCapturePoints()
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return false;
	}

	CapturePoints.Reset();
	CapturePoints.SetNum(RequiredCapturePointCount);
	int32 PlacedPointCount = 0;
	for (TActorIterator<ADominationCapturePoint> Iterator(World); Iterator; ++Iterator)
	{
		ADominationCapturePoint* CapturePoint = *Iterator;
		if (!IsValid(CapturePoint))
		{
			continue;
		}

		++PlacedPointCount;
		const EDominationPointId PointId = CapturePoint->GetPointId();
		const int32 PointIndex = DominationRules::GetPointIndex(PointId);
		if (PointIndex == INDEX_NONE)
		{
			UE_LOG(LogDominationObjective, Error,
				TEXT("Domination capture point %s has no PointId."),
				*GetNameSafe(CapturePoint));
			return false;
		}

		if (CapturePoints[PointIndex].IsValid())
		{
			UE_LOG(LogDominationObjective, Error,
				TEXT("Domination capture point id %d is duplicated."),
				static_cast<uint8>(PointId));
			return false;
		}

		CapturePoints[PointIndex] = CapturePoint;
	}

	if (PlacedPointCount != RequiredCapturePointCount)
	{
		UE_LOG(LogDominationObjective, Error,
			TEXT("Domination requires exactly %d capture points, but found %d."),
			RequiredCapturePointCount,
			PlacedPointCount);
		return false;
	}

	for (int32 PointIndex = 0; PointIndex < CapturePoints.Num(); ++PointIndex)
	{
		if (!CapturePoints[PointIndex].IsValid())
		{
			UE_LOG(LogDominationObjective, Error,
				TEXT("Domination capture point index %d is missing."),
				PointIndex);
			return false;
		}
	}

	return true;
}

ADominationCapturePoint* UDominationObjectiveManager::GetCapturePoint(
	const EDominationPointId PointId) const
{
	const int32 PointIndex = DominationRules::GetPointIndex(PointId);
	return CapturePoints.IsValidIndex(PointIndex) ? CapturePoints[PointIndex].Get() : nullptr;
}

ADominationGameState* UDominationObjectiveManager::GetDominationGameState() const
{
	const ADominationGameMode* GameMode = OwnerGameMode.Get();
	return IsValid(GameMode) ? GameMode->GetGameState<ADominationGameState>() : nullptr;
}
