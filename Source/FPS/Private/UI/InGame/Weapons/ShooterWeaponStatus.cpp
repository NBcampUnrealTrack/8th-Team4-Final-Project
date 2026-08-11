// ShooterWeaponStatus.cpp
#include "UI/InGame/Weapons/ShooterWeaponStatus.h"
#include "Weapon/WeaponBase.h"
#include "UI/InGame/Weapons/ShooterWeaponDisplaySet.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
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
	
	// UCombatComponent::SetCurrentWeapon 을 부르는 곳이 생기면 이 블록을 지운다.
	// 안 지우면 정식 장착 경로가 비어 있어도 UI 만 정상으로 보인다.
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors);

	for (AActor* AttachedActor : AttachedActors)
	{
		if (AWeaponBase* Weapon = Cast<AWeaponBase>(AttachedActor))
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
		return;
	}

	Text_Ammo->SetVisibility(NewWeapon->UsesAmmo() ? ESlateVisibility::HitTestInvisible	: ESlateVisibility::Collapsed);
}

void UShooterWeaponStatus::RefreshAmmo(AWeaponBase* Weapon)
{
	Text_Ammo->SetText(FText::Format(INVTEXT("{0} / {1}"), Weapon->GetCurrentAmmo(), Weapon->GetMagazineSize()));
}
