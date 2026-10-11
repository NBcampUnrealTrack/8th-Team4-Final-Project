#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "ShooterMovementCameraShake.generated.h"

class AActor;
class APlayerCameraManager;

/** Short body compression followed by a smaller recovery, without random rotation. */
UCLASS(EditInlineNew, meta = (AutoExpandCategories = "Movement"))
class FPS_API UShooterMovementCameraShakePattern : public UCameraShakePattern
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
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params,
		FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

private:
	bool HasViewTargetChanged() const;

	FCameraShakeState ShakeState;
	TWeakObjectPtr<APlayerCameraManager> StartingCameraManager;
	TWeakObjectPtr<AActor> StartingViewTarget;
	bool bTrackViewTarget = false;
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
