// ShooterVitalBar.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/InGame/HUD/ShooterVitalBarModel.h"
#include "ShooterVitalBar.generated.h"

class FActiveTimerHandle;
class UImage;
class UMaterialInstanceDynamic;
enum class EActiveTimerReturnType : uint8;

/**
 * Health, Shield Bar
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UShooterVitalBar : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 다음 갱신에 반영. 마지막 값만 남는다
	void SetVitals(const FShooterVitalValues& InValues);

	// 연출 없이 즉시 반영
	void SetVitalsImmediate(const FShooterVitalValues& InValues);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void SynchronizeProperties() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Bar;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	FShooterVitalBarSettings Settings;

	UPROPERTY(EditAnywhere, Category = "UI|Preview")
	FShooterVitalValues DesignTimeVitals = FShooterVitalValues(60.f, 100.f, 20.f, 50.f);

	UPROPERTY(EditAnywhere, Category = "UI|Preview")
	float DesignTimeHealthTrail = 80.f;
	UPROPERTY(EditAnywhere, Category = "UI|Preview")
	float DesignTimeShieldTrail = 30.f;

private:
	void EnsureUpdateScheduled();
	void StopUpdates();
	EActiveTimerReturnType HandleUpdateTimer(double InCurrentTime, float InDeltaTime);
	void ApplyLayout(const FShooterVitalBarLayout& Layout);
	UMaterialInstanceDynamic* GetBarMID();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedMID;

	FShooterVitalBarModel Model;

	TWeakPtr<FActiveTimerHandle> UpdateTimerHandle;

	double LastUpdateTime = 0.0;
};
