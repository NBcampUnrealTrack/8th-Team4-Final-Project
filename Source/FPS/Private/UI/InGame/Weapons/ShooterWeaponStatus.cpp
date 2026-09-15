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
		}
		return;
	}
	
	bLoggedNoWeapon = false;

	const int32 CurrentAmmo = Weapon->GetCurrentAmmo();
	const int32 MaxAmmo = Weapon->GetEffectiveMagazineSize();
	const int32 ReserveAmmo = Weapon->GetCurrentReserveAmmo();
	const bool bInfiniteReserveAmmo = Weapon->HasInfiniteReserveAmmo();

	// 수량이 같아도 정책이 바뀌면 무한 표시를 갱신한다.
	if (CachedAmmo == CurrentAmmo && CachedMaxAmmo == MaxAmmo && CachedReserveAmmo == ReserveAmmo
		&& bCachedInfiniteReserveAmmo == bInfiniteReserveAmmo)
	{
		return;
	}

	CachedAmmo = CurrentAmmo;
	CachedMaxAmmo = MaxAmmo;
	CachedReserveAmmo = ReserveAmmo;
	bCachedInfiniteReserveAmmo = bInfiniteReserveAmmo;

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
	
	// 무기 교체 시 탄약 개수가 같을 수도 있으므로 초기화
	CachedAmmo = INDEX_NONE;
	CachedMaxAmmo = INDEX_NONE;
	CachedReserveAmmo = INDEX_NONE;
	bCachedInfiniteReserveAmmo = false;
	
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
	// '/' 앞: 현재 탄창 잔탄. 장전하면 GetCurrentAmmo()가 (증강 반영된) 탄창 최대치까지 차서
	// 자동으로 최대값으로 표시된다. 탄창 증강이 켜지면 EffectiveMagazineSize가 커지므로 이 값도 함께 올라간다.
	Text_Ammo->SetText(FText::AsNumber(Weapon->GetCurrentAmmo()));
	// 무한 기호는 표시용이다. 실제 예비탄 수량을 임의의 큰 값으로 바꾸지 않는다.
	const FText ReserveAmmoText = Weapon->HasInfiniteReserveAmmo()
		? FText::AsCultureInvariant(TEXT("\u221E"))
		: FText::AsNumber(Weapon->GetCurrentReserveAmmo());
	Text_MaxAmmo->SetText(ReserveAmmoText);
	if (IsValid(Text_ReserveAmmo))
	{
		Text_ReserveAmmo->SetText(ReserveAmmoText);
	}

	EShooterHUDVisualTypes Type = EShooterHUDVisualTypes::Normal;

	if (Weapon->UsesAmmo())
	{
		// 색상 경고(Normal/Warning/Critical)는 표시값과 무관하게 "현재 탄창 / 탄창 최대치" 비율로 판정한다.
		// CachedMaxAmmo에는 GetEffectiveMagazineSize()가 들어있다(화면에는 표시되지 않고 이 비율 계산에만 쓴다).
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
	
	// AccentColor나 나머지도 여기서 적용
}
