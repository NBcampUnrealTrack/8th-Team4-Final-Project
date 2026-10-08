#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "GAS/CueNotify/RogueMontageCueNotify.h"
#include "GameMode/Types/DominationTypes.h"
#include "RogueDashCueNotify.generated.h"

class APawn;

USTRUCT(BlueprintType)
struct FPS_API FDashClassMontages
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
	TObjectPtr<UAnimMontage> Primary;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
	TObjectPtr<UAnimMontage> Pistol;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
	TObjectPtr<UAnimMontage> Knife;
};

// Dash field-of-view kick: widens quickly, then eases back. Added on top of the camera's own
// field of view (aiming included) by the player camera manager.
UCLASS()
class FPS_API URogueDashShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()
public:
	// Extra field of view (degrees) at the peak.
	float FieldOfViewKick = 8.f;
	float KickInTime = .06f;
	float Duration = .35f;

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

	float Elapsed = 0.f;
};

UCLASS()
class FPS_API URogueDashCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	URogueDashCameraShake(const FObjectInitializer& ObjectInitializer);
};

// Cosmetic selection only: distance, duration, cooldown and prediction remain in the ability.
UCLASS()
class FPS_API URogueDashCueNotify : public URogueMontageCueNotify
{
	GENERATED_BODY()
public:
	// Hard asset references keep all nine class/grip variants available in cooked builds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash")
	TMap<EDominationCombatClass, FDashClassMontages> ClassMontages;

	// Speed feedback for the dashing player's own view: a field-of-view kick and speed lines at the screen edges.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash|Feedback")
	bool bLocalSpeedFeedback = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash|Feedback", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float FieldOfViewKick = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash|Feedback", meta = (ClampMin = "0.05"))
	float FeedbackDuration = .35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash|Feedback", meta = (ClampMin = "0", ClampMax = "200"))
	int32 SpeedLineCount = 48;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash|Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedLineOpacity = .35f;

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
	virtual UAnimMontage* ResolveMontage3P(AActor* Target) const override;

private:
	void PlayLocalSpeedFeedback(APawn* Pawn) const;
};
