#include "GameMode/Manager/DominationAnnouncerManager.h"

#include "Audio/DominationSystemAnnouncer.h"
#include "Engine/World.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/State/DominationGameState.h"

bool UDominationAnnouncerManager::Initialize(
	ADominationGameMode* InOwnerGameMode,
	TSubclassOf<ASystemAnnouncer> AnnouncerClass)
{
	Shutdown();
	if (!InitializeManager(InOwnerGameMode))
	{
		return false;
	}

	// 클래스를 비워두면 아나운서만 끄고 게임 진행은 유지한다.
	if (!AnnouncerClass)
	{
		return true;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetDominationGameState();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASystemAnnouncer* SpawnedAnnouncer = GetWorld()->SpawnActor<ASystemAnnouncer>(
		AnnouncerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	if (!IsValid(SpawnedAnnouncer))
	{
		Shutdown();
		return false;
	}
	SystemAnnouncer = SpawnedAnnouncer;

	ADominationGameState* DominationGameState = GetDominationGameState();
	PreviousObjectiveFlow = DominationGameState->GetObjectiveFlowState();
	PreviousCaptureState = DominationGameState->GetCaptureState();
	LastAnnouncedCaptureTeam = PreviousCaptureState.CaptureProgress > 0.0f
		? PreviousCaptureState.CapturingTeam : EGameTeam::None;

	// 초기 상태는 기준으로만 사용한다. 이미 지난 사건을 재방송하지 않는다.
	DominationGameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(
		this, &ThisClass::HandleObjectiveFlowStateChanged);
	DominationGameState->OnCaptureStateChanged.AddUniqueDynamic(
		this, &ThisClass::HandleCaptureStateChanged);
	return true;
}

void UDominationAnnouncerManager::Shutdown()
{
	if (ADominationGameState* DominationGameState = GetDominationGameState())
	{
		DominationGameState->OnObjectiveFlowStateChanged.RemoveDynamic(
			this, &ThisClass::HandleObjectiveFlowStateChanged);
		DominationGameState->OnCaptureStateChanged.RemoveDynamic(
			this, &ThisClass::HandleCaptureStateChanged);
	}

	if (ASystemAnnouncer* Announcer = SystemAnnouncer.Get())
	{
		Announcer->Destroy();
	}
	SystemAnnouncer.Reset();
	PreviousObjectiveFlow = FDominationObjectiveFlowState();
	PreviousCaptureState = FDominationCaptureState();
	LastAnnouncedCaptureTeam = EGameTeam::None;
	Super::Shutdown();
}

ASystemAnnouncer* UDominationAnnouncerManager::GetSystemAnnouncer() const
{
	return SystemAnnouncer.Get();
}

void UDominationAnnouncerManager::HandleObjectiveFlowStateChanged()
{
	const ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(DominationGameState))
	{
		return;
	}

	const FDominationObjectiveFlowState Flow = DominationGameState->GetObjectiveFlowState();
	const bool bNewCapture = Flow.CaptureSerial > PreviousObjectiveFlow.CaptureSerial;
	const bool bObjectiveActivated = Flow.Phase == EDominationObjectivePhase::CaptureEnabled
		&& Flow.ActivePointId != EDominationPointId::None
		&& (PreviousObjectiveFlow.Phase != EDominationObjectivePhase::CaptureEnabled
			|| PreviousObjectiveFlow.ActivePointId != Flow.ActivePointId);
	PreviousObjectiveFlow = Flow;

	if (Flow.Phase != EDominationObjectivePhase::CaptureEnabled || bObjectiveActivated)
	{
		LastAnnouncedCaptureTeam = EGameTeam::None;
		PreviousCaptureState = FDominationCaptureState();
	}

	if (DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress)
	{
		return;
	}

	// 완료 기록은 최종 승리 점령에도 남으므로 일반 라운드와 같은 경로로 처리한다.
	if (bNewCapture && Flow.LastCapturedPointId != EDominationPointId::None
		&& Flow.LastCapturingTeam != EGameTeam::None)
	{
		Announce(DominationAnnouncementIds::CaptureCompleted);
	}
	if (bObjectiveActivated)
	{
		Announce(DominationAnnouncementIds::ObjectiveActivated);
	}
}

void UDominationAnnouncerManager::HandleCaptureStateChanged(
	const FDominationCaptureState NewCaptureState)
{
	const FDominationCaptureState PreviousState = PreviousCaptureState;
	PreviousCaptureState = NewCaptureState;

	if (NewCaptureState.CaptureProgress <= 0.0f
		|| NewCaptureState.CapturingTeam != PreviousState.CapturingTeam)
	{
		LastAnnouncedCaptureTeam = EGameTeam::None;
	}

	const ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::InProgress
		|| !DominationGameState->IsObjectiveCaptureEnabled()
		|| NewCaptureState.bContested
		|| NewCaptureState.CapturingTeam == EGameTeam::None
		|| NewCaptureState.CaptureProgress <= 0.0f)
	{
		return;
	}

	// 입장/인원 변경만으로 방송하지 않는다. 되돌리는 중이 아닌 실제 진행률 증가를 확인한다.
	const bool bProgressIncreased = NewCaptureState.CaptureProgress > PreviousState.CaptureProgress;
	if (bProgressIncreased && LastAnnouncedCaptureTeam != NewCaptureState.CapturingTeam)
	{
		LastAnnouncedCaptureTeam = NewCaptureState.CapturingTeam;
		Announce(DominationAnnouncementIds::CaptureStarted);
	}
}

void UDominationAnnouncerManager::Announce(const FName AnnouncementId)
{
	if (ASystemAnnouncer* Announcer = SystemAnnouncer.Get())
	{
		Announcer->Announce(AnnouncementId);
	}
}
