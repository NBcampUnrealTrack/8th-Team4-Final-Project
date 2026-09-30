// ShooterGauge.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ShooterGauge.generated.h"

class FActiveTimerHandle;
class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;
enum class EActiveTimerReturnType : uint8;

/**
 * 엔진 UProgressBar을 사용하지 않는다.
 * 채워지는 Bar, Health, Shield , Cooldown 등에서 사용
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UShooterGauge : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	// 보간해서 따라갈 목표 비율
	void SetProgress(float InProgress);
	
	// 보간 없이 즉시 반영
	void SetProgressImmediate(float InProgress);
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	virtual void SynchronizeProperties() override;
	
	UMaterialInstanceDynamic* GetBarMID();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Bar;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Bg;

	UPROPERTY(EditAnywhere, Category = "UI")
	TObjectPtr<UTexture2D> BarTexture;
	/** 빈 상태 이미지. 없으면 배경 없이 채움만 그린다 */
	UPROPERTY(EditAnywhere, Category = "UI")
	TObjectPtr<UTexture2D> BgTexture;
	
	// 클 수록 보간 Speed가 커진다.
	// 0이면 보간하지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	float InterpSpeed = 8.f;
	
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedMID;
	
	float DisplayProgress = 1.f;
	float TargetProgress = 1.f;

private:
	// 목표에 닿을 때까지만 도는 보간 루프
	void EnsureUpdateScheduled();
	void StopUpdates();
	EActiveTimerReturnType HandleUpdateTimer(double InCurrentTime, float InDeltaTime);
	void ApplyDisplayProgress();

	TWeakPtr<FActiveTimerHandle> UpdateTimerHandle;
};
