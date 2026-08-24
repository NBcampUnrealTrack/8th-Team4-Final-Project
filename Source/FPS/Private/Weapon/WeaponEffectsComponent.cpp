// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/WeaponEffectsComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
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

void UWeaponEffectsComponent::PlayFireEffects(const FVector& TraceStart, const TArray<FVector>& PelletDirections)
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	if (FireSound)
	{
		Multicast_PlayFireSound(TraceStart);
	}

	if (MuzzleFlashEffect || ProjectileTrailEffect)
	{
		Multicast_PlayMuzzleEffects(PelletDirections);
	}
}

void UWeaponEffectsComponent::PlayFireCameraShake() const
{
	if (!FireCameraShake)
	{
		return;
	}

	const AActor* WeaponOwner = GetOwner();
	const APawn* OwningPawn = WeaponOwner ? Cast<APawn>(WeaponOwner->GetOwner()) : nullptr;
	APlayerController* PC = OwningPawn ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr;

	// 화면 흔들림은 쏜 본인의 화면에만 있어야 한다. Fire()는 LocalPredicted 어빌리티라 예측
	// 클라이언트/서버 양쪽에서 실행되므로, 로컬 컨트롤러가 아닌 쪽에서는 재생하지 않는다.
	if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraShake(FireCameraShake);
	}
}

void UWeaponEffectsComponent::PlayEmptyClipSound()
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->HasAuthority() && EmptyClipSound)
	{
		Multicast_PlayWeaponSound(EmptyClipSound);
	}
}

void UWeaponEffectsComponent::NotifyReloadStarted(const float ReloadDuration)
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->HasAuthority() && ReloadStartSound)
	{
		Multicast_PlayWeaponSound(ReloadStartSound);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadChargeTimerHandle);

		if (ReloadChargeSound && ReloadChargeSoundDelay < ReloadDuration)
		{
			World->GetTimerManager().SetTimer(
				ReloadChargeTimerHandle, this, &UWeaponEffectsComponent::PlayReloadChargeSound, ReloadChargeSoundDelay, false);
		}
	}
}

void UWeaponEffectsComponent::PlayReloadChargeSound()
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->HasAuthority() && ReloadChargeSound)
	{
		Multicast_PlayWeaponSound(ReloadChargeSound);
	}
}

void UWeaponEffectsComponent::PlayReloadFinishSound()
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->HasAuthority() && ReloadFinishSound)
	{
		Multicast_PlayWeaponSound(ReloadFinishSound);
	}
}

void UWeaponEffectsComponent::Multicast_PlayFireSound_Implementation(FVector Location)
{
	if (FireSound && GetOwner())
	{
		UGameplayStatics::SpawnSoundAttached(
			FireSound,
			GetOwner()->GetRootComponent(),
			NAME_None,
			Location,
			FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition);
	}
}

void UWeaponEffectsComponent::Multicast_PlayWeaponSound_Implementation(USoundBase* Sound)
{
	if (Sound && GetOwner())
	{
		UGameplayStatics::SpawnSoundAttached(Sound, GetOwner()->GetRootComponent());
	}
}

void UWeaponEffectsComponent::Multicast_PlayMuzzleEffects_Implementation(const TArray<FVector>& PelletDirections)
{
	SpawnMuzzleFlash();
	SpawnProjectileTrails(PelletDirections);
}

void UWeaponEffectsComponent::SpawnMuzzleFlash() const
{
	if (!MuzzleFlashEffect)
	{
		return;
	}

	const AWeaponBase* Weapon = GetWeaponOwner();
	if (!IsValid(Weapon))
	{
		return;
	}

	USkeletalMeshComponent* Mesh1P = Weapon->GetMesh1P();
	if (IsValid(Mesh1P) && !MuzzleSocketName1P.IsNone())
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlashEffect, Mesh1P, MuzzleSocketName1P, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true);
	}

	USkeletalMeshComponent* Mesh3P = Weapon->GetMesh3P();
	if (IsValid(Mesh3P) && !MuzzleSocketName3P.IsNone())
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlashEffect, Mesh3P, MuzzleSocketName3P, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true);
	}
}

void UWeaponEffectsComponent::SpawnProjectileTrails(const TArray<FVector>& PelletDirections) const
{
	if (!ProjectileTrailEffect)
	{
		return;
	}

	const AWeaponBase* Weapon = GetWeaponOwner();
	if (!IsValid(Weapon))
	{
		return;
	}

	SpawnProjectileTrailsFromMesh(Weapon->GetMesh1P(), MuzzleSocketName1P, PelletDirections);
	SpawnProjectileTrailsFromMesh(Weapon->GetMesh3P(), MuzzleSocketName3P, PelletDirections);
}

void UWeaponEffectsComponent::SpawnProjectileTrailsFromMesh(
	USkeletalMeshComponent* Mesh, const FName SocketName, const TArray<FVector>& PelletDirections) const
{
	if (!IsValid(Mesh) || SocketName.IsNone())
	{
		return;
	}

	const FVector MuzzleLocation = Mesh->GetSocketLocation(SocketName);
	for (const FVector& Direction : PelletDirections)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, ProjectileTrailEffect, MuzzleLocation, Direction.Rotation());
	}
}
