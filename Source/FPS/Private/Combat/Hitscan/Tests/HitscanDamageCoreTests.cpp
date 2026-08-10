#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/Hitscan/HitscanDamageCalculator.h"
#include "Combat/Hitscan/HitscanPenetrationCalculator.h"
#include "Combat/Hitscan/HitscanPelletAccumulator.h"
#include "Combat/Hitscan/HitZoneDefinition.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanBoneRuleHeadshotWithoutTagTest,
	"FPS.Combat.Hitscan.HitZone.BoneRuleHeadshotWithoutTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanBoneRuleHeadshotWithoutTagTest::RunTest(const FString& Parameters)
{
	FHitscanHitZoneRule HeadRule;
	HeadRule.BoneNames.Add(TEXT("head"));
	HeadRule.Priority = 100;
	HeadRule.bCountsAsHeadshot = true;

	TArray<FHitscanHitZoneRule> Rules;
	Rules.Add(HeadRule);

	const FHitscanResolvedHitZoneInfo Result =
		FHitscanHitZoneResolver::Resolve(Rules, TEXT("head"), FGameplayTag());

	TestFalse(TEXT("GameplayTag 미등록 상태에서는 Tag가 비어 있음"), Result.HitZoneTag.IsValid());
	TestEqual(TEXT("Bone 규칙 Priority는 유지"), Result.Priority, 100);
	TestTrue(TEXT("Bone 규칙만으로 Headshot 판정 가능"), Result.bHeadshot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanPenetrationInsufficientPowerTest,
	"FPS.Combat.Hitscan.Penetration.InsufficientPowerStops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanPenetrationInsufficientPowerTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 3;
	Settings.MinimumDamageMultiplierToContinue = 0.0f;

	FHitscanSurfaceSample Surface;
	Surface.ThicknessCentimeters = 20.0f;
	Surface.SurfaceRule.Hardness = 20.0f;
	Surface.SurfaceRule.PenetrationCostPerCentimeter = 2.0f;
	Surface.SurfaceRule.DamageRetentionPerCentimeter = 0.98f;

	FHitscanPenetrationState State;
	State.RemainingPenetrationPower = 50.0f;
	State.AccumulatedDamageMultiplier = 1.0f;
	State.PenetrationCount = 0;

	const FHitscanPenetrationStepResult Result =
		FHitscanPenetrationCalculator::ResolveSurface(State, Surface, Settings);

	TestFalse(TEXT("필요 관통력보다 부족하면 관통하지 않음"), Result.bPenetrated);
	TestTrue(
		TEXT("관통 실패 시 관통력 유지"),
		FMath::IsNearlyEqual(Result.ResultingState.RemainingPenetrationPower, 50.0f));
	TestEqual(TEXT("관통 실패 시 횟수 증가 없음"), Result.ResultingState.PenetrationCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanShotgunTargetAggregationTest,
	"FPS.Combat.Hitscan.Damage.ShotgunTargetAggregation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanShotgunTargetAggregationTest::RunTest(const FString& Parameters)
{
	AActor* TargetA = GetMutableDefault<AActor>();
	APawn* TargetB = GetMutableDefault<APawn>();
	if (!TestNotNull(TEXT("Target A CDO"), TargetA)
		|| !TestNotNull(TEXT("Target B CDO"), TargetB))
	{
		return false;
	}

	FHitscanPelletAccumulator Accumulator;

	auto AddPellet = [&Accumulator](
		AActor* Target,
		const int32 PelletIndex,
		const float DamageBeforeHitZone,
		const float FinalDamage,
		const bool bHeadshot,
		const float DistanceMeters = 10.0f)
	{
		FHitscanResolvedPelletDamage Pellet;
		Pellet.TargetActor = Target;
		Pellet.DamageBeforeHitZoneMultiplier = DamageBeforeHitZone;
		Pellet.FinalDamage = FinalDamage;
		Pellet.HitZoneMultiplier = DamageBeforeHitZone > KINDA_SMALL_NUMBER
			? FinalDamage / DamageBeforeHitZone
			: 1.0f;
		Pellet.DistanceMeters = DistanceMeters;
		Pellet.HitZonePriority = bHeadshot ? 100 : 50;
		Pellet.PelletIndex = PelletIndex;
		Pellet.bHeadshot = bHeadshot;
		return Accumulator.AddPellet(Pellet);
	};

	for (int32 PelletIndex = 0; PelletIndex < 5; ++PelletIndex)
	{
		const float DistanceMeters = 8.0f + static_cast<float>(PelletIndex);
		TestTrue(
			TEXT("Target A 몸통 펠릿 추가"),
			AddPellet(TargetA, PelletIndex, 12.0f, 12.0f, false, DistanceMeters));
	}
	for (int32 PelletIndex = 5; PelletIndex < 8; ++PelletIndex)
	{
		const float DistanceMeters = 8.0f + static_cast<float>(PelletIndex);
		TestTrue(
			TEXT("Target A 헤드 펠릿 추가"),
			AddPellet(TargetA, PelletIndex, 12.0f, 18.0f, true, DistanceMeters));
	}

	TestFalse(
		TEXT("동일 Target의 동일 PelletIndex 중복은 거부"),
		AddPellet(TargetA, 7, 12.0f, 18.0f, true));

	TestTrue(TEXT("Target B 첫 펠릿 추가"), AddPellet(TargetB, 8, 10.0f, 10.0f, false));
	TestTrue(TEXT("Target B 두 번째 펠릿 추가"), AddPellet(TargetB, 9, 10.0f, 10.0f, false));

	FHitscanDamageRoundingSettings Rounding;
	Rounding.Mode = EHitscanDamageRoundingMode::None;

	TArray<FHitscanTargetDamageAggregate> Aggregates;
	Accumulator.BuildTargetAggregates(Rounding, Aggregates);

	TestEqual(TEXT("한 Shot에서 대상별 Aggregate 1건"), Aggregates.Num(), 2);

	const FHitscanTargetDamageAggregate* AggregateA = Aggregates.FindByPredicate(
		[TargetA](const FHitscanTargetDamageAggregate& Aggregate)
		{
			return Aggregate.TargetActor.Get() == TargetA;
		});
	const FHitscanTargetDamageAggregate* AggregateB = Aggregates.FindByPredicate(
		[TargetB](const FHitscanTargetDamageAggregate& Aggregate)
		{
			return Aggregate.TargetActor.Get() == TargetB;
		});

	if (!TestNotNull(TEXT("Target A Aggregate"), AggregateA)
		|| !TestNotNull(TEXT("Target B Aggregate"), AggregateB))
	{
		return false;
	}

	TestTrue(TEXT("Target A 전체 대미지 = 114"), FMath::IsNearlyEqual(AggregateA->BaseDamage, 114.0f));
	TestTrue(TEXT("Target A 헤드 몫 = 54"), FMath::IsNearlyEqual(AggregateA->HeadshotDamage, 54.0f));
	TestTrue(
		TEXT("Target A 적용 헤드 배율 = 1.5"),
		FMath::IsNearlyEqual(AggregateA->AppliedHeadshotMultiplier, 1.5f));
	TestEqual(TEXT("Target A 적중 펠릿 수 = 8"), AggregateA->PelletsHit, 8);

	TestTrue(TEXT("Target B 전체 대미지 = 20"), FMath::IsNearlyEqual(AggregateB->BaseDamage, 20.0f));
	TestEqual(TEXT("Target B 적중 펠릿 수 = 2"), AggregateB->PelletsHit, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanHeadshotMultiplierPrecisionTest,
	"FPS.Combat.Hitscan.Damage.HeadshotMultiplierPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanHeadshotMultiplierPrecisionTest::RunTest(const FString& Parameters)
{
	AActor* Target = GetMutableDefault<AActor>();
	if (!TestNotNull(TEXT("Target CDO"), Target))
	{
		return false;
	}

	FHitscanResolvedPelletDamage Pellet;
	Pellet.TargetActor = Target;
	Pellet.DamageBeforeHitZoneMultiplier = 0.31f;
	Pellet.FinalDamage = 0.465f;
	Pellet.HitZoneMultiplier = 1.5f;
	Pellet.DistanceMeters = 5.0f;
	Pellet.PelletIndex = 0;
	Pellet.bHeadshot = true;

	FHitscanPelletAccumulator Accumulator;
	TestTrue(TEXT("헤드 펠릿 추가"), Accumulator.AddPellet(Pellet));

	FHitscanDamageRoundingSettings Rounding;
	Rounding.Mode = EHitscanDamageRoundingMode::Nearest;
	Rounding.Step = 0.1f;

	TArray<FHitscanTargetDamageAggregate> Aggregates;
	Accumulator.BuildTargetAggregates(Rounding, Aggregates);
	if (!TestEqual(TEXT("Aggregate 1건"), Aggregates.Num(), 1))
	{
		return false;
	}

	TestTrue(
		TEXT("반올림과 무관하게 원래 적용 헤드 배율 1.5 유지"),
		FMath::IsNearlyEqual(Aggregates[0].AppliedHeadshotMultiplier, 1.5f, 0.0001f));
	return true;
}

#endif
