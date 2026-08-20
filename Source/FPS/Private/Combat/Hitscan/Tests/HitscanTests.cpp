#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Combat/CombatHitZoneDefinition.h"
#include "Combat/Hitscan/Helpers/HitscanMath.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "Tags/GameplayTags.h"

static void TestNearly(
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanPenetrationRetentionTest,
	"FPS.Combat.Hitscan.Math.PenetrationRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanPenetrationRetentionTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationState State;
	State.AccumulatedWallRetention = 1.0f;

	FHitscanSurfaceRule Surface;
	Surface.bCanPenetrate = true;
	Surface.DamageMultiplier = 0.65f;

	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 2;

	const FHitscanPenetrationStepResult Result = FHitscanMath::ResolvePenetration(
		State,
		Surface,
		Settings);

	TestTrue(TEXT("Surface penetrated"), Result.bPenetrated);
	TestNearly(*this, TEXT("Wall multiplier is applied"), Result.ResultingState.AccumulatedWallRetention, 0.65f);
	TestEqual(TEXT("Penetration count"), Result.ResultingState.PenetrationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanSequentialPenetrationTest,
	"FPS.Combat.Hitscan.Math.SequentialPenetration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanSequentialPenetrationTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 3;

	FHitscanSurfaceRule FirstSurface;
	FirstSurface.bCanPenetrate = true;
	FirstSurface.DamageMultiplier = 0.8f;

	FHitscanSurfaceRule SecondSurface;
	SecondSurface.bCanPenetrate = true;
	SecondSurface.DamageMultiplier = 0.5f;

	FHitscanPenetrationState State;
	State.AccumulatedWallRetention = 1.0f;

	const FHitscanPenetrationStepResult First = FHitscanMath::ResolvePenetration(
		State, FirstSurface, Settings);
	if (!TestTrue(TEXT("First surface penetrated"), First.bPenetrated))
	{
		return false;
	}

	const FHitscanPenetrationStepResult Second = FHitscanMath::ResolvePenetration(
		First.ResultingState, SecondSurface, Settings);
	if (!TestTrue(TEXT("Second surface penetrated"), Second.bPenetrated))
	{
		return false;
	}

	TestEqual(TEXT("Penetration count"), Second.ResultingState.PenetrationCount, 2);
	TestNearly(*this, TEXT("Accumulated wall multiplier"), Second.ResultingState.AccumulatedWallRetention, 0.4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanNonPenetrableSurfaceStopsTest,
	"FPS.Combat.Hitscan.Math.NonPenetrableSurfaceStops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanNonPenetrableSurfaceStopsTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationState State;
	State.AccumulatedWallRetention = 1.0f;

	FHitscanSurfaceRule Surface;
	Surface.bCanPenetrate = false;
	Surface.DamageMultiplier = 0.5f;

	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 1;

	const FHitscanPenetrationStepResult Result = FHitscanMath::ResolvePenetration(
		State,
		Surface,
		Settings);

	TestFalse(TEXT("Non-penetrable surfaces stop the shot"), Result.bPenetrated);
	TestNearly(*this, TEXT("Wall multiplier is unchanged"), Result.ResultingState.AccumulatedWallRetention, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHitscanMaximumPenetrationCountTest,
	"FPS.Combat.Hitscan.Math.MaximumPenetrationCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanMaximumPenetrationCountTest::RunTest(const FString& Parameters)
{
	FHitscanPenetrationSettings Settings;
	Settings.MaximumPenetrationCount = 1;

	FHitscanSurfaceRule Surface;
	Surface.bCanPenetrate = true;
	Surface.DamageMultiplier = 0.5f;

	FHitscanPenetrationState State;
	State.AccumulatedWallRetention = 1.0f;
	State.PenetrationCount = 1;

	const FHitscanPenetrationStepResult Result = FHitscanMath::ResolvePenetration(State, Surface, Settings);

	TestFalse(TEXT("Maximum penetration count stops the shot"), Result.bPenetrated);
	TestEqual(TEXT("Count is unchanged on failure"), Result.ResultingState.PenetrationCount, 1);
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
		FHitscanMath::CalculateRayExitDistanceFromBox(
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
	FHitscanBackstabHorizontalProjectionTest,
	"FPS.Combat.Hitscan.Math.BackstabHorizontalProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitscanBackstabHorizontalProjectionTest::RunTest(const FString& Parameters)
{
	const FVector Facing = FVector::ForwardVector;

	TestTrue(
		TEXT("Vertical component does not affect rear alignment"),
		FHitscanMath::IsBackstabDirection(Facing, FVector(1.0f, 0.0f, 5.0f), 45.0f));
	TestFalse(
		TEXT("Front attack is not a backstab"),
		FHitscanMath::IsBackstabDirection(Facing, FVector(-1.0f, 0.0f, 0.0f), 45.0f));
	TestTrue(
		TEXT("Rear cone boundary is accepted"),
		FHitscanMath::IsBackstabDirection(Facing, FVector(1.0f, 1.0f, 0.0f), 45.0f));
	return true;
}

#endif
