// ShooterVitalBar.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/InGame/HUD/ShooterVitalBarBurst.h"
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

	// 피격 조각
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_DamageBurst;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	FShooterVitalBarSettings Settings;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	FShooterVitalBarBurstSettings BurstSettings;

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

	// 대기 값을 그 시각에 반영
	void CommitPending();
	// 모델과 조각을 같은 시계로 진행
	FShooterVitalBarTickResult AdvanceTo(double Time);

	void ApplyLayout(const FShooterVitalBarLayout& Layout);
	void ApplyBurst();
	void SetBurstVisible(bool bVisible);
	UMaterialInstanceDynamic* GetBarMID();
	UMaterialInstanceDynamic* GetBurstMID();

	static double GetSlateTime();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedBurstMID;

	FShooterVitalBarModel Model;
	FShooterVitalBarBurst Burst;

	// 아직 반영 안 한 마지막 값
	FShooterVitalValues PendingValues;
	double PendingTime = 0.0;
	bool bHasPending = false;

	// 모델과 조각이 진행된 시각
	double ClockTime = 0.0;

	// 마지막으로 MID 에 넣은 레이아웃
	FShooterVitalBarLayout LastAppliedLayout;

	bool bBurstVisible = false;

	TWeakPtr<FActiveTimerHandle> UpdateTimerHandle;
};
