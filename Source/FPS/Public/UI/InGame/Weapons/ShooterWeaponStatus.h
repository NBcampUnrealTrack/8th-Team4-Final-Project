// ShooterWeaponStatus.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/InGame/HUD/ShooterHUDTypes.h"
#include "ShooterWeaponStatus.generated.h"

class AWeaponBase;
class UCommonTextBlock;
class UImage;
class UShooterWeaponDisplaySet;
class UBorder;

/**
 * HUD의 자식으로 탄약 표시 목적
 * NativeTick으로 폴링한다
 * OnAmmoChanged가 생기면 NativeTick을ㄹ 걷어내고 Delegate로 변경
 */
UCLASS(Abstract)
class FPS_API UShooterWeaponStatus : public UCommonUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	// Pawn에게 IPlayerInterface로 물어본다 못찾으면 nullptr
	AWeaponBase* FindEquippedWeapon() const;
	
	// Weapon Change시 이름,아이콘 갱신
	void RebuildFromWeapon(AWeaponBase* NewWeapon);
	void RefreshAmmo(AWeaponBase* Weapon);
	
	// Threshold로 퍼센트를 가져온다.
	EShooterHUDVisualTypes GetAmmoVisualType(float AmmoPercent) const;
	// Set Style 적용
	void ApplyHUDVisualStyle(const FShooterAmmoVisualStyle& Style);
	
	// '/' 앞: 현재 탄창 잔탄(GetCurrentAmmo).
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Ammo;
	// '/' 뒤: 이름은 MaxAmmo지만 실제로는 예비탄 잔탄 또는 무한 기호를 표시한다.
	// 바인딩/스타일 데이터를 유지하려고 위젯 이름은 그대로 두고 표시값만 바꿨다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_MaxAmmo;
	// 예비(보유) 탄약을 별도 위젯으로도 표시하고 싶을 때 쓰는 옵션 슬롯. 없으면 무시된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_ReserveAmmo;
	
	// 무기이름 없으면 비워둠
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_WeaponName;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_WeaponIcon;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Ammo;
	// Ammo / MaxAmmo 사이의 / <-이거
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Panel;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Frame;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Accent;
	
	// Weapon 상태 변화 per
	UPROPERTY(EditDefaultsOnly, Category = "Ammo|Edit")
	float WarningThreshold = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Ammo|Edit")
	float CriticalThreshold = 0.25f;
	
	// Weapon Panel 탄약에 따른 표시 상태 변화
	UPROPERTY(EditDefaultsOnly, Category = "Ammo|Visual")
	FShooterAmmoVisualStyleSet AmmoVisualStyles;
	
	// WeaponType Tag로 Find
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TObjectPtr<UShooterWeaponDisplaySet> WeaponDisplaySet
	;
	// 현재 착용중인 무기 Cashing
	TWeakObjectPtr<AWeaponBase> CachedWeapon;
	
	int32 CachedAmmo = INDEX_NONE;
	// GetEffectiveMagazineSize()(증강 반영된 탄창 최대치). 화면에는 표시하지 않고 색상 경고 비율 계산에만 쓴다.
	int32 CachedMaxAmmo = INDEX_NONE;
	int32 CachedReserveAmmo = INDEX_NONE;
	bool bCachedInfiniteAmmo = false;
	
	bool bLoggedNoWeapon = false;
};
