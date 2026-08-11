// ShooterGauge.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ShooterGauge.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * 엔진 UProgressBar을 사용하지 않는다.
 * 채워지는 Bar, Health, Shield , Cooldown 등에서 사용
 */
UCLASS(Abstract)
class FPS_API UShooterGauge : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	// 보간해서 따라갈 목표 비율
	void SetProgress(float InProgress);
	
	// 보간 없이 즉시 반영
	void SetProgressImmediate(float InProgress);
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
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
	
	// 머터리얼에 값을 한 번이라도 넘겼는가 확인
	// MID를 못 얻은 상태에서 SetProgressImmediate가 불리면 재시도를 안 하는 것을 막는 플래그
	bool bParameterApplied = false;
};
