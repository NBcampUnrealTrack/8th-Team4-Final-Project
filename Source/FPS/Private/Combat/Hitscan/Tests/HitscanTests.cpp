#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/CombatHitZoneDefinition.h"
#include "Combat/Hitscan/HitscanBallistics.h"
#include "Combat/Hitscan/HitscanShotTypes.h"
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
	FHitscanPenetrationRetentionTest,
	"FPS.Combat.Hitscan.Ballistics.PenetrationRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanPenetrationRetentionTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationState State;
	State.RemainingPenetrationPower = 30.0f;
	State.AccumulatedWallRetention = 1.0f;

	FHitscanSurfaceRule Surface;
	Surface.Hardness = 5.0f;
	Surface.PenetrationCostPerCentimeter = 1.0f;
	Surface.DamageRetentionPerCentimeter = 0.9f;

	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 2;

	const FHitscanPenetrationStepResult Result = HitscanBallistics::ResolvePenetration(
		State,
		10.0f,
		Surface,
		Settings);

	TestTrue(TEXT("Surface penetrated"), Result.bPenetrated);
	TestNearly(*this, TEXT("Penetration cost"), Result.PenetrationCost, 15.0f);
	TestNearly(*this, TEXT("Remaining power"), Result.ResultingState.RemainingPenetrationPower, 15.0f);
	TestNearly(
		*this,
		TEXT("Wall retention is preserved"),
		Result.ResultingState.AccumulatedWallRetention,
		FMath::Pow(0.9f, 10.0f));
	TestEqual(TEXT("Penetration count"), Result.ResultingState.PenetrationCount, 1);
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
	State.AccumulatedWallRetention = 1.0f;

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

	const float ExpectedRetention = FMath::Pow(0.98f, 10.0f) * FMath::Pow(0.95f, 5.0f);
	TestEqual(TEXT("Penetration count"), Second.ResultingState.PenetrationCount, 2);
	TestNearly(*this, TEXT("Remaining penetration power"), Second.ResultingState.RemainingPenetrationPower, 40.0f);
	TestNearly(*this, TEXT("Accumulated wall retention"), Second.ResultingState.AccumulatedWallRetention, ExpectedRetention);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanPositiveRetentionIsNotThresholdedTest,
	"FPS.Combat.Hitscan.Ballistics.PositiveRetentionIsNotThresholded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanPositiveRetentionIsNotThresholdedTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationState State;
	State.RemainingPenetrationPower = 100.0f;
	State.AccumulatedWallRetention = 1.0f;

	FHitscanSurfaceRule Surface;
	Surface.DamageRetentionPerCentimeter = 0.1f;

	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 1;

	const FHitscanPenetrationStepResult Result = HitscanBallistics::ResolvePenetration(
		State,
		6.0f,
		Surface,
		Settings);

	TestTrue(TEXT("Any finite positive retention is passed through"), Result.bPenetrated);
	TestTrue(TEXT("Retention remains positive"), Result.ResultingState.AccumulatedWallRetention > 0.0f);
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
	State.AccumulatedWallRetention = 1.0f;

	const FHitscanPenetrationStepResult Result = HitscanBallistics::ResolvePenetration(
		State, 20.0f, Surface, Settings);

	TestFalse(TEXT("Insufficient power stops penetration"), Result.bPenetrated);
	TestNearly(*this, TEXT("Power is unchanged on failure"), Result.ResultingState.RemainingPenetrationPower, 50.0f);
	TestEqual(TEXT("Count is unchanged on failure"), Result.ResultingState.PenetrationCount, 0);
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
	FCombatHitZoneRule Torso;
	Torso.HitZoneTag = RogueTags::Hit_Zone_Torso;
	Torso.BoneNames = { TEXT("spine_03") };
	Torso.Priority = 50;

	FCombatHitZoneRule LowPriorityHead;
	LowPriorityHead.HitZoneTag = RogueTags::Hit_Zone_Torso;
	LowPriorityHead.BoneNames = { TEXT("head") };
	LowPriorityHead.Priority = 10;

	FCombatHitZoneRule InvalidHighPriorityRule;
	InvalidHighPriorityRule.BoneNames = { TEXT("head") };
	InvalidHighPriorityRule.Priority = 200;

	FCombatHitZoneRule Head;
	Head.HitZoneTag = RogueTags::Hit_Zone_Head;
	Head.BoneNames = { TEXT("head") };
	Head.Priority = 100;

	const TArray<FCombatHitZoneRule> Rules = { Torso, LowPriorityHead, InvalidHighPriorityRule, Head };
	const FGameplayTag ResolvedHead = UCombatHitZoneDefinition::ResolveRules(
		Rules,
		TEXT("head"),
		RogueTags::Hit_Zone_Torso);
	const FGameplayTag Fallback = UCombatHitZoneDefinition::ResolveRules(
		Rules,
		TEXT("missing_bone"),
		RogueTags::Hit_Zone_Torso);

	TestTrue(TEXT("Highest-priority valid rule wins"), ResolvedHead.MatchesTagExact(RogueTags::Hit_Zone_Head));
	TestTrue(TEXT("Unknown bones use the fallback tag"), Fallback.MatchesTagExact(RogueTags::Hit_Zone_Torso));
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
		HitscanBallistics::IsBackstabDirection(Facing, FVector(1.0f, 0.0f, 5.0f), 45.0f));
	TestFalse(
		TEXT("Front attack is not a backstab"),
		HitscanBallistics::IsBackstabDirection(Facing, FVector(-1.0f, 0.0f, 0.0f), 45.0f));
	TestTrue(
		TEXT("Rear cone boundary is accepted"),
		HitscanBallistics::IsBackstabDirection(Facing, FVector(1.0f, 1.0f, 0.0f), 45.0f));
	return true;
}

#endif
