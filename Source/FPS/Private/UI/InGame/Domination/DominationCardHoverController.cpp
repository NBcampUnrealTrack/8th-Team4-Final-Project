// DominationCardHoverController.cpp
#include "UI/InGame/Domination/DominationCardHoverController.h"

void FDominationCardHoverController::Reset(const int32 InNumCards)
{
	HoveredIndex = INDEX_NONE;

	CurrentStates.Reset();
	CurrentStates.SetNum(FMath::Max(InNumCards, 0));
	TargetStates = CurrentStates;
}

void FDominationCardHoverController::SetHovered(const int32 InIndex)
{
	HoveredIndex = CurrentStates.IsValidIndex(InIndex) ? InIndex : INDEX_NONE;
}

void FDominationCardHoverController::ComputeTargets(
	const FDominationCardHoverSettings& Settings)
{
	for (int32 Index = 0; Index < TargetStates.Num(); ++Index)
	{
		FDominationCardHoverState& Target = TargetStates[Index];

		if (HoveredIndex == INDEX_NONE)
		{
			Target = FDominationCardHoverState();
			continue;
		}

		if (Index == HoveredIndex)
		{
			Target.Translation = FVector2D(0.0f, Settings.FocusLift);
			Target.Scale = Settings.FocusScale;
			Target.Opacity = 1.0f;
			continue;
		}

		// hover된 카드 기준으로 바깥쪽으로 밀린다.
		const float PushDirection = Index < HoveredIndex ? -1.0f : 1.0f;
		Target.Translation = FVector2D(Settings.SidePush * PushDirection, 0.0f);
		Target.Scale = Settings.SideScale;
		Target.Opacity = Settings.SideOpacity;
	}
}

bool FDominationCardHoverController::Tick(
	const float DeltaTime,
	const FDominationCardHoverSettings& Settings)
{
	if (CurrentStates.IsEmpty())
	{
		return false;
	}

	ComputeTargets(Settings);

	bool bChanged = false;

	for (int32 Index = 0; Index < CurrentStates.Num(); ++Index)
	{
		FDominationCardHoverState& Current = CurrentStates[Index];
		const FDominationCardHoverState& Target = TargetStates[Index];

		const bool bAtTarget =
			Current.Translation.Equals(Target.Translation, KINDA_SMALL_NUMBER)
			&& FMath::IsNearlyEqual(Current.Scale, Target.Scale, KINDA_SMALL_NUMBER)
			&& FMath::IsNearlyEqual(Current.Opacity, Target.Opacity, KINDA_SMALL_NUMBER);

		if (bAtTarget)
		{
			// 목표에 도달했으면 스냅해 두고 다음 틱부터는 이 카드를 건너뛴다.
			Current = Target;
			continue;
		}

		Current.Translation = FMath::Vector2DInterpTo(
			Current.Translation, Target.Translation, DeltaTime, Settings.InterpSpeed);
		Current.Scale = FMath::FInterpTo(
			Current.Scale, Target.Scale, DeltaTime, Settings.InterpSpeed);
		Current.Opacity = FMath::FInterpTo(
			Current.Opacity, Target.Opacity, DeltaTime, Settings.InterpSpeed);

		bChanged = true;
	}

	return bChanged;
}

const TArray<FDominationCardHoverState>& FDominationCardHoverController::GetStates() const
{
	return CurrentStates;
}

int32 FDominationCardHoverController::GetHoveredIndex() const
{
	return HoveredIndex;
}
