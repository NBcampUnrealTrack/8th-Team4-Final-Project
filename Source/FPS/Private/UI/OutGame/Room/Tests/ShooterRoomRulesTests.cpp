#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "UI/OutGame/Room/ShooterRoomTypes.h"

namespace
{
	FShooterRoomSlotView MakeView(const int32 PlayerId, const EGameTeam Team)
	{
		FShooterRoomSlotView View;
		View.PlayerId = PlayerId;
		View.Team = Team;
		return View;
	}

	// TestEqual 에 스코프 열거형을 바로 넘길 수 없어 문자열로 비교함
	FString PresentationToString(const EShooterRoomSlotPresentation Presentation)
	{
		switch (Presentation)
		{
		case EShooterRoomSlotPresentation::Snap:
			return TEXT("Snap");
		case EShooterRoomSlotPresentation::Animate:
			return TEXT("Animate");
		default:
			return TEXT("None");
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterRoomNameSanitizeTest,
	"FPS.UI.Lobby.Rules.SanitizeRoomName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterRoomNameSanitizeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("앞뒤 공백을 지운다"),
		FShooterRoomRules::SanitizeRoomName(TEXT("  내 방  "), TEXT("Nick")),
		FString(TEXT("내 방")));

	TestEqual(TEXT("24자를 넘는 입력은 자른다"),
		FShooterRoomRules::SanitizeRoomName(FString::ChrN(30, TEXT('A')), TEXT("Nick")).Len(),
		24);

	TestEqual(TEXT("빈 입력은 내 이름으로 기본 이름을 만든다"),
		FShooterRoomRules::SanitizeRoomName(TEXT(""), TEXT("Nick")),
		FString(TEXT("Nick 님의 게임")));

	TestEqual(TEXT("공백뿐인 입력도 기본 이름이다"),
		FShooterRoomRules::SanitizeRoomName(TEXT("   "), TEXT("Nick")),
		FString(TEXT("Nick 님의 게임")));

	TestEqual(TEXT("이름도 비면 플레이어로 부른다"),
		FShooterRoomRules::SanitizeRoomName(TEXT(""), TEXT("")),
		FString(TEXT("플레이어 님의 게임")));

	const FString LongNick = FString::ChrN(30, TEXT('B'));
	TestTrue(TEXT("긴 닉네임의 기본 이름은 어미가 살아 있다"),
		FShooterRoomRules::SanitizeRoomName(TEXT(""), LongNick).EndsWith(TEXT(" 님의 게임")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterRoomTeamPlayersTest,
	"FPS.UI.Lobby.Rules.TeamPlayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterRoomTeamPlayersTest::RunTest(const FString& Parameters)
{
	const TArray<FShooterRoomSlotView> All = {
		MakeView(300, EGameTeam::TeamB),
		MakeView(258, EGameTeam::TeamA),
		MakeView(999, EGameTeam::None),
		MakeView(256, EGameTeam::TeamA),
	};

	const TArray<FShooterRoomSlotView> TeamA = FShooterRoomRules::GetTeamPlayers(All, EGameTeam::TeamA);
	TestEqual(TEXT("팀 A 는 둘이다"), TeamA.Num(), 2);
	TestEqual(TEXT("PlayerId 오름차순이다"), TeamA[0].PlayerId, 256);
	TestEqual(TEXT("두 번째는 258"), TeamA[1].PlayerId, 258);

	const TArray<FShooterRoomSlotView> TeamB = FShooterRoomRules::GetTeamPlayers(All, EGameTeam::TeamB);
	TestEqual(TEXT("팀 B 는 하나다"), TeamB.Num(), 1);

	const TArray<FShooterRoomSlotView> Unassigned = FShooterRoomRules::GetTeamPlayers(All, EGameTeam::None);
	TestEqual(TEXT("미배정은 어느 팀에도 섞이지 않는다"), Unassigned.Num(), 1);
	TestEqual(TEXT("미배정은 999 다"), Unassigned[0].PlayerId, 999);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterRoomReadyPresentationTest,
	"FPS.UI.Lobby.Rules.ReadyPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterRoomReadyPresentationTest::RunTest(const FString& Parameters)
{
	FShooterRoomSlotView View = MakeView(256, EGameTeam::TeamA);
	View.bIsReady = false;
	View.bIsHost = false;

	TestEqual(TEXT("빈 칸에 사람이 들어오면 즉시 적용"),
		PresentationToString(FShooterRoomRules::DecideReadyPresentation(INDEX_NONE, false, false, View)),
		FString(TEXT("Snap")));

	TestEqual(TEXT("다른 PlayerId 면 즉시 적용"),
		PresentationToString(FShooterRoomRules::DecideReadyPresentation(300, true, false, View)),
		FString(TEXT("Snap")));

	TestEqual(TEXT("같은 사람의 같은 값은 아무것도 안 한다"),
		PresentationToString(FShooterRoomRules::DecideReadyPresentation(256, false, false, View)),
		FString(TEXT("None")));

	View.bIsReady = true;
	TestEqual(TEXT("같은 사람의 레디 변화는 연출"),
		PresentationToString(FShooterRoomRules::DecideReadyPresentation(256, false, false, View)),
		FString(TEXT("Animate")));

	View.bIsReady = false;
	TestEqual(TEXT("해제도 연출"),
		PresentationToString(FShooterRoomRules::DecideReadyPresentation(256, true, false, View)),
		FString(TEXT("Animate")));

	// 호스트 승격 — 레디도 PlayerId 도 그대로인데 왕관이 붙어야 한다
	View.bIsReady = true;
	View.bIsHost = true;
	TestEqual(TEXT("호스트가 되면 레디가 같아도 즉시 적용"),
		PresentationToString(FShooterRoomRules::DecideReadyPresentation(256, true, false, View)),
		FString(TEXT("Snap")));

	TestEqual(TEXT("호스트 승격은 레디 변화보다 우선한다"),
		PresentationToString(FShooterRoomRules::DecideReadyPresentation(256, false, false, View)),
		FString(TEXT("Snap")));

	return true;
}

#endif
