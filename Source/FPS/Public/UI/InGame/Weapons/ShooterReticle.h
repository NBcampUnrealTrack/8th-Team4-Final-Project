// ShooterReticle.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterReticle.generated.h"

class AWeaponBase;
class UImage;

/**
 * 크로스헤어 탄퍼짐(Spread)만 표현한다.
 * 현재 탄퍼짐 값을 받지 못하므로 더미 데이터 넣어 먼저 검증한다. 
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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	/** 테스트용 더미 데이터 탄퍼짐이 오면 지운다.*/ 
	float GetSpreadDegrees() const;
	
	AWeaponBase* FindEquippedWeapon() const;
	
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
	
	// 탄퍼짐 0일때 간격
	UPROPERTY(EditAnywhere, Category = "UI")
	float MinSpreadRadius = 8.f;
	
	// 탄퍼짐 표현 정도가 심해지면 제한한다
	UPROPERTY(EditAnywhere, Category = "UI")
	float SpreadRadiusScale = 1.f;
};
