// ShooterWeaponStatus.cpp
#include "UI/InGame/Weapons/ShooterWeaponStatus.h"
#include "Weapon/WeaponBase.h"
#include "UI/InGame/Weapons/ShooterWeaponDisplaySet.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Interfaces/PlayerInterface.h"
#include "GameFramework/Pawn.h"

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
			UE_LOG(LogTemp, Warning,TEXT("[WeaponStatus] 장착 무기를 찾지 못했습니다. 폰에 AWeaponBase 가 Attach 되는지 확인하세요"));
		}
		return;
	}
	
	bLoggedNoWeapon = false;

	const int32 CurrentAmmo = Weapon->GetCurrentAmmo();
	const int32 MaxAmmo = Weapon->GetMagazineSize();
	
	// 값이 달라질때만 갱신
	if (CachedAmmo == CurrentAmmo && CachedMaxAmmo == MaxAmmo)
	{
		return;
	}
	
	CachedAmmo = CurrentAmmo;
	CachedMaxAmmo = MaxAmmo;
	
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
		return;
	}

	Text_Ammo->SetVisibility(NewWeapon->UsesAmmo() ? ESlateVisibility::HitTestInvisible	: ESlateVisibility::Collapsed);
	Text_MaxAmmo->SetVisibility(NewWeapon->UsesAmmo() ? ESlateVisibility::HitTestInvisible	: ESlateVisibility::Collapsed);
	
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
	Text_Ammo->SetText(FText::AsNumber(Weapon->GetCurrentAmmo()));
	Text_MaxAmmo->SetText(FText::AsNumber(Weapon->GetMagazineSize()));
	
	// Style 갱신
	const float AmmoPer = CachedMaxAmmo > 0 ? static_cast<float>(CachedAmmo) / CachedMaxAmmo : 0.f;
	UE_LOG(LogTemp, Warning, TEXT("%f = %d / %d | AmmoPer = CurrentAmmo / MaxAmmo"), AmmoPer, CachedAmmo, CachedMaxAmmo);
	const EShooterHUDVisualTypes Type = GetAmmoVisualType(AmmoPer);
	
	ApplyHUDVisualStyle(AmmoVisualStyles.GetStyle(Type));
}

EShooterHUDVisualTypes UShooterWeaponStatus::GetAmmoVisualType(float AmmoPercent) const
{
	if (AmmoPercent <= CriticalThreshold)
	{
		UE_LOG(LogTemp, Warning, TEXT("Critical Mode On"));
		return EShooterHUDVisualTypes::Critical;
	}
	if (AmmoPercent <= WarningThreshold)
	{
		UE_LOG(LogTemp, Warning, TEXT("Warning Mode On"));
		return EShooterHUDVisualTypes::Warning;
	}

	UE_LOG(LogTemp, Warning, TEXT("Normal Mode On"));
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
	
	// AccentColor나 나머지도 여기서 적용
}
