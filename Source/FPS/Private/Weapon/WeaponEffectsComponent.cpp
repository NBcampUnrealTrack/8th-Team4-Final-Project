#include "Weapon/WeaponEffectsComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Weapon/WeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Tags/GameplayTags.h"

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

void UWeaponEffectsComponent::PlayFireSound() const
{
	const auto* Weapon = GetWeaponOwner();
	if (!FireSound || !Weapon || GetNetMode() == NM_DedicatedServer
		|| Weapon->GetWeaponTag() == RogueTags::Weapon_Type_Knife || !Weapon->GetMesh3P()) return;
	// A single world-space emitter serves owner and observers alike.
	UGameplayStatics::SpawnSoundAttached(FireSound, Weapon->GetMesh3P());
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
	const auto* Weapon = GetWeaponOwner();
	if (!EmptyClipSound || !Weapon || !Weapon->GetMesh3P() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UGameplayStatics::SpawnSoundAttached(
		EmptyClipSound,
		Weapon->GetMesh3P(),
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset);
}
