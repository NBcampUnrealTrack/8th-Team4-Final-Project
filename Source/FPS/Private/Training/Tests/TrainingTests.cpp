#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Hitscan/HitscanHitContextBuilder.h"
#include "Tags/GameplayTags.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingTarget.h"
#include "Training/TrainingTypes.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingPresetBandsTest,
    "FPS.Training.Distance.Presets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingPresetBandsTest::RunTest(const FString&)
{
    FTrainingDistanceBand Band;
    TestTrue(TEXT("10m resolves"), TrainingRules::ResolveDistanceBand(ETrainingDistancePreset::TenMeters, 0.0f, Band));
    TestEqual(TEXT("10m min"), Band.MinCentimeters, 800.0f);
    TestEqual(TEXT("10m max"), Band.MaxCentimeters, 1200.0f);
    TestTrue(TEXT("20m resolves"), TrainingRules::ResolveDistanceBand(ETrainingDistancePreset::TwentyMeters, 0.0f, Band));
    TestEqual(TEXT("20m min"), Band.MinCentimeters, 1800.0f);
    TestEqual(TEXT("20m max"), Band.MaxCentimeters, 2200.0f);
    TestTrue(TEXT("30m resolves"), TrainingRules::ResolveDistanceBand(ETrainingDistancePreset::ThirtyMeters, 0.0f, Band));
    TestEqual(TEXT("30m min"), Band.MinCentimeters, 2800.0f);
    TestEqual(TEXT("30m max"), Band.MaxCentimeters, 3200.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingCustomBandTest,
    "FPS.Training.Distance.CustomClamp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingCustomBandTest::RunTest(const FString&)
{
    FTrainingDistanceBand Band;
    TestTrue(TEXT("5m resolves"), TrainingRules::ResolveDistanceBand(ETrainingDistancePreset::Custom, 5.0f, Band));
    TestEqual(TEXT("Effective minimum is 5m"), Band.MinCentimeters, 500.0f);
    TestEqual(TEXT("5m upper bound is 7m"), Band.MaxCentimeters, 700.0f);
    TestTrue(TEXT("50m resolves"), TrainingRules::ResolveDistanceBand(ETrainingDistancePreset::Custom, 50.0f, Band));
    TestEqual(TEXT("50m lower bound is 48m"), Band.MinCentimeters, 4800.0f);
    TestEqual(TEXT("Effective maximum is 50m"), Band.MaxCentimeters, 5000.0f);
    TestTrue(TEXT("Finite out-of-range value clamps"),
        TrainingRules::ResolveDistanceBand(ETrainingDistancePreset::Custom, -100.0f, Band));
    TestEqual(TEXT("Clamped lower bound"), Band.MinCentimeters, 500.0f);
    TestFalse(TEXT("NaN is rejected"), TrainingRules::ResolveDistanceBand(ETrainingDistancePreset::Custom,
        std::numeric_limits<float>::quiet_NaN(), Band));
    TestFalse(TEXT("Invalid enum is rejected"),
        TrainingRules::ResolveDistanceBand(static_cast<ETrainingDistancePreset>(255), 20.0f, Band));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingCenterDistanceTest,
    "FPS.Training.Distance.FixedCenter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingCenterDistanceTest::RunTest(const FString&)
{
    FTrainingDistanceBand Band;
    const FVector Center(1234.0, -5678.0, 90.0);
    const FVector Spawn = Center + FVector(2000.0, 0.0, 120.0);
    TestTrue(TEXT("Distance is horizontal around the fixed center"), Band.Contains(Center, Spawn));
    TestFalse(TEXT("Outside minimum after projection"), Band.Contains(Center, Center + FVector(1799.0, 0.0, 0.0)));
    TestFalse(TEXT("Outside maximum after projection"), Band.Contains(Center, Center + FVector(2201.0, 0.0, 0.0)));
    const FVector PlayerBefore = Center;
    const FVector PlayerAfter = Center + FVector(0.0, 200.0, 0.0);
    TestTrue(TEXT("Player moved in this fixture"), !PlayerBefore.Equals(PlayerAfter));
    TestTrue(TEXT("Player movement is not an input to the arena distance test"), Band.Contains(Center, Spawn));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingNavSegmentTest,
    "FPS.Training.Navigation.AnnularSegments", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingNavSegmentTest::RunTest(const FString&)
{
    FTrainingDistanceBand Band;
    const FVector Center = FVector::ZeroVector;
    const FVector Start(2000.0, 0.0, 0.0);
    TestTrue(TEXT("Short tangential chord remains in the band"),
        Band.ContainsSegment(Center, Start, Start.RotateAngleAxis(5.0f, FVector::UpVector)));
    TestFalse(TEXT("Opposite endpoints cannot cut through the inner circle"),
        Band.ContainsSegment(Center, Start, -Start));
    TestFalse(TEXT("Outside endpoint is rejected"),
        Band.ContainsSegment(Center, Start, FVector(2300.0, 0.0, 0.0)));
    TestTrue(TEXT("Stationary valid segment"), Band.ContainsSegment(Center, Start, Start));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingCapacityTest,
    "FPS.Training.Combat.LivingCapacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingCapacityTest::RunTest(const FString&)
{
    TestTrue(TEXT("0 living permits a spawn"), TrainingRules::CanSpawnCombatAI(0));
    TestTrue(TEXT("9 living permits a spawn"), TrainingRules::CanSpawnCombatAI(9));
    TestFalse(TEXT("10 living rejects a spawn"), TrainingRules::CanSpawnCombatAI(10));
    TestFalse(TEXT("11 living rejects a spawn"), TrainingRules::CanSpawnCombatAI(11));
    TestEqual(TEXT("Target interval"), TrainingRules::TargetIntervalSeconds, 2.0f);
    TestEqual(TEXT("Target deadline"), TrainingRules::TargetDurationSeconds, 60.0f);
    TestEqual(TEXT("Combat interval"), TrainingRules::CombatIntervalSeconds, 5.0f);
    TestEqual(TEXT("Combat health"), TrainingRules::CombatHealth, 100.0f);
    TestEqual(TEXT("Corpse lifetime"), TrainingRules::CorpseLifetimeSeconds, 3.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingPelletAggregationTest,
    "FPS.Training.Regression.PelletsReachEveryActor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingPelletAggregationTest::RunTest(const FString&)
{
    // Distinct read-only actor CDOs are sufficient to exercise the existing pointer-keyed builder.
    // This does not spawn a map or claim to test physics/weapon firing/network damage.
    AActor* Targets[] = {
        GetMutableDefault<ATrainingTarget>(),
        GetMutableDefault<ATrainingCombatAI>(),
        GetMutableDefault<ATrainingArena>()
    };
    TArray<FVector> PelletDirections;
    PelletDirections.Init(FVector::ForwardVector, 12);
    FHitscanShotRequest Request;
    Request.PelletDirections = MakeArrayView(PelletDirections);
    TArray<FHitscanImpactSample> Samples;
    const int32 PelletCounts[] = { 3, 2, 1 };
    int32 NextPellet = 0;
    for (int32 TargetIndex = 0; TargetIndex < 3; ++TargetIndex)
    {
        for (int32 PelletOffset = 0; PelletOffset < PelletCounts[TargetIndex]; ++PelletOffset)
        {
            FHitscanImpactSample& Sample = Samples.AddDefaulted_GetRef();
            Sample.TargetActor = Targets[TargetIndex];
            Sample.PelletIndex = NextPellet++;
            Sample.HitZoneTag = RogueTags::Hit_Zone_Torso;
            Sample.ImpactPoint = FVector(2000.0, TargetIndex * 100.0, 0.0);
        }
    }
    TArray<FRogueHitContext> Contexts;
    FHitscanHitContextBuilder::Build(Request, Samples, Contexts);
    TestEqual(TEXT("Three distinct actors remain after aggregation"), Contexts.Num(), 3);
    for (int32 TargetIndex = 0; TargetIndex < 3; ++TargetIndex)
    {
        const FRogueHitContext* Result = Contexts.FindByPredicate([&Targets, TargetIndex](const FRogueHitContext& Entry)
        {
            return Entry.Target.Get() == Targets[TargetIndex];
        });
        if (TestNotNull(TEXT("Each actor has its own hit context"), Result) && Result->HitGroups.Num() == 1)
        {
            TestEqual(TEXT("Same-actor pellets are aggregated"), Result->HitGroups[0].PelletCount, PelletCounts[TargetIndex]);
        }
        else
        {
            AddError(TEXT("Expected exactly one torso group for each actor."));
        }
    }
    return true;
}

#endif
