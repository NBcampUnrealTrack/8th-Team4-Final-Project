#include "Weapon/WeaponEffectsComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Weapon/WeaponBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Tags/GameplayTags.h"
#include "Sound/SoundAttenuation.h"

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
	UGameplayStatics::SpawnSoundAttached(FireSound, Weapon->GetMesh3P(), NAME_None,
		FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, 1.f, 1.f, 0.f, FireAttenuation);
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
		EAttachLocation::KeepRelativeOffset, false, 1.f, 1.f, 0.f, ActionAttenuation);
}

void UWeaponEffectsComponent::PlayEquipSound()
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->HasAuthority() && EquipSound)
	{
		// 무기 액터는 Pawn(→PlayerController) 소유라 Client RPC는 무기를 든 본인 연결에만 간다.
		// 리슨 서버 호스트 본인이면 로컬에서 바로 실행된다.
		Client_PlayEquipSound();
	}
}

void UWeaponEffectsComponent::Client_PlayEquipSound_Implementation()
{
	if (EquipSound && IsOwnerLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(this, EquipSound);
	}
}

void UWeaponEffectsComponent::PlayMeleeAttackSound(const bool bHeavyAttack)
{
	const AActor* Owner = GetOwner();
	USoundBase* Sound = ResolveMeleeAttackSound(bHeavyAttack);
	if (!IsValid(Owner) || !Sound)
	{
		return;
	}

	// 공격한 본인(예측 클라이언트 또는 리슨 서버 호스트)은 몽타주와 같은 프레임에 2D로 바로 재생한다.
	if (IsOwnerLocallyControlled())
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}

	if (Owner->HasAuthority())
	{
		Multicast_PlayMeleeAttackSound(bHeavyAttack);
	}
}

void UWeaponEffectsComponent::Multicast_PlayMeleeAttackSound_Implementation(const bool bHeavyAttack)
{
	// 공격한 본인은 PlayMeleeAttackSound에서 이미 로컬 재생했으므로 중복 재생하지 않는다.
	if (IsOwnerLocallyControlled())
	{
		return;
	}

	USoundBase* Sound = ResolveMeleeAttackSound(bHeavyAttack);
	USceneComponent* AttachComponent = GetSoundAttachComponent();
	if (!Sound || !AttachComponent)
	{
		return;
	}

	// 다른 플레이어에게는 공격자 위치에 붙여 3D로 재생해, 멀어질수록 작아지게 한다.
	UGameplayStatics::SpawnSoundAttached(
		Sound,
		AttachComponent,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		false,
		1.f,
		1.f,
		0.f,
		MeleeAttackAttenuation);
}

USoundBase* UWeaponEffectsComponent::ResolveMeleeAttackSound(const bool bHeavyAttack) const
{
	return (bHeavyAttack && HeavyMeleeAttackSound) ? HeavyMeleeAttackSound.Get() : MeleeAttackSound.Get();
}

bool UWeaponEffectsComponent::IsOwnerLocallyControlled() const
{
	const AWeaponBase* Weapon = GetWeaponOwner();
	const APawn* OwningPawn = Weapon ? Cast<APawn>(Weapon->GetOwner()) : nullptr;
	return OwningPawn && OwningPawn->IsLocallyControlled();
}

USceneComponent* UWeaponEffectsComponent::GetSoundAttachComponent() const
{
	if (!GetOwner())
	{
		return nullptr;
	}

	// 무기 액터 루트는 캐릭터에 부착되지 않을 수 있으므로(메쉬만 소켓에 부착), 무기를 든
	// 캐릭터의 루트에 붙여 소리가 플레이어를 따라가게 한다.
	const AActor* WeaponHolder = GetOwner()->GetOwner();
	return IsValid(WeaponHolder) && WeaponHolder->GetRootComponent()
		? WeaponHolder->GetRootComponent()
		: GetOwner()->GetRootComponent();
}
