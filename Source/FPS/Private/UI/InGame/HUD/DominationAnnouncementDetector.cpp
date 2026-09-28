// DominationAnnouncementDetector.cpp
#include "UI/InGame/HUD/DominationAnnouncementDetector.h"

namespace DominationAnnouncementDetectorPrivate
{
	// 첫 점령 대기 C
	bool IsInitialTransition(const FDominationObjectiveFlowState& Flow)
	{
		return Flow.Phase == EDominationObjectivePhase::Repositioning
			&& Flow.CaptureSerial == 0
			&& Flow.LastCapturedPointId == EDominationPointId::None;
	}
}

TOptional<FDominationAnnouncement> FDominationAnnouncementDetector::Initialize(
	const FDominationAnnouncementSnapshot& Snapshot)
{
	PreviousFlow = Snapshot.ObjectiveFlow;
	PreviousCapture = Snapshot.Capture;
	LastHandledCaptureSerial = Snapshot.ObjectiveFlow.CaptureSerial;
	LastBannerCaptureTeam = Snapshot.Capture.CaptureProgress > 0.0f
		? Snapshot.Capture.CapturingTeam
		: EGameTeam::None;

	// 늦게 생긴 HUD 2초 이내시 보장
	const float Elapsed = Snapshot.ObjectiveFlow.TransitionDuration - Snapshot.TransitionTimeRemaining;
	if (Snapshot.MatchPhase == EDominationMatchPhase::InProgress
		&& DominationAnnouncementDetectorPrivate::IsInitialTransition(Snapshot.ObjectiveFlow)
		&& Elapsed >= -ServerTimeToleranceSeconds
		&& Elapsed <= LateMatchStartGraceSeconds)
	{
		return FDominationAnnouncement{ EDominationAnnouncementType::MatchStarted, Snapshot.ObjectiveFlow.ActivePointId };
	}

	return {};
}

TOptional<FDominationAnnouncement> FDominationAnnouncementDetector::Update(
	const FDominationAnnouncementSnapshot& Snapshot)
{
	const FDominationObjectiveFlowState& Flow = Snapshot.ObjectiveFlow;
	const FDominationCaptureState& Capture = Snapshot.Capture;

	// 거점이 바뀌거나 잠기면 점령 추적을 새로
	const bool bObjectiveActivated = Flow.Phase == EDominationObjectivePhase::CaptureEnabled
		&& Flow.ActivePointId != EDominationPointId::None
		&& (PreviousFlow.Phase != EDominationObjectivePhase::CaptureEnabled
			|| PreviousFlow.ActivePointId != Flow.ActivePointId);
	if (Flow.Phase != EDominationObjectivePhase::CaptureEnabled || bObjectiveActivated)
	{
		LastBannerCaptureTeam = EGameTeam::None;
		PreviousCapture = FDominationCaptureState();
	}
	const bool bEnteredInitialTransition = DominationAnnouncementDetectorPrivate::IsInitialTransition(Flow)
		&& PreviousFlow.Phase != EDominationObjectivePhase::Repositioning;
	PreviousFlow = Flow;

	// 페이즈 검사 전에 갱신
	const bool bNewCapture = Flow.CaptureSerial > 0 && Flow.CaptureSerial != LastHandledCaptureSerial;
	LastHandledCaptureSerial = Flow.CaptureSerial;

	// 점령 시도 추적
	const FDominationCaptureState LastCapture = PreviousCapture;
	PreviousCapture = Capture;
	if (Capture.CaptureProgress <= 0.0f || Capture.CapturingTeam != LastCapture.CapturingTeam)
	{
		LastBannerCaptureTeam = EGameTeam::None;
	}
	const bool bCaptureStarted = Snapshot.bCaptureEnabled
		&& !Capture.bContested
		&& Capture.CapturingTeam != EGameTeam::None
		&& Capture.CaptureProgress > 0.0f
		&& Capture.CaptureProgress > LastCapture.CaptureProgress
		&& LastBannerCaptureTeam != Capture.CapturingTeam;

	if (Snapshot.MatchPhase != EDominationMatchPhase::InProgress)
	{
		return {};
	}

	if (bNewCapture)
	{
		// 마지막 점령은 결과 화면에 맡김
		if (Flow.ActivePointId == Flow.LastCapturedPointId)
		{
			return {};
		}
		return FDominationAnnouncement{
			EDominationAnnouncementType::CaptureCompleted, Flow.LastCapturedPointId, Flow.LastCapturingTeam };
	}

	if (bEnteredInitialTransition)
	{
		return FDominationAnnouncement{ EDominationAnnouncementType::MatchStarted, Flow.ActivePointId };
	}

	if (bCaptureStarted)
	{
		LastBannerCaptureTeam = Capture.CapturingTeam;
		return FDominationAnnouncement{
			EDominationAnnouncementType::CaptureStarted, Flow.ActivePointId, Capture.CapturingTeam };
	}

	return {};
}
