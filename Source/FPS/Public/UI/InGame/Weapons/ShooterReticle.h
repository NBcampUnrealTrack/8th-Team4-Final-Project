// ShooterReticle.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "ShooterReticle.generated.h"

class AWeaponBase;
class UImage;

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
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	// 장착 무기가 없거나 동적 크로스헤어가 꺼져 있으면 0.
	float GetSpreadDegrees() const;
	
	AWeaponBase* FindEquippedWeapon() const;
	
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
	
	// 탄퍼짐 0일때 간격
	UPROPERTY(EditAnywhere, Category = "UI")
	float MinSpreadRadius = 8.f;
	
	// 탄퍼짐 표현 정도가 심해지면 제한한다
	UPROPERTY(EditAnywhere, Category = "UI")
	float SpreadRadiusScale = 1.f;
};
