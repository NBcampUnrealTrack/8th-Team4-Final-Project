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

    OutBand.MinCentimeters = FMath::Max(5.0f, NominalMeters - 2.0f) * 100.0f;
    OutBand.MaxCentimeters = FMath::Min(50.0f, NominalMeters + 2.0f) * 100.0f;
    return true;
}

bool TrainingRules::CanSpawnCombatAI(const int32 LivingCount)
{
    return LivingCount >= 0 && LivingCount < MaxLivingCombatAI;
}

bool TrainingRules::IsRunningState(const ETrainingSessionState State)
{
    return State == ETrainingSessionState::TargetAcquisitionActive
        || State == ETrainingSessionState::CombatResponseActive;
}

float TrainingRules::FeedbackOpacity(const float Elapsed, const float Duration,
    const float FadeIn, const float FadeOut)
{
    if (Duration <= 0.0f || Elapsed < 0.0f || Elapsed >= Duration)
    {
        return 0.0f;
    }
    const float InLength = FMath::Clamp(FadeIn, 0.0f, Duration * 0.5f);
    const float OutLength = FMath::Clamp(FadeOut, 0.0f, Duration * 0.5f);
    const float InAlpha = InLength > KINDA_SMALL_NUMBER
        ? FMath::Clamp(Elapsed / InLength, 0.0f, 1.0f) : 1.0f;
    const float OutAlpha = OutLength > KINDA_SMALL_NUMBER
        ? FMath::Clamp((Duration - Elapsed) / OutLength, 0.0f, 1.0f) : 1.0f;
    const float LinearAlpha = FMath::Min(InAlpha, OutAlpha);
    return LinearAlpha * LinearAlpha * (3.0f - 2.0f * LinearAlpha);
}

void FTrainingSessionStats::RecordShot(const bool bHitTrainingBot)
{
    ++ShotsFired;
    if (bHitTrainingBot) { ++ShotsHit; }
}

float FTrainingSessionStats::GetAccuracyPercent() const
{
    return ShotsFired > 0
        ? 100.0f * static_cast<float>(FMath::Clamp(ShotsHit, 0, ShotsFired)) / ShotsFired : 0.0f;
}

int32 TrainingRules::RemainingWholeSeconds(const float Duration, const float Elapsed)
{
    return FMath::CeilToInt(FMath::Max(0.0f, Duration - FMath::Max(0.0f, Elapsed)));
}
