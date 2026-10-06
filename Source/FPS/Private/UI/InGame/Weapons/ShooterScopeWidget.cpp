#include "UI/InGame/Weapons/ShooterScopeWidget.h"

#include "Combat/CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/ShooterPlayerController.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

void UShooterScopeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UnbindController();
	BoundController = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (AShooterPlayerController* Controller = BoundController.Get())
	{
		Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		ViewPresentationChangedHandle = Controller->OnViewPresentationChanged.AddUObject(
			this, &ThisClass::HandleViewPresentationChanged);
	}

	HandleViewPresentationChanged();
}

void UShooterScopeWidget::NativeDestruct()
{
	UnbindController();
	UnbindCombatComponent();

	Super::NativeDestruct();
}

void UShooterScopeWidget::UnbindController()
{
	if (AShooterPlayerController* Controller = BoundController.Get())
	{
		Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		Controller->OnViewPresentationChanged.Remove(ViewPresentationChangedHandle);
	}
	ViewPresentationChangedHandle.Reset();
	BoundController.Reset();
}

void UShooterScopeWidget::HandlePossessedPawnChanged(APawn*, APawn*)
{
	HandleViewPresentationChanged();
}

void UShooterScopeWidget::HandleViewPresentationChanged()
{
	AShooterPlayerController* Controller = BoundController.Get();
	APawn* ViewedPawn = IsValid(Controller) ? Cast<APawn>(Controller->GetViewTarget()) : nullptr;
	UCombatComponent* Combat = IsValid(ViewedPawn) ? ViewedPawn->FindComponentByClass<UCombatComponent>() : nullptr;
	BindToCombatComponent(Combat);
	RefreshScopeVisibility();
}

void UShooterScopeWidget::BindToCombatComponent(UCombatComponent* NewCombat)
{
	if (BoundCombatComponent.Get() == NewCombat)
	{
		return;
	}

	UnbindCombatComponent();
	BoundCombatComponent = NewCombat;
	if (NewCombat)
	{
		NewCombat->OnCurrentWeaponChanged.AddUniqueDynamic(this, &ThisClass::HandleCurrentWeaponChanged);
		AimingStateChangedHandle = NewCombat->OnAimingStateChanged.AddUObject(
			this, &ThisClass::HandleAimingStateChanged);
	}
}

void UShooterScopeWidget::UnbindCombatComponent()
{
	if (UCombatComponent* Combat = BoundCombatComponent.Get())
	{
		Combat->OnCurrentWeaponChanged.RemoveDynamic(this, &ThisClass::HandleCurrentWeaponChanged);
		Combat->OnAimingStateChanged.Remove(AimingStateChangedHandle);
	}
	AimingStateChangedHandle.Reset();
	BoundCombatComponent.Reset();
}

void UShooterScopeWidget::HandleCurrentWeaponChanged(AWeaponBase*)
{
	RefreshScopeVisibility();
}

void UShooterScopeWidget::HandleAimingStateChanged(bool)
{
	RefreshScopeVisibility();
}

void UShooterScopeWidget::RefreshScopeVisibility()
{
	const AShooterPlayerController* Controller = BoundController.Get();
	const UCombatComponent* Combat = BoundCombatComponent.Get();
	const AWeaponBase* Weapon = IsValid(Combat) ? Combat->GetCurrentWeapon() : nullptr;
	const bool bShowScope = IsValid(Controller)
		&& !Controller->IsKillcamViewTargetSuppressed()
		&& IsValid(Combat)
		&& Combat->IsAiming()
		&& IsValid(Weapon)
		&& Weapon->CanADS()
		&& Weapon->GetWeaponTag() == RogueTags::Weapon_Type_Sniper;
	const ESlateVisibility DesiredVisibility = bShowScope
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (GetVisibility() != DesiredVisibility)
	{
		SetVisibility(DesiredVisibility);
	}
}
