#include "Training/AI/TrainingBotPersonalityTypes.h"

void FTrainingVelocityHistory::Reset()
{
    NextIndex = 0;
    SampleCount = 0;
}

void FTrainingVelocityHistory::Add(const float TimeSeconds, const FVector& Velocity)
{
    if (!FMath::IsFinite(TimeSeconds) || Velocity.ContainsNaN())
    {
        return;
    }
    if (SampleCount > 0)
    {
        const int32 PreviousIndex = (NextIndex + Capacity - 1) % Capacity;
        if (TimeSeconds < Samples[PreviousIndex].TimeSeconds)
        {
            Reset();
        }
        else if (TimeSeconds == Samples[PreviousIndex].TimeSeconds)
        {
            Samples[PreviousIndex].Velocity = Velocity;
            return;
        }
    }
    Samples[NextIndex].TimeSeconds = TimeSeconds;
    Samples[NextIndex].Velocity = Velocity;
    NextIndex = (NextIndex + 1) % Capacity;
    SampleCount = FMath::Min(SampleCount + 1, Capacity);
}

bool FTrainingVelocityHistory::TryGetAtOrBefore(const float TimeSeconds, FVector& OutVelocity) const
{
    OutVelocity = FVector::ZeroVector;
    if (!FMath::IsFinite(TimeSeconds))
    {
        return false;
    }
    for (int32 Offset = 0; Offset < SampleCount; ++Offset)
    {
        const int32 Index = (NextIndex + Capacity - 1 - Offset) % Capacity;
        if (Samples[Index].TimeSeconds <= TimeSeconds)
        {
            OutVelocity = Samples[Index].Velocity;
            return true;
        }
    }
    return false;
}

void FTrainingAimLatch::Reset()
{
    bAimedAt = false;
    PendingSeconds = 0.0f;
}

void FTrainingAimLatch::Update(const bool bObserved, const float DeltaSeconds, const float ReactionSeconds)
{
    if (bObserved == bAimedAt)
    {
        PendingSeconds = 0.0f;
        return;
    }
    PendingSeconds += FMath::Max(0.0f, DeltaSeconds);
    if (PendingSeconds >= FMath::Max(0.0f, ReactionSeconds))
    {
        bAimedAt = bObserved;
        PendingSeconds = 0.0f;
    }
}

bool TrainingPersonality::IsConcretePersonality(const ETrainingBotPersonality Personality)
{
    return Personality == ETrainingBotPersonality::Copycat
        || Personality == ETrainingBotPersonality::Coward
        || Personality == ETrainingBotPersonality::Trickster;
}

bool TrainingPersonality::IsCopyAction(const ETrainingBotAction Action)
{
    return Action == ETrainingBotAction::CopyMirror
        || Action == ETrainingBotAction::CopyInverse
        || Action == ETrainingBotAction::CopyDelayed;
}

float TrainingPersonality::TangentialVelocity(const FVector& WorldVelocity,
    const FVector& ArenaCenter, const FVector& BotPosition)
{
    const FVector Radial = (BotPosition - ArenaCenter).GetSafeNormal2D();
    const FVector PositiveTangent(-Radial.Y, Radial.X, 0.0);
    return static_cast<float>(FVector::DotProduct(WorldVelocity, PositiveTangent));
}

bool TrainingPersonality::IsAimNearCapsule(const FVector& Eye, const FVector& ViewDirection,
    const FVector& Center, const float HalfHeight, const float Radius, const float Padding,
    FVector& OutClosestAimPoint)
{
    OutClosestAimPoint = Eye;
    const FVector Direction = ViewDirection.GetSafeNormal();
    if (Eye.ContainsNaN() || Center.ContainsNaN() || Direction.ContainsNaN()
        || Direction.IsNearlyZero() || !FMath::IsFinite(HalfHeight)
        || !FMath::IsFinite(Radius) || !FMath::IsFinite(Padding)
        || Radius <= 0.0f || HalfHeight < Radius
        || FVector::DotProduct(Center - Eye, Direction) <= 0.0)
    {
        return false;
    }

    const double AxisHalfLength = static_cast<double>(HalfHeight - Radius);
    const FVector Bottom = Center - FVector(0.0, 0.0, AxisHalfLength);
    const FVector Top = Center + FVector(0.0, 0.0, AxisHalfLength);
    double BestDistanceSquared = TNumericLimits<double>::Max();
    const auto TestAxisPoint = [&](const FVector& AxisPoint)
    {
        const double RayDistance = FMath::Max(0.0, FVector::DotProduct(AxisPoint - Eye, Direction));
        const FVector AimPoint = Eye + Direction * RayDistance;
        const double DistanceSquared = FVector::DistSquared(AxisPoint, AimPoint);
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            OutClosestAimPoint = AimPoint;
        }
    };
    TestAxisPoint(Bottom);
    TestAxisPoint(Top);
    const FVector Offset = Eye - Bottom;
    const double Denominator = 1.0 - FMath::Square(Direction.Z);
    if (Denominator > 1.e-8)
    {
        const double RayDistance = (Direction.Z * Offset.Z
            - FVector::DotProduct(Direction, Offset)) / Denominator;
        const double AxisDistance = Offset.Z + Direction.Z * RayDistance;
        if (RayDistance >= 0.0 && AxisDistance >= 0.0 && AxisDistance <= 2.0 * AxisHalfLength)
        {
            TestAxisPoint(Bottom + FVector(0.0, 0.0, AxisDistance));
        }
    }
    TestAxisPoint(Bottom + FVector(0.0, 0.0, FMath::Clamp(Offset.Z, 0.0, 2.0 * AxisHalfLength)));
    return BestDistanceSquared <= FMath::Square(static_cast<double>(Radius + FMath::Max(0.0f, Padding)));
}
