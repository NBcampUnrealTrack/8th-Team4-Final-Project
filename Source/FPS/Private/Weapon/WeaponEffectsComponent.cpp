#include "Weapon/WeaponEffectsComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Weapon/WeaponBase.h"

UWeaponEffectsComponent::UWeaponEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

AWeaponBase* UWeaponEffectsComponent::GetWeaponOwner() const
{
	return Cast<AWeaponBase>(GetOwner());
}

void UWeaponEffectsComponent::PlayFireCameraShake() const
{
	if (!FireCameraShake)
	{
		return;
	}

	const AWeaponBase* Weapon = GetWeaponOwner();
	const APawn* OwningPawn = Weapon ? Cast<APawn>(Weapon->GetOwner()) : nullptr;
	APlayerController* PlayerController = OwningPawn ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr;
	if (PlayerController && PlayerController->IsLocalController() && PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->StartCameraShake(FireCameraShake);
	}
}

void UWeaponEffectsComponent::PlayEmptyClipSound()
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->HasAuthority() && EmptyClipSound)
	{
		Multicast_PlayEmptyClipSound();
	}
}

void UWeaponEffectsComponent::Multicast_PlayEmptyClipSound_Implementation()
{
	if (!EmptyClipSound || !GetOwner())
	{
		return;
	}

	UGameplayStatics::SpawnSoundAttached(
		EmptyClipSound,
		GetOwner()->GetRootComponent(),
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset);
}
