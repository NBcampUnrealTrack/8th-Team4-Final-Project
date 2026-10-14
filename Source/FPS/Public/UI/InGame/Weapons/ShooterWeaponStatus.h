// ShooterWeaponStatus.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/InGame/HUD/ShooterHUDTypes.h"
#include "ShooterWeaponStatus.generated.h"

class AWeaponBase;
class APawn;
class UCombatComponent;
class UCommonTextBlock;
class UImage;
class UShooterWeaponDisplaySet;
class UBorder;

/**
 * HUD의 자식으로 탄약 표시 목적.
 * 폴링하지 않는다 - PlayerController::OnPossessedPawnChanged로 빙의 변경(리스폰 포함)을 감지해
 * CombatComponent::OnCurrentWeaponChanged를 구독하고, 무기가 바뀌면 그 무기의 OnAmmoChanged를 구독한다.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UShooterWeaponStatus : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 빙의 대상(Pawn)이 바뀔 때마다 PlayerController가 호출한다 - 리스폰으로 Pawn이 새로 생겨도
	// 여기서 다시 CombatComponent를 찾아 구독하므로 매 프레임 폴링할 필요가 없다.
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	// 이전 CombatComponent 구독을 해제하고 새 CombatComponent(nullptr 가능)를 구독한다.
	void BindToCombatComponent(UCombatComponent* NewCombat);

	// CombatComponent::OnCurrentWeaponChanged 구독 콜백. 이전 무기의 OnAmmoChanged 구독을 해제하고
	// 새 무기(nullptr 가능)를 구독한다.
	UFUNCTION()
	void HandleCurrentWeaponChanged(AWeaponBase* NewWeapon);

	// AWeaponBase::OnAmmoChanged 구독 콜백.
	UFUNCTION()
	void HandleAmmoChanged();

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
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Panel;
	// Ammo / MaxAmmo 사이의 / <-이거
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Slash;
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

	// OnCurrentWeaponChanged 구독 해제용으로 현재 구독 중인 CombatComponent를 기억해둔다.
	TWeakObjectPtr<UCombatComponent> CachedCombat;

	int32 CachedAmmo = INDEX_NONE;
	// GetEffectiveMagazineSize()(증강 반영된 탄창 최대치). 화면에는 표시하지 않고 색상 경고 비율 계산에만 쓴다.
	int32 CachedMaxAmmo = INDEX_NONE;
	int32 CachedReserveAmmo = INDEX_NONE;
	bool bCachedInfiniteAmmo = false;
};
