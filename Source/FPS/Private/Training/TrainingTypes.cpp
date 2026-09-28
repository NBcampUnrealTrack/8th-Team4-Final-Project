#include "Training/TrainingTypes.h"

#include "Training/TrainingSettings.h"

bool FTrainingDistanceBand::IsValid() const
{
    return FMath::IsFinite(MinCentimeters) && FMath::IsFinite(MaxCentimeters)
        && MinCentimeters >= 0.0f && MaxCentimeters >= MinCentimeters;
}

bool FTrainingDistanceBand::Contains(const FVector& Center, const FVector& Position) const
{
    if (!IsValid() || Center.ContainsNaN() || Position.ContainsNaN())
    {
        return false;
    }
    const double RadiusSquared = FVector::DistSquared2D(Center, Position);
    const double MinRadius = MinCentimeters - KINDA_SMALL_NUMBER;
    const double MaxRadius = MaxCentimeters + KINDA_SMALL_NUMBER;
    return FMath::IsFinite(RadiusSquared)
        && (MinRadius <= 0.0 || RadiusSquared >= FMath::Square(MinRadius))
        && MaxRadius >= 0.0 && RadiusSquared <= FMath::Square(MaxRadius);
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
    const double MinRadius = MinCentimeters - KINDA_SMALL_NUMBER;
    return MinRadius <= 0.0 || (A + Segment * Alpha).SizeSquared() >= FMath::Square(MinRadius);
}

bool TrainingRules::ResolveDistanceBand(const ETrainingDistancePreset Preset,
    const float CustomMeters, FTrainingDistanceBand& OutBand)
{
    OutBand.MinCentimeters = 0.0f;
    OutBand.MaxCentimeters = -1.0f;
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    if (!FMath::IsFinite(Settings->MinDistanceMeters)
        || !FMath::IsFinite(Settings->MaxDistanceMeters)
        || !FMath::IsFinite(Settings->DistanceHalfWidthMeters))
    {
        return false;
    }
    const float MinDistanceMeters = Settings->GetMinDistanceMeters();
    const float MaxDistanceMeters = Settings->GetMaxDistanceMeters();
    const float DistanceHalfWidthMeters = FMath::Max(0.0f, Settings->DistanceHalfWidthMeters);

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
        NominalMeters = FMath::Clamp(CustomMeters, MinDistanceMeters, MaxDistanceMeters);
        break;
    default:
        return false;
    }

    // 고정 프리셋의 거리를 다른 값으로 바꾸거나 뒤집힌 범위를 전달하지 않는다.
    if (NominalMeters < MinDistanceMeters || NominalMeters > MaxDistanceMeters)
    {
        return false;
    }
    FTrainingDistanceBand ResolvedBand;
    ResolvedBand.MinCentimeters = FMath::Max(MinDistanceMeters, NominalMeters - DistanceHalfWidthMeters) * CentimetersPerMeter;
    ResolvedBand.MaxCentimeters = FMath::Min(MaxDistanceMeters, NominalMeters + DistanceHalfWidthMeters) * CentimetersPerMeter;
    if (!ResolvedBand.IsValid())
    {
        return false;
    }
    OutBand = ResolvedBand;
    return true;
}

bool TrainingRules::IsRunningState(const ETrainingSessionState State)
{
    return State == ETrainingSessionState::Active;
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
