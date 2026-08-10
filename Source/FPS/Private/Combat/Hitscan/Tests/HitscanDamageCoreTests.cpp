#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/Hitscan/HitscanDamageCalculator.h"
#include "Combat/Hitscan/HitscanPenetrationCalculator.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanLinearFalloffTest,
	"FPS.Combat.Hitscan.Damage.LinearFalloff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanLinearFalloffTest::RunTest(const FString& Parameters)
{
	FHitscanDistanceFalloffSettings Settings;
	Settings.EffectiveRangeMeters = 20.0f;
	Settings.MaximumRangeMeters = 40.0f;
	Settings.MinimumDamageMultiplierAtMaximumRange = 0.5f;

	TestTrue(
		TEXT("유효 사거리 이전에는 대미지 배율 1.0"),
		FMath::IsNearlyEqual(
			FHitscanDamageCalculator::CalculateDistanceMultiplier(10.0f, Settings),
			1.0f));
	TestTrue(
		TEXT("감쇄 구간 중간에서는 대미지 배율 0.75"),
		FMath::IsNearlyEqual(
			FHitscanDamageCalculator::CalculateDistanceMultiplier(30.0f, Settings),
			0.75f));
	TestTrue(
		TEXT("최대 사거리에서는 설정된 최소 배율 사용"),
		FMath::IsNearlyEqual(
			FHitscanDamageCalculator::CalculateDistanceMultiplier(40.0f, Settings),
			0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanSequentialPenetrationTest,
	"FPS.Combat.Hitscan.Penetration.SequentialSurfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanSequentialPenetrationTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 3;
	Settings.MinimumDamageMultiplierToContinue = 0.0f;

	FHitscanSurfaceSample FirstSurface;
	FirstSurface.ThicknessCentimeters = 10.0f;
	FirstSurface.SurfaceRule.Hardness = 10.0f;
	FirstSurface.SurfaceRule.PenetrationCostPerCentimeter = 2.0f;
	FirstSurface.SurfaceRule.DamageRetentionPerCentimeter = 0.98f;

	FHitscanSurfaceSample SecondSurface;
	SecondSurface.ThicknessCentimeters = 5.0f;
	SecondSurface.SurfaceRule.Hardness = 20.0f;
	SecondSurface.SurfaceRule.PenetrationCostPerCentimeter = 2.0f;
	SecondSurface.SurfaceRule.DamageRetentionPerCentimeter = 0.95f;

	TArray<FHitscanSurfaceSample> Surfaces;
	Surfaces.Add(FirstSurface);
	Surfaces.Add(SecondSurface);

	const FHitscanPenetrationSequenceResult Result =
		FHitscanPenetrationCalculator::ResolveSequence(100.0f, Surfaces, Settings);

	const float ExpectedPower = 40.0f;
	const float ExpectedDamageMultiplier = FMath::Pow(0.98f, 10.0f)
		* FMath::Pow(0.95f, 5.0f);

	TestTrue(TEXT("두 표면을 모두 관통"), Result.bReachedEnd);
	TestEqual(TEXT("관통 횟수"), Result.FinalState.PenetrationCount, 2);
	TestTrue(
		TEXT("남은 관통력 누적 계산"),
		FMath::IsNearlyEqual(Result.FinalState.RemainingPenetrationPower, ExpectedPower));
	TestTrue(
		TEXT("대미지 유지 배율 누적 계산"),
		FMath::IsNearlyEqual(
			Result.FinalState.AccumulatedDamageMultiplier,
			ExpectedDamageMultiplier));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanDamageRoundingTest,
	"FPS.Combat.Hitscan.Damage.Rounding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanDamageRoundingTest::RunTest(const FString& Parameters)
{
	FHitscanDamageRoundingSettings Settings;
	Settings.Step = 0.1f;

	Settings.Mode = EHitscanDamageRoundingMode::Nearest;
	TestTrue(
		TEXT("Nearest 반올림"),
		FMath::IsNearlyEqual(FHitscanDamageCalculator::RoundDamage(12.36f, Settings), 12.4f, 0.0001f));

	Settings.Mode = EHitscanDamageRoundingMode::Floor;
	TestTrue(
		TEXT("Floor 반올림"),
		FMath::IsNearlyEqual(FHitscanDamageCalculator::RoundDamage(12.39f, Settings), 12.3f, 0.0001f));

	Settings.Mode = EHitscanDamageRoundingMode::Ceil;
	TestTrue(
		TEXT("Ceil 반올림"),
		FMath::IsNearlyEqual(FHitscanDamageCalculator::RoundDamage(12.31f, Settings), 12.4f, 0.0001f));
	return true;
}

#endif
