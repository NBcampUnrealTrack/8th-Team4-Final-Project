#include "Training/TrainingTypes.h"

bool FTrainingDistanceBand::Contains(const FVector& Center, const FVector& Position) const
{
    const double Radius = FVector::Dist2D(Center, Position);
    return FMath::IsFinite(Radius)
        && Radius >= MinCentimeters - KINDA_SMALL_NUMBER
        && Radius <= MaxCentimeters + KINDA_SMALL_NUMBER;
}

bool FTrainingDistanceBand::ContainsSegment(const FVector& Center,
    const FVector& Start, const FVector& End) const
{
    if (!Contains(Center, Start) || !Contains(Center, End))
    {
        return false;
    }

    // Checking endpoints alone would accept a NavMesh path cutting through the inner circle.
    const FVector2D A(Start.X - Center.X, Start.Y - Center.Y);
    const FVector2D B(End.X - Center.X, End.Y - Center.Y);
    const FVector2D Segment = B - A;
    const double LengthSquared = Segment.SizeSquared();
    const double Alpha = LengthSquared > SMALL_NUMBER
        ? FMath::Clamp(-FVector2D::DotProduct(A, Segment) / LengthSquared, 0.0, 1.0)
        : 0.0;
    const double ClosestRadius = (A + Segment * Alpha).Size();
    return ClosestRadius >= MinCentimeters - KINDA_SMALL_NUMBER;
}

bool TrainingRules::ResolveDistanceBand(const ETrainingDistancePreset Preset,
    const float CustomMeters, FTrainingDistanceBand& OutBand)
{
    float NominalMeters = 0.0f;
    switch (Preset)
    {
    case ETrainingDistancePreset::TenMeters: NominalMeters = 10.0f; break;
    case ETrainingDistancePreset::TwentyMeters: NominalMeters = 20.0f; break;
    case ETrainingDistancePreset::ThirtyMeters: NominalMeters = 30.0f; break;
    case ETrainingDistancePreset::Custom:
        if (!FMath::IsFinite(CustomMeters))
        {
            return false;
        }
        NominalMeters = FMath::Clamp(CustomMeters, 5.0f, 50.0f);
        break;
    default:
        return false;
    }

    // Prototype choice: the effective band is also bounded by 5m..50m.
    OutBand.MinCentimeters = FMath::Max(5.0f, NominalMeters - 2.0f) * 100.0f;
    OutBand.MaxCentimeters = FMath::Min(50.0f, NominalMeters + 2.0f) * 100.0f;
    return true;
}

bool TrainingRules::CanSpawnCombatAI(const int32 LivingCount)
{
    return LivingCount >= 0 && LivingCount < MaxLivingCombatAI;
}
