#include "Camera/ShooterMovementCameraShake.h"

#include "Camera/PlayerCameraManager.h"

void UShooterLocalMovementCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
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

bool UShooterLocalMovementCameraShakePattern::IsFinishedImpl() const
{
	return !ShakeState.IsPlaying() || HasViewTargetChanged();
}

void UShooterLocalMovementCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	ShakeState.Stop(Params.bImmediately);
}

void UShooterLocalMovementCameraShakePattern::TeardownShakePatternImpl()
{
	ShakeState.Stop(true);
	StartingCameraManager.Reset();
	StartingViewTarget.Reset();
	bTrackViewTarget = false;
}

bool UShooterLocalMovementCameraShakePattern::HasViewTargetChanged() const
{
	if (!bTrackViewTarget)
	{
		return false;
	}

	const APlayerCameraManager* CameraManager = StartingCameraManager.Get();
	return !CameraManager || !StartingViewTarget.IsValid()
		|| CameraManager->GetViewTarget() != StartingViewTarget.Get();
}

void UShooterMovementCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(FMath::Max(Duration, 0.0f), ECameraShakeDurationType::Fixed);
	OutInfo.BlendIn = 0.0f;
	OutInfo.BlendOut = 0.0f;
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

void UShooterSprintCameraShakePattern::SetMovementState(float Speed, float Intensity, float StepDistance)
{
	TargetSpeed = FMath::Max(0.f, Speed);
	TargetIntensity = FMath::Max(0.f, Intensity);
	CurrentStepDistance = FMath::Max(1.f, StepDistance);
}

void UShooterSprintCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration::Infinite();
	// The target envelope handles entry, exit, and resuming during a fade.
	OutInfo.BlendIn = 0.f;
	OutInfo.BlendOut = 0.f;
}

void UShooterSprintCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Super::StartShakePatternImpl(Params);
	if (!Params.bIsRestarting)
	{
		Phase = 0.f;
		TargetSpeed = 0.f;
		TargetIntensity = 0.f;
		CurrentIntensity = 0.f;
		CurrentStepDistance = 1.f;
	}
}

void UShooterSprintCameraShakePattern::UpdateShakePatternImpl(
	const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	if (HasViewTargetChanged())
	{
		ShakeState.Stop(true);
		return;
	}

	const float DeltaTime = FMath::Max(0.f, Params.DeltaTime);
	const float BlendWeight = ShakeState.Update(DeltaTime);
	if (!ShakeState.IsPlaying())
	{
		return;
	}

	const float ResponseTime = TargetIntensity > CurrentIntensity ? EnterResponseTime : ExitResponseTime;
	const float ResponseAlpha = ResponseTime > UE_SMALL_NUMBER
		? 1.f - FMath::Exp(-DeltaTime / ResponseTime)
		: 1.f;
	CurrentIntensity = FMath::Lerp(CurrentIntensity, TargetIntensity, ResponseAlpha);
	if (TargetIntensity <= 0.f && CurrentIntensity <= UE_KINDA_SMALL_NUMBER)
	{
		CurrentIntensity = 0.f;
		ShakeState.Stop(true);
		return;
	}

	// A left/right cycle spans two steps; vertical motion repeats on each step.
	Phase = FMath::Fmod(Phase + PI * TargetSpeed * DeltaTime / CurrentStepDistance, 2.f * PI);
	const float LateralWave = FMath::Sin(Phase);
	OutResult.Location.Y = LateralOffset * LateralWave;
	OutResult.Location.Z = -VerticalOffset * 0.5f * (1.f - FMath::Cos(2.f * Phase));
	OutResult.Rotation.Roll = -RollOffset * LateralWave;
	OutResult.ApplyScale(CurrentIntensity * BlendWeight);
}

void UShooterSprintCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	TargetSpeed = 0.f;
	TargetIntensity = 0.f;
	if (Params.bImmediately)
	{
		CurrentIntensity = 0.f;
		Super::StopShakePatternImpl(Params);
	}
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

UShooterSprintCameraShake::UShooterSprintCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UShooterSprintCameraShakePattern>(TEXT("MovementPattern")));
}

void UShooterSprintCameraShake::UpdateMovement(float Speed, float Intensity, float StepDistance)
{
	if (UShooterSprintCameraShakePattern* Pattern = Cast<UShooterSprintCameraShakePattern>(GetRootShakePattern()))
	{
		Pattern->SetMovementState(Speed, Intensity, StepDistance);
	}
}
