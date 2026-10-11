#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "ShooterMovementCameraShake.generated.h"

class AActor;
class APlayerCameraManager;

/** Shared lifecycle for movement feedback attached to one local view target. */
UCLASS(Abstract, EditInlineNew)
class FPS_API UShooterLocalMovementCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

protected:
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

	bool HasViewTargetChanged() const;
	FCameraShakeState ShakeState;

private:
	TWeakObjectPtr<APlayerCameraManager> StartingCameraManager;
	TWeakObjectPtr<AActor> StartingViewTarget;
	bool bTrackViewTarget = false;
};

/** Short body compression followed by a smaller recovery, without random rotation. */
UCLASS(EditInlineNew, meta = (AutoExpandCategories = "Movement"))
class FPS_API UShooterMovementCameraShakePattern : public UShooterLocalMovementCameraShakePattern
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (Units = "cm"))
	float VerticalOffset = -0.7f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (Units = "deg"))
	float PitchOffset = -0.2f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float CompressionFraction = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReboundRatio = 0.15f;

protected:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,
		FCameraShakePatternUpdateResult& OutResult) override;
};

/** Continuous running sway; one lateral cycle spans two footsteps. */
UCLASS(EditInlineNew, meta = (AutoExpandCategories = "Movement"))
class FPS_API UShooterSprintCameraShakePattern : public UShooterLocalMovementCameraShakePattern
{
	GENERATED_BODY()

public:
	void SetMovementState(float Speed, float Intensity, float StepDistance);

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm"))
	float LateralOffset = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm"))
	float VerticalOffset = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", Units = "deg"))
	float RollOffset = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", Units = "s"))
	float EnterResponseTime = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", Units = "s"))
	float ExitResponseTime = 0.08f;

protected:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,
		FCameraShakePatternUpdateResult& OutResult) override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;

private:
	float Phase = 0.f;
	float TargetSpeed = 0.f;
	float TargetIntensity = 0.f;
	float CurrentIntensity = 0.f;
	float CurrentStepDistance = 1.f;
};

UCLASS(Blueprintable)
class FPS_API UShooterJumpCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UShooterJumpCameraShake(const FObjectInitializer& ObjectInitializer);
};

UCLASS(Blueprintable)
class FPS_API UShooterLandingCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UShooterLandingCameraShake(const FObjectInitializer& ObjectInitializer);
};

UCLASS(Blueprintable)
class FPS_API UShooterSprintCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UShooterSprintCameraShake(const FObjectInitializer& ObjectInitializer);

	void UpdateMovement(float Speed, float Intensity, float StepDistance);
};
