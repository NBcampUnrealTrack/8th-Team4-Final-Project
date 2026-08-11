// ShooterWeaponStatus.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"
#include "ShooterWeaponStatus.generated.h"

class AWeaponBase;
class UCommonTextBlock;
class UImage;
class UShooterWeaponDisplaySet;

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
	
	UPROPERTY(meta = (BindWIdget))
	TObjectPtr<UCommonTextBlock> Text_Ammo;
	
	// 무기이름 없으면 비워둠
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_WeaponName;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_WeaponIcon;
	
	// WeaponType Tag로 Find -> 이름/아이콘 맵핑 비어있어도 탄약은 보임
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TObjectPtr<UShooterWeaponDisplaySet> WeaponDisplaySet
	;
	// 현재 착용중인 무기 Cashing
	TWeakObjectPtr<AWeaponBase> CachedWeapon;
	
	bool bLoggedNoWeapon = false;
};
