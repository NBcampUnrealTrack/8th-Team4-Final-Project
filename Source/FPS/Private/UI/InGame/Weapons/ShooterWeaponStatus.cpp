#include "UI/InGame/Weapons/ShooterWeaponStatus.h"
#include "Weapon/WeaponBase.h"
#include "UI/InGame/Weapons/ShooterWeaponDisplaySet.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Interfaces/PlayerInterface.h"
#include "GameFramework/Pawn.h"
#include "Player/TrainingPlayerController.h"

void UShooterWeaponStatus::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	AWeaponBase* Weapon = FindEquippedWeapon();
	
	if (Weapon != CachedWeapon.Get())
	{
		CachedWeapon = Weapon;
		RebuildFromWeapon(Weapon);
	}
	
	if (!Weapon)
	{
		if (!bLoggedNoWeapon)
		{
			bLoggedNoWeapon = true;
		}
		return;
	}
	
	bLoggedNoWeapon = false;

	const int32 CurrentAmmo = Weapon->GetCurrentAmmo();
	const int32 MaxAmmo = Weapon->GetEffectiveMagazineSize();
	const int32 ReserveAmmo = Weapon->GetCurrentReserveAmmo();
	if (CachedAmmo == CurrentAmmo && CachedMaxAmmo == MaxAmmo && CachedReserveAmmo == ReserveAmmo)
	{
		return;
	}

	CachedAmmo = CurrentAmmo;
	CachedMaxAmmo = MaxAmmo;
	CachedReserveAmmo = ReserveAmmo;

	RefreshAmmo(Weapon);
}

AWeaponBase* UShooterWeaponStatus::FindEquippedWeapon() const
{
	APawn* Pawn = GetOwningPlayerPawn();
	if (!Pawn)
	{
		return nullptr;
	}
	
	if (Pawn->Implements<UPlayerInterface>())
	{
		if (AWeaponBase* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(Pawn))
		{
			return Weapon;
		}
	}
	
	return nullptr;
}

void UShooterWeaponStatus::RebuildFromWeapon(AWeaponBase* NewWeapon)
{
	if (!NewWeapon)
	{
		Text_Ammo->SetText(FText::GetEmpty());
		Text_MaxAmmo->SetText(FText::GetEmpty());
		if (IsValid(Text_ReserveAmmo))
		{
			Text_ReserveAmmo->SetText(FText::GetEmpty());
		}
		return;
	}

	Text_Ammo->SetVisibility(NewWeapon->UsesAmmo() ? ESlateVisibility::HitTestInvisible	: ESlateVisibility::Collapsed);
	Text_MaxAmmo->SetVisibility(NewWeapon->UsesAmmo() ? ESlateVisibility::HitTestInvisible	: ESlateVisibility::Collapsed);
	if (IsValid(Text_ReserveAmmo))
	{
		Text_ReserveAmmo->SetVisibility(NewWeapon->UsesAmmo() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	
	CachedAmmo = INDEX_NONE;
	CachedMaxAmmo = INDEX_NONE;
	CachedReserveAmmo = INDEX_NONE;
	
	if (!WeaponDisplaySet)
	{
		UE_LOG(LogTemp, Warning,TEXT("[WeaponStatus] DA_WeaponDisplaySet이 지정되지않았습니다."));
		return;
	}
	
	if (const FShooterWeaponDisplay* WeaponTag = WeaponDisplaySet->FindDisplay(NewWeapon->GetWeaponTag()))
	{
		if (IsValid(Text_WeaponName))
		{
			Text_WeaponName->SetText(WeaponTag->DisplayName);
		}
		if (IsValid(Image_WeaponIcon))
		{
			Image_WeaponIcon->SetBrush(WeaponTag->Icon);
		}
	}
}

void UShooterWeaponStatus::RefreshAmmo(AWeaponBase* Weapon)
{
	const bool bTrainingMode = Cast<ATrainingPlayerController>(GetOwningPlayer()) != nullptr;
	const FText InfiniteAmmoText = FText::AsCultureInvariant(TEXT("\x221E"));

	Text_Ammo->SetText(bTrainingMode
		? InfiniteAmmoText
		: FText::AsNumber(Weapon->GetCurrentAmmo()));

	const FText ReserveAmmoText = bTrainingMode
		? InfiniteAmmoText
		: FText::AsNumber(Weapon->GetCurrentReserveAmmo());
	Text_MaxAmmo->SetText(ReserveAmmoText);
	if (IsValid(Text_ReserveAmmo))
	{
		Text_ReserveAmmo->SetText(ReserveAmmoText);
	}

	EShooterHUDVisualTypes Type = EShooterHUDVisualTypes::Normal;

	if (Weapon->UsesAmmo())
	{
		const float AmmoPer = CachedMaxAmmo > 0 ? static_cast<float>(CachedAmmo) / CachedMaxAmmo : 0.f;
		Type = GetAmmoVisualType(AmmoPer);
	}
	
	ApplyHUDVisualStyle(AmmoVisualStyles.GetStyle(Type));
}

EShooterHUDVisualTypes UShooterWeaponStatus::GetAmmoVisualType(float AmmoPercent) const
{
	if (AmmoPercent <= CriticalThreshold)
	{
		return EShooterHUDVisualTypes::Critical;
	}
	if (AmmoPercent <= WarningThreshold)
	{
		return EShooterHUDVisualTypes::Warning;
	}

	return EShooterHUDVisualTypes::Normal;
}

void UShooterWeaponStatus::ApplyHUDVisualStyle(const FShooterAmmoVisualStyle& Style)
{
	if (Image_WeaponIcon)
	{
		Image_WeaponIcon->SetColorAndOpacity(Style.WeaponIconColor);
	}
	
	if (Text_WeaponName)
	{
		Text_WeaponName->SetColorAndOpacity(Style.WeaponNameColor);
	}
	
	if (Text_Ammo)
	{
		Text_Ammo->SetColorAndOpacity(Style.CurrentAmmoColor);
	}
	
	if (Text_MaxAmmo)
	{
		Text_MaxAmmo->SetColorAndOpacity(Style.MaxAmmoColor);
	}
	
	if (Image_Ammo)
	{
		Image_Ammo->SetColorAndOpacity(Style.AmmoIconColor);
	}
	
	if (Border_Panel)
	{
		Border_Panel->SetBrushColor(Style.PanelColor);
	}
	
	if (Image_Frame)
	{
		Image_Frame->SetColorAndOpacity(Style.FrameColor);
	}
	
	if (Image_Accent)
	{
		Image_Accent->SetColorAndOpacity(Style.AccentColor);
	}
	
}
