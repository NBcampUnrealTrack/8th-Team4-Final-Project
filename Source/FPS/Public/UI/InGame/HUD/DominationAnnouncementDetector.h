// DominationAnnouncementDetector.h
#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"

enum class EDominationAnnouncementType : uint8
{
	MatchStarted,
	CaptureStarted,
	CaptureCompleted
};

// 문구 띄울 사건
struct FDominationAnnouncement
{
	EDominationAnnouncementType Type = EDominationAnnouncementType::MatchStarted;
	EDominationPointId PointId = EDominationPointId::None;
	EGameTeam EventTeam = EGameTeam::None;
};

// Detector가 알림 사건을 판정할 때 필요한 현재 도미네이션 상태.
struct FDominationAnnouncementSnapshot
{
	EDominationMatchPhase MatchPhase = EDominationMatchPhase::WaitingForPlayers;
	FDominationObjectiveFlowState ObjectiveFlow;
	FDominationCaptureState Capture;
	bool bCaptureEnabled = false;
	float TransitionTimeRemaining = 0.0f;
};

// 이전 도미네이션 상태와 현재 상태를 비교해 알림이 필요한지 판정
struct FPS_API FDominationAnnouncementDetector
{
	TOptional<FDominationAnnouncement> Initialize(const FDominationAnnouncementSnapshot& Snapshot);
	TOptional<FDominationAnnouncement> Update(const FDominationAnnouncementSnapshot& Snapshot);

	// 게임 시작 후 이 시간 안에 HUD가 늦게 만들어지면 알림 허용
	static constexpr float LateMatchStartGraceSeconds = 2.0f;
	// 서버 시간 오차 허용
	static constexpr float ServerTimeToleranceSeconds = 0.25f;

private:
	FDominationObjectiveFlowState PreviousFlow;
	FDominationCaptureState PreviousCapture;
	EGameTeam LastBannerCaptureTeam = EGameTeam::None;
	int32 LastHandledCaptureSerial = 0;
};
