// ShooterReticle.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "ShooterReticle.generated.h"

class AWeaponBase;
class UImage;
class UTextBlock; // (Modified: CommonTextBlock 요구가 바인딩 혼란을 줘서 기본 UTextBlock으로 완화)
class UWidgetAnimation; // (Modified: 히트마커 페이드 효과 추가)

/**
 * 크로스헤어 탄퍼짐(Spread)만 표현한다.
 * GetSpreadDegrees()가 장착 무기의 AWeaponBase::GetCurrentSpreadDegrees()를 읽는다.
 */
UCLASS(Abstract)
class FPS_API UShooterReticle : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	float ComputeSpreadRadius() const;
	
	// ADS 상태 확인. 
	// 상태가 바뀔때 크로스헤어의 디자인 바꾸고싶으면 사용
	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsPerfectAccuracy() const;
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 장착 무기가 없거나 동적 크로스헤어가 꺼져 있으면 0.
	float GetSpreadDegrees() const;

	AWeaponBase* FindEquippedWeapon() const;

	// AShooterPlayerController::OnHitMarker 구독 핸들러
	// 히트 시 X 표시, 헤드샷이면 빨간색으로 표시
	void HandleHitMarker(bool bRed);
	void HideHitMarker();
	void SetCrosshairArmsVisible(bool bVisible);

	UFUNCTION()
	void HandleHitMarkerAnimationFinished();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Top;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Bottom;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Left;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Right;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Center;

	// 텍스트 글리프로 표시
	// 평상시 Hidden, 히트 시 잠깐 노출. 색상은 코드에서 덮어쓰지만 폰트/크기/글리프는 디자이너가 설정.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HitMarker;
	
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HitMarker;

	// 탄퍼짐 0일때 간격
	UPROPERTY(EditAnywhere, Category = "UI")
	float MinSpreadRadius = 8.f;

	// 탄퍼짐 표현 정도가 심해지면 제한한다
	UPROPERTY(EditAnywhere, Category = "UI")
	float SpreadRadiusScale = 1.f;

	// 히트마커 노출 시간(초)
	UPROPERTY(EditAnywhere, Category = "UI")
	float HitMarkerDuration = 0.4f;

private:
	FTimerHandle HitMarkerTimerHandle;
};
