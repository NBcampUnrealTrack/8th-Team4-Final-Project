#include "Camera/ShooterMovementCameraShake.h"

#include "Camera/PlayerCameraManager.h"

void UShooterMovementCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(FMath::Max(Duration, 0.0f), ECameraShakeDurationType::Fixed);
	OutInfo.BlendIn = 0.0f;
	OutInfo.BlendOut = 0.0f;
}

void UShooterMovementCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	ShakeState.Start(this, Params);
	StartingCameraManager.Reset();
	StartingViewTarget.Reset();
	bTrackViewTarget = false;

	if (UCameraShakeBase* Shake = GetShakeInstance())
	{
		if (APlayerCameraManager* CameraManager = Shake->GetCameraManager())
		{
			StartingCameraManager = CameraManager;
			StartingViewTarget = CameraManager->GetViewTarget();
			bTrackViewTarget = true;
		}
	}
}

void UShooterMovementCameraShakePattern::UpdateShakePatternImpl(
	const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	if (HasViewTargetChanged())
	{
		ShakeState.Stop(true);
		return;
	}

	const float BlendWeight = ShakeState.Update(Params.DeltaTime);
	const float ActiveDuration = ShakeState.GetDuration();
	if (!ShakeState.IsPlaying() || ActiveDuration <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float TimeAlpha = FMath::Clamp(ShakeState.GetElapsedTime() / ActiveDuration, 0.0f, 1.0f);
	const float CompressionEnd = FMath::Clamp(CompressionFraction, 0.05f, 0.95f);
	const bool bCompressing = TimeAlpha <= CompressionEnd;
	const float LobeAlpha = bCompressing
		? TimeAlpha / CompressionEnd
		: (TimeAlpha - CompressionEnd) / (1.0f - CompressionEnd);
	const float Lobe = FMath::Square(FMath::Sin(PI * LobeAlpha));
	const float OffsetWeight = Lobe * (bCompressing ? 1.0f : -FMath::Clamp(ReboundRatio, 0.0f, 1.0f));

	OutResult.Location.Z = VerticalOffset * OffsetWeight;
	OutResult.Rotation.Pitch = PitchOffset * OffsetWeight;
	// The engine applies the shake's base/dynamic scale and play space afterward.
	OutResult.ApplyScale(BlendWeight);
}

bool UShooterMovementCameraShakePattern::IsFinishedImpl() const
{
	return !ShakeState.IsPlaying() || HasViewTargetChanged();
}

void UShooterMovementCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	ShakeState.Stop(Params.bImmediately);
}

void UShooterMovementCameraShakePattern::TeardownShakePatternImpl()
{
	ShakeState.Stop(true);
	StartingCameraManager.Reset();
	StartingViewTarget.Reset();
	bTrackViewTarget = false;
}

bool UShooterMovementCameraShakePattern::HasViewTargetChanged() const
{
	if (!bTrackViewTarget)
	{
		return false;
	}

	const APlayerCameraManager* CameraManager = StartingCameraManager.Get();
	return !CameraManager || !StartingViewTarget.IsValid()
		|| CameraManager->GetViewTarget() != StartingViewTarget.Get();
}

UShooterJumpCameraShake::UShooterJumpCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UShooterMovementCameraShakePattern>(TEXT("MovementPattern")));
}

UShooterLandingCameraShake::UShooterLandingCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	UShooterMovementCameraShakePattern* Pattern =
		CreateDefaultSubobject<UShooterMovementCameraShakePattern>(TEXT("MovementPattern"));
	Pattern->Duration = 0.28f;
	Pattern->VerticalOffset = -2.2f;
	Pattern->PitchOffset = -0.6f;
	SetRootShakePattern(Pattern);
}
