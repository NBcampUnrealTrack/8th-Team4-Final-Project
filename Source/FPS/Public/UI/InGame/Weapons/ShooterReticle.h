// ShooterReticle.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "ShooterReticle.generated.h"

class AWeaponBase;
class UImage;
class UTextBlock;
class UWidgetAnimation;

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
	
	// 탄퍼짐이 없는 상태인지 확인한다.
	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsPerfectAccuracy() const;

	// 현재 장착 무기가 ADS 상태인지 UI에 전달한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	bool IsAiming() const;

	// 현재 장착 무기의 태그를 UI에 전달한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	FGameplayTag GetEquippedWeaponTag() const;

	// 히트마커 표시 중인지 UI에 전달한다.
	// 기본 크로스헤어 Visibility는 Blueprint의 단일 바인딩에서 결정한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	bool IsHitMarkerActive() const;
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// 장착 무기가 없거나 동적 크로스헤어가 꺼져 있으면 0.
	float GetSpreadDegrees() const;

	AWeaponBase* FindEquippedWeapon() const;

	// 히트마커 자체의 표시 상태만 관리한다.
	// 기본 크로스헤어의 Visibility는 Blueprint 바인딩이 전담한다.
	void HandleHitMarker(const bool bRed);
	void HideHitMarker();

	UFUNCTION()
	void HandleHitMarkerAnimationFinished();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Top;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Bottom;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Left;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Right;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Center;

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

	// Anim_HitMarker가 없을 때 히트마커를 표시하는 시간
	UPROPERTY(EditAnywhere, Category = "UI")
	float HitMarkerDuration = 0.4f;

private:
	FTimerHandle HitMarkerTimerHandle;
	bool bHitMarkerActive = false;
};
