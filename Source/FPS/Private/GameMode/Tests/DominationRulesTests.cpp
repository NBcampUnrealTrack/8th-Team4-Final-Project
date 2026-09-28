#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameMode/Types/DominationTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDominationCaptureTimeRuleTest,
	"FPS.GameFlow.Domination.Rules.CaptureTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDominationCaptureTimeRuleTest::RunTest(const FString& Parameters)
{
	const FDominationCaptureRules Rules;

	TestEqual(TEXT("One player captures in 30 seconds"), Rules.GetCaptureTime(1, false), 30.0f);
	TestEqual(TEXT("Two players capture in 22 seconds"), Rules.GetCaptureTime(2, false), 22.0f);
	TestEqual(TEXT("Three players capture in 15 seconds"), Rules.GetCaptureTime(3, false), 15.0f);
	TestEqual(TEXT("Additional players do not shorten the 15-second cap"), Rules.GetCaptureTime(4, false), 15.0f);

	TestEqual(TEXT("Sudden death ignores one-player timing"), Rules.GetCaptureTime(1, true), 10.0f);
	TestEqual(TEXT("Sudden death ignores multi-player timing"), Rules.GetCaptureTime(4, true), 10.0f);
	return true;
}

// 중계 비콘 부활 대기 감소 규칙 테스트
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDominationRespawnDelayRuleTest,
	"FPS.GameFlow.Domination.Rules.RespawnDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDominationRespawnDelayRuleTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("No reduction keeps the base delay"), DominationRules::GetRespawnDelay(8.0f, 0.0f), 8.0f);
	TestEqual(TEXT("Relay reduction shortens 8 seconds to 6"), DominationRules::GetRespawnDelay(8.0f, 2.0f), 6.0f);
	TestEqual(TEXT("Negative reduction is ignored"), DominationRules::GetRespawnDelay(8.0f, -2.0f), 8.0f);
	return true;
}

#endif
