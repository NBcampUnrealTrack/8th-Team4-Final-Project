// ShooterLoadingProgressTests.cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "UI/Loading/ShooterLoadingProgressTracker.h"

namespace ShooterLoadingProgressTest
{
	void TestNearly(
		FAutomationTestBase& Test,
		const FString& What,
		const float Actual,
		const float Expected,
		const float Tolerance = KINDA_SMALL_NUMBER)
	{
		Test.TestTrue(
			FString::Printf(TEXT("%s (Actual=%.6f Expected=%.6f)"), *What, Actual, Expected),
			FMath::IsNearlyEqual(Actual, Expected, Tolerance));
	}

	// 60fps 한 프레임
	constexpr float FrameDT = 1.0f / 60.0f;

	void TickFrames(
		FShooterLoadingProgressTracker& Tracker,
		const FShooterLoadingProgressSettings& Settings,
		const int32 FrameCount,
		const float DeltaSeconds = FrameDT)
	{
		for (int32 Index = 0; Index < FrameCount; ++Index)
		{
			Tracker.Tick(DeltaSeconds, Settings);
		}
	}
}

// 계단이 사라지는가
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterLoadingNoSingleTickJumpTest,
	"FPS.UI.Loading.Rules.NoSingleTickJump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterLoadingNoSingleTickJumpTest::RunTest(const FString& Parameters)
{
	using namespace ShooterLoadingProgressTest;

	const FShooterLoadingProgressSettings Settings;
	FShooterLoadingProgressTracker Tracker;

	// 정상 보간
	TestNearly(*this, TEXT("Display starts at zero"), Tracker.GetDisplay(), 0.0f);
	Tracker.SetTarget(0.20f);
	TickFrames(Tracker, Settings, 600);
	TestNearly(*this, TEXT("Display settles on 0.20"), Tracker.GetDisplay(), 0.20f);

	// 0.20 → 0.90 계단
	Tracker.SetTarget(0.90f);
	TestTrue(TEXT("Interpolating tick reports a change"), Tracker.Tick(FrameDT, Settings));
	TestTrue(TEXT("One tick does not reach 0.90"), Tracker.GetDisplay() < 0.90f);
	TestTrue(TEXT("One tick stays well below halfway"), Tracker.GetDisplay() < 0.40f);

	// 값이 연속으로 들어와도 Display 는 단조·연속이다
	float Previous = Tracker.GetDisplay();
	for (const float Raw : { 0.30f, 0.65f, 0.80f, 0.95f })
	{
		Tracker.SetTarget(Raw);
		for (int32 Index = 0; Index < 60; ++Index)
		{
			Tracker.Tick(FrameDT, Settings);
			TestTrue(TEXT("Display never goes backwards"), Tracker.GetDisplay() >= Previous);
			Previous = Tracker.GetDisplay();
		}
	}
	return true;
}

// 히치에도 순간이동하지 않는가
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterLoadingHitchClampTest,
	"FPS.UI.Loading.Rules.HitchClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterLoadingHitchClampTest::RunTest(const FString& Parameters)
{
	using namespace ShooterLoadingProgressTest;

	const FShooterLoadingProgressSettings Settings;

	// 상한이 DeltaSeconds 에 비례하면 히치 때 방어가 함께 풀린다
	const float MaxStep = Settings.MaxVisualProgressPerSecond * Settings.MaxInterpolationDeltaTime;

	for (const float HitchSeconds : { 1.0f, 2.0f, 5.0f })
	{
		FShooterLoadingProgressTracker Tracker;
		Tracker.SetTarget(0.20f);
		TickFrames(Tracker, Settings, 600);

		const float Before = Tracker.GetDisplay();
		Tracker.SetTarget(0.90f);
		Tracker.Tick(HitchSeconds, Settings);

		TestTrue(
			FString::Printf(TEXT("Hitch %.1fs step stays under cap (Step=%.6f Cap=%.6f)"),
				HitchSeconds, Tracker.GetDisplay() - Before, MaxStep),
			(Tracker.GetDisplay() - Before) <= MaxStep + KINDA_SMALL_NUMBER);
	}
	return true;
}

// 이상한 Raw 가 와도 Target 이 망가지지 않는가
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterLoadingRawGuardsTest,
	"FPS.UI.Loading.Rules.RawGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterLoadingRawGuardsTest::RunTest(const FString& Parameters)
{
	using namespace ShooterLoadingProgressTest;

	// 뒤로 가는 값이 와도 Target 은 내려가지 않는다
	{
		FShooterLoadingProgressTracker Tracker;
		Tracker.SetTarget(0.90f);
		Tracker.SetTarget(0.50f);
		TestNearly(*this, TEXT("Target keeps the maximum"), Tracker.GetTarget(), 0.90f);
	}

	// Clamp01
	{
		FShooterLoadingProgressTracker Tracker;
		Tracker.SetTarget(-0.5f);
		TestNearly(*this, TEXT("Negative raw clamps to zero"), Tracker.GetTarget(), 0.0f);
	}
	{
		FShooterLoadingProgressTracker Tracker;
		Tracker.SetTarget(1.7f);
		TestNearly(*this, TEXT("Over-one raw clamps to one"), Tracker.GetTarget(), 1.0f);
	}
	return true;
}

// 스냅이 없는가
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterLoadingCompletionInterpTest,
	"FPS.UI.Loading.Rules.CompletionInterp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterLoadingCompletionInterpTest::RunTest(const FString& Parameters)
{
	using namespace ShooterLoadingProgressTest;

	const FShooterLoadingProgressSettings Settings;
	FShooterLoadingProgressTracker Tracker;

	// 도미네이션은 0.9 에서 오래 멈춘다
	Tracker.SetTarget(0.90f);
	TickFrames(Tracker, Settings, 1800);
	TestTrue(TEXT("Display never reaches 1.0 while target is 0.90"), Tracker.GetDisplay() < 1.0f);

	// 부동소수점 꼬리가 남지 않는다
	TestEqual(TEXT("Display settles exactly on target"), Tracker.GetDisplay(), 0.90f);

	// 정착한 뒤에는 화면에 적용할 것이 없다
	TestFalse(TEXT("Settled tick reports no change"), Tracker.Tick(FrameDT, Settings));

	// 완료도 보간이다
	Tracker.SetTarget(1.0f);
	Tracker.Tick(FrameDT, Settings);
	TestTrue(TEXT("One tick does not snap to 1.0"), Tracker.GetDisplay() < 1.0f);

	TickFrames(Tracker, Settings, 600);
	TestNearly(*this, TEXT("Eventually reaches 1.0"), Tracker.GetDisplay(), 1.0f);
	return true;
}

// 두 번째 매치가 이전 상태를 물려받지 않는가
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterLoadingResetTest,
	"FPS.UI.Loading.Rules.Reset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterLoadingResetTest::RunTest(const FString& Parameters)
{
	using namespace ShooterLoadingProgressTest;

	const FShooterLoadingProgressSettings Settings;
	FShooterLoadingProgressTracker Tracker;

	// 시각 완료 판정
	Tracker.SetTarget(0.90f);
	TickFrames(Tracker, Settings, 600);
	TestFalse(TEXT("Not visually complete below threshold"), Tracker.IsVisuallyComplete(Settings));

	Tracker.SetTarget(1.0f);
	TickFrames(Tracker, Settings, 600);
	TestTrue(TEXT("Visually complete at 1.0"), Tracker.IsVisuallyComplete(Settings));

	// 새 세션
	Tracker.Reset();
	TestNearly(*this, TEXT("Display is zero"), Tracker.GetDisplay(), 0.0f);
	TestNearly(*this, TEXT("Target is zero"), Tracker.GetTarget(), 0.0f);
	TestFalse(TEXT("Not visually complete after reset"), Tracker.IsVisuallyComplete(Settings));
	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
