#include "Customization/AppearanceGrip.h"
#include "ReferenceSkeleton.h"

namespace
{
bool PalmFrame(const FReferenceSkeleton& Ref, FName Hand, FQuat& Out)
{
    const int32 HandIndex = Ref.FindBoneIndex(Hand);
    if (HandIndex == INDEX_NONE) return false;
    const FString Side = Hand.ToString().EndsWith(TEXT("_r")) ? TEXT("r") : TEXT("l");
    auto Finger = [&](const TCHAR* Name, FVector& Position)
    {
        int32 Index = Ref.FindBoneIndex(FName(*(FString(Name) + Side)));
        FTransform Local = FTransform::Identity;
        while (Index != INDEX_NONE && Index != HandIndex)
        {
            Local = Local * Ref.GetRefBonePose()[Index];
            Index = Ref.GetParentIndex(Index);
        }
        Position = Local.GetTranslation();
        return Index == HandIndex && !Position.ContainsNaN();
    };
    FVector Middle, Index, Pinky;
    if (!Finger(TEXT("middle_01_"), Middle) || !Finger(TEXT("index_01_"), Index) || !Finger(TEXT("pinky_01_"), Pinky)) return false;
    const FVector X = Middle.GetSafeNormal();
    const FVector Y = (Index - Pinky).GetSafeNormal();
    if (X.IsNearlyZero() || FVector::CrossProduct(X, Y).IsNearlyZero()) return false;
    Out = FRotationMatrix::MakeFromXY(X, Y).ToQuat();
    return !Out.ContainsNaN();
}
}

bool AppearanceGrip::BuildHandBasis(const FReferenceSkeleton& Source, const FReferenceSkeleton& Target, FName Hand, FTransform& OutBasis)
{
    OutBasis = FTransform::Identity;
    FQuat SourceFrame, TargetFrame;
    if (!PalmFrame(Source, Hand, SourceFrame) || !PalmFrame(Target, Hand, TargetFrame)) return false;
    OutBasis.SetRotation((TargetFrame * SourceFrame.Inverse()).GetNormalized());
    return true;
}
FTransform AppearanceGrip::MapRelativeHand(const FTransform& SourceRight, const FTransform& SourceLeft,
    const FTransform& TargetRight, const FTransform& RightBasis, const FTransform& LeftBasis)
{
    FTransform Result = SourceLeft.GetRelativeTransform(SourceRight) * (RightBasis * TargetRight);
    Result.SetRotation((Result.GetRotation() * LeftBasis.GetRotation().Inverse()).GetNormalized());
    return Result;
}

bool AppearanceGrip::BuildGripBoneBasis(const FReferenceSkeleton& Source, const FReferenceSkeleton& Target,
    FName Bone, FTransform& OutBasis)
{
    OutBasis = FTransform::Identity;
    FTransform HandBasis;
    if (!BuildHandBasis(Source, Target, TEXT("hand_r"), HandBasis)) return false;
    auto RelativeToHand = [Bone](const FReferenceSkeleton& Ref, FQuat& Rotation)
    {
        int32 Index = Ref.FindBoneIndex(Bone);
        const int32 Hand = Ref.FindBoneIndex(TEXT("hand_r"));
        Rotation = FQuat::Identity;
        if (Index == INDEX_NONE || Hand == INDEX_NONE) return false;
        while (Index != Hand && Index != INDEX_NONE)
        {
            Rotation = Ref.GetRefBonePose()[Index].GetRotation() * Rotation;
            Index = Ref.GetParentIndex(Index);
        }
        return Index == Hand;
    };
    FQuat SourceRotation, TargetRotation;
    if (!RelativeToHand(Source, SourceRotation) || !RelativeToHand(Target, TargetRotation)) return false;
    // Express the palm-axis conversion in the actual socket parent's frame.
    // Keep translation zero: a finger-mounted knife must follow that finger's
    // animated origin, not the old body's wrist or reference-pose position.
    OutBasis.SetRotation((TargetRotation.Inverse() * HandBasis.GetRotation() * SourceRotation).GetNormalized());
    return true;
}
