// DominationAnnouncementDetectorTests.cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "UI/InGame/HUD/DominationAnnouncementDetector.h"

namespace DominationAnnouncementDetectorTest
{
	FDominationAnnouncementSnapshot MakeCaptureEnabled(const EDominationPointId PointId)
	{
		FDominationAnnouncementSnapshot Snapshot;
		Snapshot.MatchPhase = EDominationMatchPhase::InProgress;
		Snapshot.ObjectiveFlow.ActivePointId = PointId;
		Snapshot.ObjectiveFlow.Phase = EDominationObjectivePhase::CaptureEnabled;
		Snapshot.bCaptureEnabled = true;
		return Snapshot;
	}

	// C 거점 점령 상태
	FDominationAnnouncementSnapshot MakeCapturing(const EGameTeam Team, const float Progress, const bool bContested = false)
	{
		FDominationAnnouncementSnapshot Snapshot = MakeCaptureEnabled(EDominationPointId::C);
		Snapshot.Capture.CapturingTeam = Team;
		Snapshot.Capture.CaptureProgress = Progress;
		Snapshot.Capture.bContested = bContested;
		return Snapshot;
	}

	// 서버가 점령을 기록한 직후
	FDominationAnnouncementSnapshot MakeCaptureRecorded(
		const EDominationPointId CapturedPointId,
		const EGameTeam CapturingTeam,
		const EDominationPointId ActivePointId,
		const int32 CaptureSerial)
	{
		const bool bFinalCapture = ActivePointId == CapturedPointId;

		FDominationAnnouncementSnapshot Snapshot;
		Snapshot.MatchPhase = EDominationMatchPhase::InProgress;
		Snapshot.ObjectiveFlow.ActivePointId = ActivePointId;
		Snapshot.ObjectiveFlow.Phase = bFinalCapture
			? EDominationObjectivePhase::CaptureEnabled
			: EDominationObjectivePhase::Repositioning;
		Snapshot.ObjectiveFlow.TransitionDuration = bFinalCapture ? 0.0f : 30.0f;
		Snapshot.ObjectiveFlow.LastCapturedPointId = CapturedPointId;
		Snapshot.ObjectiveFlow.LastCapturingTeam = CapturingTeam;
		Snapshot.ObjectiveFlow.CaptureSerial = CaptureSerial;
		Snapshot.bCaptureEnabled = bFinalCapture;
		Snapshot.TransitionTimeRemaining = Snapshot.ObjectiveFlow.TransitionDuration;
		return Snapshot;
	}

	// 최초 C 30초 대기	
	FDominationAnnouncementSnapshot MakeInitialTransition(const float TimeRemaining)
	{
		FDominationAnnouncementSnapshot Snapshot;
		Snapshot.MatchPhase = EDominationMatchPhase::InProgress;
		Snapshot.ObjectiveFlow.ActivePointId = EDominationPointId::C;
		Snapshot.ObjectiveFlow.Phase = EDominationObjectivePhase::Repositioning;
		Snapshot.ObjectiveFlow.TransitionDuration = 30.0f;
		Snapshot.TransitionTimeRemaining = TimeRemaining;
		return Snapshot;
	}

	FDominationAnnouncementSnapshot MakeInactive()
	{
		FDominationAnnouncementSnapshot Snapshot;
		Snapshot.MatchPhase = EDominationMatchPhase::InProgress;
		return Snapshot;
	}

	void ExpectAnnouncement(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const TOptional<FDominationAnnouncement>& Actual,
		const EDominationAnnouncementType Type,
		const EDominationPointId PointId,
		const EGameTeam EventTeam)
	{
		if (!Test.TestTrue(What, Actual.IsSet()))
		{
			return;
		}
		Test.TestEqual(What, static_cast<int32>(Actual->Type), static_cast<int32>(Type));
		Test.TestEqual(What, static_cast<int32>(Actual->PointId), static_cast<int32>(PointId));
		Test.TestEqual(What, static_cast<int32>(Actual->EventTeam), static_cast<int32>(EventTeam));
	}

	void ExpectNone(FAutomationTestBase& Test, const TCHAR* What, const TOptional<FDominationAnnouncement>& Actual)
	{
		Test.TestFalse(What, Actual.IsSet());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDominationAnnouncementCaptureStartedTest,
	"FPS.UI.DominationAnnouncement.Detector.CaptureStarted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDominationAnnouncementCaptureStartedTest::RunTest(const FString& Parameters)
{
	using namespace DominationAnnouncementDetectorTest;

	FDominationAnnouncementDetector Detector;
	Detector.Initialize(MakeCaptureEnabled(EDominationPointId::C));

	ExpectAnnouncement(*this, TEXT("1 최초 점령 시작"),
		Detector.Update(MakeCapturing(EGameTeam::TeamA, 0.05f)),
		EDominationAnnouncementType::CaptureStarted, EDominationPointId::C, EGameTeam::TeamA);
	ExpectNone(*this, TEXT("2 같은 팀 진행률 증가"), Detector.Update(MakeCapturing(EGameTeam::TeamA, 0.10f)));
	ExpectNone(*this, TEXT("3 경합"), Detector.Update(MakeCapturing(EGameTeam::TeamA, 0.10f, true)));
	ExpectNone(*this, TEXT("4 경합 해제 후 같은 팀"), Detector.Update(MakeCapturing(EGameTeam::TeamA, 0.15f)));

	ExpectNone(*this, TEXT("5 진행률 0 · 팀 None"), Detector.Update(MakeCapturing(EGameTeam::None, 0.0f)));
	ExpectAnnouncement(*this, TEXT("5 0 리셋 뒤 재점령"),
		Detector.Update(MakeCapturing(EGameTeam::TeamA, 0.05f)),
		EDominationAnnouncementType::CaptureStarted, EDominationPointId::C, EGameTeam::TeamA);

	ExpectNone(*this, TEXT("6 A 계속"), Detector.Update(MakeCapturing(EGameTeam::TeamA, 0.10f)));
	ExpectNone(*this, TEXT("6 B 가 되돌리는 중"), Detector.Update(MakeCapturing(EGameTeam::TeamA, 0.05f)));
	ExpectNone(*this, TEXT("6 0 에서 B 로 바뀜"), Detector.Update(MakeCapturing(EGameTeam::TeamB, 0.0f)));
	ExpectAnnouncement(*this, TEXT("6 B 점령 시작"),
		Detector.Update(MakeCapturing(EGameTeam::TeamB, 0.03f)),
		EDominationAnnouncementType::CaptureStarted, EDominationPointId::C, EGameTeam::TeamB);

	FDominationAnnouncementDetector LateDetector;
	ExpectNone(*this, TEXT("12 생성 시 진행 중 점령은 기준값"),
		LateDetector.Initialize(MakeCapturing(EGameTeam::TeamA, 0.4f)));
	ExpectNone(*this, TEXT("12 이어서 같은 팀"), LateDetector.Update(MakeCapturing(EGameTeam::TeamA, 0.45f)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDominationAnnouncementCaptureCompletedTest,
	"FPS.UI.DominationAnnouncement.Detector.CaptureCompleted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDominationAnnouncementCaptureCompletedTest::RunTest(const FString& Parameters)
{
	using namespace DominationAnnouncementDetectorTest;

	FDominationAnnouncementDetector Detector;
	Detector.Initialize(MakeCapturing(EGameTeam::TeamA, 0.9f));
	ExpectAnnouncement(*this, TEXT("7 CaptureSerial 변화"),
		Detector.Update(MakeCaptureRecorded(EDominationPointId::C, EGameTeam::TeamA, EDominationPointId::D, 1)),
		EDominationAnnouncementType::CaptureCompleted, EDominationPointId::C, EGameTeam::TeamA);

	FDominationAnnouncementSnapshot BeforeFinal = MakeCaptureEnabled(EDominationPointId::E);
	BeforeFinal.ObjectiveFlow.CaptureSerial = 2;
	BeforeFinal.ObjectiveFlow.LastCapturedPointId = EDominationPointId::D;
	BeforeFinal.ObjectiveFlow.LastCapturingTeam = EGameTeam::TeamA;

	FDominationAnnouncementDetector FinalDetector;
	FinalDetector.Initialize(BeforeFinal);
	ExpectNone(*this, TEXT("8 마지막 점령은 생략"),
		FinalDetector.Update(MakeCaptureRecorded(EDominationPointId::E, EGameTeam::TeamA, EDominationPointId::E, 3)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDominationAnnouncementMatchStartedTest,
	"FPS.UI.DominationAnnouncement.Detector.MatchStarted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDominationAnnouncementMatchStartedTest::RunTest(const FString& Parameters)
{
	using namespace DominationAnnouncementDetectorTest;

	{
		FDominationAnnouncementDetector Detector;
		ExpectAnnouncement(*this, TEXT("9 시작 후 1.5초에 생성"),
			Detector.Initialize(MakeInitialTransition(28.5f)),
			EDominationAnnouncementType::MatchStarted, EDominationPointId::C, EGameTeam::None);
	}
	{
		FDominationAnnouncementDetector Detector;
		ExpectNone(*this, TEXT("10 시작 후 10초에 생성"), Detector.Initialize(MakeInitialTransition(20.0f)));
	}
	{
		FDominationAnnouncementDetector Detector;
		Detector.Initialize(MakeInactive());
		ExpectAnnouncement(*this, TEXT("11 최초 전환 진입 관찰"),
			Detector.Update(MakeInitialTransition(30.0f)),
			EDominationAnnouncementType::MatchStarted, EDominationPointId::C, EGameTeam::None);
	}
	{
		FDominationAnnouncementDetector Detector;
		ExpectAnnouncement(*this, TEXT("13 경과 -0.1초는 오차 안"),
			Detector.Initialize(MakeInitialTransition(30.1f)),
			EDominationAnnouncementType::MatchStarted, EDominationPointId::C, EGameTeam::None);
	}
	{
		FDominationAnnouncementDetector Detector;
		ExpectNone(*this, TEXT("14 경과 -1초는 하한 밖"), Detector.Initialize(MakeInitialTransition(31.0f)));
	}

	return true;
}

#endif
