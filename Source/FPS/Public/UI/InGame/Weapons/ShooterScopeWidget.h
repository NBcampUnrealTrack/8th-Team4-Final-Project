#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterScopeWidget.generated.h"

class APawn;
class AShooterPlayerController;
class AWeaponBase;
class UCombatComponent;

// 스코프 디자인은 Blueprint, 표시 상태는 현재 시점의 Combat 상태로만 결정한다.
UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UShooterScopeWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleCurrentWeaponChanged(AWeaponBase* NewWeapon);

	void HandleViewPresentationChanged();
	void HandleAimingStateChanged(bool bAiming);
	void UnbindController();
	void BindToCombatComponent(UCombatComponent* NewCombat);
	void UnbindCombatComponent();
	void RefreshScopeVisibility();

	TWeakObjectPtr<AShooterPlayerController> BoundController;
	TWeakObjectPtr<UCombatComponent> BoundCombatComponent;
	FDelegateHandle ViewPresentationChangedHandle;
	FDelegateHandle AimingStateChangedHandle;
};
