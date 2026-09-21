#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Customization/CustomizationPreviewGrounding.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationPreviewGroundingTest,
    "FPS.Customization.MutableLevel.Grounding.Geometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCustomizationPreviewGroundingTest::RunTest(const FString&)
{
    // A bound/pivot-based correction would leave these distinct authored soles apart.
    for (const double SoleZ : { -3.0, 0.75, 6.0 })
    {
        const TArray<FVector> Vertices = { FVector(-10, 0, SoleZ), FVector(10, 0, SoleZ), FVector(0, 0, 180) };
        const FTransform Original(FRotator(0, 73, 0), FVector(12, 19, 8), FVector(1.2, 1.2, 1.2));
        FTransform Grounded;
        TestTrue(TEXT("Valid geometry resolves"), CalculatePreviewGroundedTransform(Vertices, Original, 2.0, Grounded));
        TestTrue(TEXT("Actual sole reaches floor"), FMath::IsNearlyEqual(Grounded.TransformPosition(Vertices[0]).Z, 2.0));
        TestTrue(TEXT("Rotation retained"), Grounded.GetRotation().Equals(Original.GetRotation()));
        TestTrue(TEXT("Scale retained"), Grounded.GetScale3D().Equals(Original.GetScale3D()));
        TestEqual(TEXT("Horizontal translation retained"), Grounded.GetTranslation().X, 12.0);
        TestEqual(TEXT("Horizontal translation retained Y"), Grounded.GetTranslation().Y, 19.0);
        FTransform Repeated;
        TestTrue(TEXT("Regeneration resolves"), CalculatePreviewGroundedTransform(Vertices, Grounded, 2.0, Repeated));
        TestTrue(TEXT("Regeneration does not accumulate correction"), Repeated.Equals(Grounded));
    }
    FTransform Unchanged;
    const FTransform Initial(FVector(1, 2, 3));
    TestFalse(TEXT("Unavailable geometry rejected"), CalculatePreviewGroundedTransform({}, Initial, 0, Unchanged));
    TestTrue(TEXT("Unavailable geometry preserves previous transform"), Unchanged.Equals(Initial));
    return true;
}
#endif
