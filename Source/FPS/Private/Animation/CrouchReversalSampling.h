#pragma once

// Shared by the animation node and its standalone/engine behavior tests.
namespace FPSCrouchReversal
{
    template <typename ValueType>
    ValueType BlendCapturedSample(const ValueType& Snapshot, const ValueType& InitialSample, const ValueType& CurrentSample, float Alpha)
    {
        // Fade only the initial mismatch. Blending the snapshot toward the
        // moving current sample would also scale authored motion, then catch
        // up with a burst of extra velocity even when the match is exact.
        return CurrentSample + (Snapshot - InitialSample) * (1.f - Alpha);
    }

    template <typename ScoreType>
    float FindPoseTime(float Length, ScoreType Score)
    {
        if (Length <= 0.f) return 0.f;
        float BestTime = 0.f;
        auto BestScore = Score(0.f);
        const auto Consider = [&](float Time)
        {
            const auto Error = Score(Time);
            if (Error < BestScore) { BestScore = Error; BestTime = Time; }
        };
        // Search the entire opposite clip: enter/exit timing need not be inverse.
        // Two local refinements avoid visible quantization without sampling every
        // frame every update. This work occurs only when a transition reverses.
        float Step = Length / 32.f;
        for (int Index = 1; Index <= 32; ++Index) Consider(Index * Step);
        for (int Pass = 0; Pass < 2; ++Pass)
        {
            const float Left = BestTime > Step ? BestTime - Step : 0.f;
            const float Right = BestTime + Step < Length ? BestTime + Step : Length;
            Step = (Right - Left) / 8.f;
            for (int Index = 0; Index <= 8; ++Index) Consider(Left + Index * Step);
        }
        return BestTime;
    }

    inline float SmoothUnit(float Value)
    {
        const float Clamped = Value < 0.f ? 0.f : (Value > 1.f ? 1.f : Value);
        return Clamped * Clamped * (3.f - 2.f * Clamped);
    }

    inline float BlendIn(float Elapsed, float Duration)
    {
        if (Duration <= 0.f) return 1.f;
        const float Window = Duration * .25f < .16f ? Duration * .25f : .16f;
        return SmoothUnit(Elapsed / Window);
    }

    inline float BlendOut(float Elapsed, float Duration)
    {
        if (Duration <= 0.f) return 0.f;
        const float Window = Duration * .35f < .22f ? Duration * .35f : .22f;
        return 1.f - SmoothUnit((Elapsed - (Duration - Window)) / Window);
    }
}
