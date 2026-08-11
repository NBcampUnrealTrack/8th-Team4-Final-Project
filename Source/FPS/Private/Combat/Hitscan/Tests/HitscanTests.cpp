#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/Hitscan/HitZoneDefinition.h"
#include "Combat/Hitscan/HitscanBallistics.h"
#include "Combat/Hitscan/HitscanShotTypes.h"
#include "Curves/CurveFloat.h"
#include "Tags/GameplayTags.h"

namespace
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanDistanceFalloffTest,
	"FPS.Combat.Hitscan.Ballistics.DistanceFalloff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanDistanceFalloffTest::RunTest(const FString& Parameters)
{
	FHitscanDistanceFalloffSettings Settings;
	Settings.EffectiveRangeMeters = 10.0f;
	Settings.MaximumRangeMeters = 30.0f;
	Settings.MinimumDamageMultiplierAtMaximumRange = 0.5f;

	TestNearly(*this, TEXT("Inside effective range"), HitscanBallistics::CalculateDistanceMultiplier(5.0f, Settings), 1.0f);
	TestNearly(*this, TEXT("Mid falloff"), HitscanBallistics::CalculateDistanceMultiplier(20.0f, Settings), 0.75f);
	TestNearly(*this, TEXT("At max range"), HitscanBallistics::CalculateDistanceMultiplier(30.0f, Settings), 0.5f);
	TestNearly(*this, TEXT("Past max range clamps"), HitscanBallistics::CalculateDistanceMultiplier(100.0f, Settings), 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanDistanceCurveClampTest,
	"FPS.Combat.Hitscan.Ballistics.DistanceCurveClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanDistanceCurveClampTest::RunTest(const FString& Parameters)
{
	UCurveFloat* Curve = NewObject<UCurveFloat>(GetTransientPackage());
	if (!TestNotNull(TEXT("Curve"), Curve))
	{
		return false;
	}

	Curve->FloatCurve.UpdateOrAddKey(0.0f, 2.0f);
	Curve->FloatCurve.UpdateOrAddKey(1.0f, 1.5f);
	Curve->FloatCurve.UpdateOrAddKey(3.0f, -1.0f);

	FHitscanDistanceFalloffSettings Settings;
	Settings.EffectiveRangeMeters = 10.0f;
	Settings.MaximumRangeMeters = 30.0f;
	Settings.MinimumDamageMultiplierAtMaximumRange = 0.4f;
	Settings.DamageMultiplierCurve = Curve;

	TestNearly(*this, TEXT("Curve upper clamp"), HitscanBallistics::CalculateDistanceMultiplier(0.0f, Settings), 1.0f);
	TestNearly(*this, TEXT("Curve lower clamp"), HitscanBallistics::CalculateDistanceMultiplier(30.0f, Settings), 0.4f);
	TestNearly(*this, TEXT("Curve input clamps past max range"), HitscanBallistics::CalculateDistanceMultiplier(300.0f, Settings), 0.4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanBallisticDamageBoundaryTest,
	"FPS.Combat.Hitscan.Ballistics.DistancePenetrationOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanBallisticDamageBoundaryTest::RunTest(const FString& Parameters)
{
	FHitscanDistanceFalloffSettings Settings;
	Settings.EffectiveRangeMeters = 10.0f;
	Settings.MaximumRangeMeters = 30.0f;
	Settings.MinimumDamageMultiplierAtMaximumRange = 0.5f;

	// 100 * 거리 0.75 * 관통 0.8 = 60. 부위 배율은 Hitscan 계산에 들어오지 않는다.
	const float Damage = HitscanBallistics::CalculateBallisticDamage(100.0f, 20.0f, 0.8f, Settings);
	TestNearly(*this, TEXT("Only distance and penetration are applied"), Damage, 60.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanPenetrationTest,
	"FPS.Combat.Hitscan.Ballistics.Penetration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanPenetrationTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationState State;
	State.RemainingPenetrationPower = 30.0f;
	State.AccumulatedDamageMultiplier = 1.0f;

	FHitscanSurfaceRule Surface;
	Surface.Hardness = 5.0f;
	Surface.PenetrationCostPerCentimeter = 1.0f;
	Surface.DamageRetentionPerCentimeter = 0.9f;

	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 2;
	Settings.MinimumDamageMultiplierToContinue = 0.1f;

	const FHitscanPenetrationStepResult Result = HitscanBallistics::ResolvePenetration(
		State,
		10.0f,
		Surface,
		Settings);

	TestTrue(TEXT("Surface penetrated"), Result.bPenetrated);
	TestNearly(*this, TEXT("Penetration cost"), Result.PenetrationCost, 15.0f);
	TestNearly(*this, TEXT("Remaining power"), Result.ResultingState.RemainingPenetrationPower, 15.0f);
	TestEqual(TEXT("Penetration count"), Result.ResultingState.PenetrationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanThickSurfaceBoundsTest,
	"FPS.Combat.Hitscan.Penetration.ThickSurfaceBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanThickSurfaceBoundsTest::RunTest(const FString& Parameters)
{
	const FBox ThickWallBounds(
		FVector(0.0f, -100.0f, -100.0f),
		FVector(500.0f, 100.0f, 100.0f));

	float ExitDistance = 0.0f;
	TestTrue(
		TEXT("500cm wall exit distance can be resolved"),
		HitscanBallistics::CalculateRayExitDistanceFromBox(
			ThickWallBounds,
			FVector::ZeroVector,
			FVector::ForwardVector,
			ExitDistance));
	TestNearly(*this, TEXT("Thick wall exit distance"), ExitDistance, 500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanHitZoneResolveTest,
	"FPS.Combat.Hitscan.HitZone.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanHitZoneResolveTest::RunTest(const FString& Parameters)
{
	FHitscanHitZoneRule Torso;
	Torso.HitZoneTag = RogueTags::Hit_Zone_Torso;
	Torso.BoneNames = { TEXT("spine_03") };
	Torso.Priority = 50;

	FHitscanHitZoneRule Head;
	Head.HitZoneTag = RogueTags::Hit_Zone_Head;
	Head.BoneNames = { TEXT("head") };
	Head.Priority = 100;
	Head.bCountsAsHeadshot = true;

	TArray<FHitscanHitZoneRule> Rules = { Torso, Head };
	const FHitscanResolvedHitZoneInfo Result = UHitscanHitZoneDefinition::ResolveRules(
		Rules,
		TEXT("head"),
		RogueTags::Hit_Zone_Torso);

	TestTrue(TEXT("Head tag resolved"), Result.HitZoneTag.MatchesTagExact(RogueTags::Hit_Zone_Head));
	TestEqual(TEXT("Head priority"), Result.Priority, 100);
	TestTrue(TEXT("Headshot flag"), Result.bHeadshot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanFallbackDisabledByDefaultTest,
	"FPS.Combat.Hitscan.Penetration.FallbackDisabledByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanFallbackDisabledByDefaultTest::RunTest(const FString& Parameters)
{
	const FHitscanTraceSettings Settings;
	TestFalse(TEXT("Fallback thickness is opt-in"), Settings.bAllowFallbackSurfaceThickness);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanZeroCostSurfacePolicyTest,
	"FPS.Combat.Hitscan.Penetration.ZeroCostSurfacePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanZeroCostSurfacePolicyTest::RunTest(const FString& Parameters)
{
	const FHitscanTraceSettings Settings;
	TestFalse(
		TEXT("Unconfigured zero-cost surfaces fail closed by default"),
		Settings.bAllowZeroCostSurfaceRule);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanBackstabHorizontalProjectionTest,
	"FPS.Combat.Hitscan.Ballistics.BackstabHorizontalProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanBackstabHorizontalProjectionTest::RunTest(const FString& Parameters)
{
	const FVector Facing = FVector::ForwardVector;

	TestTrue(
		TEXT("Vertical component does not affect rear alignment"),
		HitscanBallistics::IsBackstabDirection(
			Facing,
			FVector(1.0f, 0.0f, 5.0f),
			45.0f));
	TestFalse(
		TEXT("Front attack is not a backstab"),
		HitscanBallistics::IsBackstabDirection(
			Facing,
			FVector(-1.0f, 0.0f, 0.0f),
			45.0f));
	TestTrue(
		TEXT("Rear cone boundary is accepted"),
		HitscanBallistics::IsBackstabDirection(
			Facing,
			FVector(1.0f, 1.0f, 0.0f),
			45.0f));
	TestFalse(
		TEXT("Outside rear cone is rejected"),
		HitscanBallistics::IsBackstabDirection(
			Facing,
			FVector(0.0f, 1.0f, 0.0f),
			45.0f));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanSequentialPenetrationTest,
	"FPS.Combat.Hitscan.Ballistics.SequentialPenetration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanSequentialPenetrationTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 3;
	Settings.MinimumDamageMultiplierToContinue = 0.0f;

	FHitscanSurfaceRule FirstSurface;
	FirstSurface.Hardness = 10.0f;
	FirstSurface.PenetrationCostPerCentimeter = 2.0f;
	FirstSurface.DamageRetentionPerCentimeter = 0.98f;

	FHitscanSurfaceRule SecondSurface;
	SecondSurface.Hardness = 20.0f;
	SecondSurface.PenetrationCostPerCentimeter = 2.0f;
	SecondSurface.DamageRetentionPerCentimeter = 0.95f;

	FHitscanPenetrationState State;
	State.RemainingPenetrationPower = 100.0f;
	State.AccumulatedDamageMultiplier = 1.0f;

	const FHitscanPenetrationStepResult First = HitscanBallistics::ResolvePenetration(
		State, 10.0f, FirstSurface, Settings);
	if (!TestTrue(TEXT("First surface penetrated"), First.bPenetrated))
	{
		return false;
	}

	const FHitscanPenetrationStepResult Second = HitscanBallistics::ResolvePenetration(
		First.ResultingState, 5.0f, SecondSurface, Settings);
	if (!TestTrue(TEXT("Second surface penetrated"), Second.bPenetrated))
	{
		return false;
	}

	const float ExpectedMultiplier = FMath::Pow(0.98f, 10.0f) * FMath::Pow(0.95f, 5.0f);
	TestEqual(TEXT("Penetration count"), Second.ResultingState.PenetrationCount, 2);
	TestNearly(*this, TEXT("Remaining penetration power"), Second.ResultingState.RemainingPenetrationPower, 40.0f);
	TestNearly(*this, TEXT("Accumulated penetration damage multiplier"), Second.ResultingState.AccumulatedDamageMultiplier, ExpectedMultiplier);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanInsufficientPenetrationPowerTest,
	"FPS.Combat.Hitscan.Ballistics.InsufficientPenetrationPower",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanInsufficientPenetrationPowerTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 3;

	FHitscanSurfaceRule Surface;
	Surface.Hardness = 20.0f;
	Surface.PenetrationCostPerCentimeter = 2.0f;
	Surface.DamageRetentionPerCentimeter = 0.98f;

	FHitscanPenetrationState State;
	State.RemainingPenetrationPower = 50.0f;
	State.AccumulatedDamageMultiplier = 1.0f;

	const FHitscanPenetrationStepResult Result = HitscanBallistics::ResolvePenetration(
		State, 20.0f, Surface, Settings);

	TestFalse(TEXT("Insufficient power stops penetration"), Result.bPenetrated);
	TestNearly(*this, TEXT("Power is unchanged on failure"), Result.ResultingState.RemainingPenetrationPower, 50.0f);
	TestEqual(TEXT("Count is unchanged on failure"), Result.ResultingState.PenetrationCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanDamageRoundingTest,
	"FPS.Combat.Hitscan.Ballistics.Rounding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanDamageRoundingTest::RunTest(const FString& Parameters)
{
	FHitscanDamageRoundingSettings Settings;
	Settings.Step = 0.1f;

	Settings.Mode = EHitscanDamageRoundingMode::Nearest;
	TestNearly(*this, TEXT("Nearest"), HitscanBallistics::RoundDamage(12.36f, Settings), 12.4f, 0.0001f);

	Settings.Mode = EHitscanDamageRoundingMode::Floor;
	TestNearly(*this, TEXT("Floor"), HitscanBallistics::RoundDamage(12.39f, Settings), 12.3f, 0.0001f);

	Settings.Mode = EHitscanDamageRoundingMode::Ceil;
	TestNearly(*this, TEXT("Ceil"), HitscanBallistics::RoundDamage(12.31f, Settings), 12.4f, 0.0001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanBoneRuleHeadshotWithoutTagTest,
	"FPS.Combat.Hitscan.HitZone.BoneRuleHeadshotWithoutTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanBoneRuleHeadshotWithoutTagTest::RunTest(const FString& Parameters)
{
	FHitscanHitZoneRule HeadRule;
	HeadRule.BoneNames = { TEXT("head") };
	HeadRule.Priority = 100;
	HeadRule.bCountsAsHeadshot = true;

	const TArray<FHitscanHitZoneRule> Rules = { HeadRule };
	const FHitscanResolvedHitZoneInfo Result = UHitscanHitZoneDefinition::ResolveRules(
		Rules,
		TEXT("head"),
		FGameplayTag());

	TestFalse(TEXT("Tag may remain empty when no tag is configured"), Result.HitZoneTag.IsValid());
	TestEqual(TEXT("Priority survives bone-only rule"), Result.Priority, 100);
	TestTrue(TEXT("Headshot survives bone-only rule"), Result.bHeadshot);
	return true;
}


#endif
