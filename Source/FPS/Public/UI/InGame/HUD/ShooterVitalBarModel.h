// ShooterVitalBarModel.h
#pragma once

#include "CoreMinimal.h"
#include "UI/InGame/HUD/ShooterVitalBarLayout.h"
#include "ShooterVitalBarModel.generated.h"

// 바에 넘기는 속성 값
USTRUCT()
struct FShooterVitalValues
{
	GENERATED_BODY()

	FShooterVitalValues() = default;

	FShooterVitalValues(const float InHealth, 
						const float InMaxHealth, 
						const float InShield,
	                    const float InMaxShield) : Health(InHealth)
	                                               , MaxHealth(InMaxHealth)
	                                               , Shield(InShield)
	                                               , MaxShield(InMaxShield)
	{
	}

	UPROPERTY(EditAnywhere, Category = "VitalBar")
	float Health = 0.f;
	UPROPERTY(EditAnywhere, Category = "VitalBar")
	float MaxHealth = 0.f;
	UPROPERTY(EditAnywhere, Category = "VitalBar")
	float Shield = 0.f;
	UPROPERTY(EditAnywhere, Category = "VitalBar")
	float MaxShield = 0.f;
};

// 표시 튜닝값
USTRUCT()
struct FShooterVitalBarSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "VitalBar", meta = (ClampMin = "1.0"))
	float SegmentValue = 10.f;
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar", meta = (ClampMin = "0.0"))
	float HoldSeconds = 0.35f;
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar", meta = (ClampMin = "1.0"))
	float DrainPerSecond = 120.f;
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar", meta = (ClampMin = "0.0"))
	float HealDelaySeconds = 0.05f;
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar", meta = (ClampMin = "1.0"))
	float HealRisePerSecond = 80.f;
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar", meta = (ClampMin = "0.01"))
	float FlashSeconds = 0.12f;
};

struct FShooterVitalBarTickResult
{
	bool bVisualChanged = false;
	bool bNeedsMoreUpdates = false;
};

// 한 자원의 표시 상태
struct FShooterVitalTrack
{
	float Real = 0.f;
	float Trail = 0.f;
	float HoldRemaining = 0.f;
	float HealDelayRemaining = 0.f;
	float Flash = 0.f;
};

// 실제값과 뒤따르는 값을 분리한다
struct FPS_API FShooterVitalBarModel
{
	// 최신값 저장 후 다음 갱신 때 처리
	void QueueTarget(const FShooterVitalValues& InValues);

	// 초기값 Snap 처리
	void SnapToTarget();

	FShooterVitalBarTickResult Tick(float DeltaSeconds, const FShooterVitalBarSettings& Settings);

	FShooterVitalBarLayout GetLayout(const FShooterVitalBarSettings& Settings) const;

	float GetTargetD() const;
	float GetMaxHealth() const;
	float GetMaxShield() const;
	const FShooterVitalTrack& GetHealthTrack() const;
	const FShooterVitalTrack& GetShieldTrack() const;

private:
	void TakePendingValues(float& OutHealth, float& OutShield);
	void ApplyPendingValues(const FShooterVitalBarSettings& Settings);

	// 새 값 받을때 처리
	static void ApplyToTrack(FShooterVitalTrack& Track, float NewReal, const FShooterVitalBarSettings& Settings);
	static void AdvanceTrack(FShooterVitalTrack& Track, float DeltaSeconds, const FShooterVitalBarSettings& Settings);
	static void SnapTrack(FShooterVitalTrack& Track, float Value);
	static bool NeedsUpdates(const FShooterVitalTrack& Track);

	FShooterVitalTrack Health;
	FShooterVitalTrack Shield;

	float MaxHealth = 0.f;
	float MaxShield = 0.f;

	FShooterVitalValues PendingValues;
	bool bHasPendingValues = false;
};
