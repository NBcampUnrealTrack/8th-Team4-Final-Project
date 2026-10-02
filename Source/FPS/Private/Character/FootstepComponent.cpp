#include "Character/FootstepComponent.h"

#include "Audio/FootstepSoundDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Perception/AISense_Hearing.h"
#include "Player/DominationPlayerState.h"

static constexpr float FootstepTraceDistance = 100.0f;

namespace
{
	void ReportFootstepNoise(const AActor& Owner, const FVector& Location)
	{
		const ACharacter* Character = Cast<ACharacter>(&Owner);
		const float Speed = Character ? Character->GetVelocity().Size2D() : 0.0f;
		const float Loudness = Speed >= 500.0f ? 1.0f : (Speed >= 250.0f ? 0.7f : 0.45f);
		UAISense_Hearing::ReportNoiseEvent(
			const_cast<AActor*>(&Owner), Location, Loudness, const_cast<AActor*>(&Owner), 1800.0f, TEXT("Footstep"));
	}
}

UFootstepComponent::UFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UFootstepComponent::PlayFootstep(
	USkeletalMeshComponent* SourceMesh,
	const FName FootSocketName,
	const float VolumeMultiplier,
	const float PitchMultiplier)
{
	if (!IsValid(SourceMesh)
		|| !IsValid(FootstepSoundDefinition))
	{
		return;
	}

	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	const APawn* PawnOwner = Cast<APawn>(Owner);
	const bool bLocallyControlled = IsValid(PawnOwner) && PawnOwner->IsLocallyControlled();

	if (!bLocallyControlled && !Owner->HasAuthority())
	{
		return;
	}

	FVector FootstepLocation = FVector::ZeroVector;
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;
	if (!BuildFootstepData(SourceMesh, FootSocketName, FootstepLocation, SurfaceType))
	{
		return;
	}

	if (bLocallyControlled)
	{
		PlayFootstepSound(FootstepLocation, SurfaceType, VolumeMultiplier, PitchMultiplier);
	}

	if (Owner->HasAuthority())
	{
		ReportFootstepNoise(*Owner, FootstepLocation);
		MulticastPlayFootstep(FootstepLocation, SurfaceType, VolumeMultiplier, PitchMultiplier);
	}
	else
	{
		ServerPlayFootstep(FootSocketName, VolumeMultiplier, PitchMultiplier);
	}
}

void UFootstepComponent::PlayJumpSound()
{
	if (!IsValid(FootstepSoundDefinition))
	{
		return;
	}

	AActor* Owner = GetOwner();
	APawn* PawnOwner = Cast<APawn>(Owner);
	if (!IsValid(Owner) || !IsValid(PawnOwner))
	{
		return;
	}

	const EDominationCombatClass CombatClass = ResolveCombatClass();
	if (PawnOwner->IsLocallyControlled())
	{
		PlayJumpSoundAtOwner(CombatClass);
	}

	if (Owner->HasAuthority())
	{
		MulticastPlayJumpSound(CombatClass);
	}
	else
	{
		ServerPlayJumpSound();
	}
}

void UFootstepComponent::PlayCrouchSound()
{
	PlayStanceSound(true);
}

void UFootstepComponent::PlayStandSound()
{
	PlayStanceSound(false);
}

void UFootstepComponent::ServerPlayFootstep_Implementation(
	const FName FootSocketName,
	const float VolumeMultiplier,
	const float PitchMultiplier)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	USkeletalMeshComponent* SourceMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!IsValid(SourceMesh))
	{
		return;
	}

	FVector FootstepLocation = FVector::ZeroVector;
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;
	if (BuildFootstepData(SourceMesh, FootSocketName, FootstepLocation, SurfaceType))
	{
		ReportFootstepNoise(*Owner, FootstepLocation);
		MulticastPlayFootstep(FootstepLocation, SurfaceType, VolumeMultiplier, PitchMultiplier);
	}
}

void UFootstepComponent::MulticastPlayFootstep_Implementation(
	const FVector_NetQuantize FootstepLocation,
	const EPhysicalSurface SurfaceType,
	const float VolumeMultiplier,
	const float PitchMultiplier)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (IsValid(PawnOwner) && PawnOwner->IsLocallyControlled())
	{
		return;
	}

	PlayFootstepSound(FootstepLocation, SurfaceType, VolumeMultiplier, PitchMultiplier);
}

void UFootstepComponent::ServerPlayJumpSound_Implementation()
{
	if (IsValid(GetOwner()))
	{
		MulticastPlayJumpSound(ResolveCombatClass());
	}
}

void UFootstepComponent::MulticastPlayJumpSound_Implementation(const EDominationCombatClass CombatClass)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (IsValid(PawnOwner) && PawnOwner->IsLocallyControlled())
	{
		return;
	}

	PlayJumpSoundAtOwner(CombatClass);
}

void UFootstepComponent::ServerPlayStanceSound_Implementation(const bool bEnteringCrouch)
{
	if (IsValid(GetOwner()))
	{
		MulticastPlayStanceSound(bEnteringCrouch);
	}
}

void UFootstepComponent::MulticastPlayStanceSound_Implementation(const bool bEnteringCrouch)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (IsValid(PawnOwner) && PawnOwner->IsLocallyControlled())
	{
		return;
	}

	PlayStanceSoundAtOwner(bEnteringCrouch);
}

bool UFootstepComponent::BuildFootstepData(
	USkeletalMeshComponent* SourceMesh,
	const FName FootSocketName,
	FVector& OutFootstepLocation,
	TEnumAsByte<EPhysicalSurface>& OutSurfaceType) const
{
	if (!IsValid(SourceMesh) || !IsValid(FootstepSoundDefinition))
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}

	const ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
	if (IsValid(CharacterOwner)
		&& IsValid(CharacterOwner->GetCharacterMovement())
		&& CharacterOwner->GetCharacterMovement()->IsFalling())
	{
		return false;
	}

	const FVector TraceStart = SourceMesh->DoesSocketExist(FootSocketName)
		? SourceMesh->GetSocketLocation(FootSocketName)
		: Owner->GetActorLocation();
	const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, FootstepTraceDistance);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FootstepTrace), false, Owner);
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult HitResult;
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		return false;
	}

	OutFootstepLocation = HitResult.ImpactPoint;
	OutSurfaceType =
		UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());

	return true;
}

void UFootstepComponent::PlayFootstepSound(
	const FVector FootstepLocation,
	const TEnumAsByte<EPhysicalSurface> SurfaceType,
	const float VolumeMultiplier,
	const float PitchMultiplier) const
{
	if (!IsValid(FootstepSoundDefinition))
	{
		return;
	}

	const FFootstepSurfaceSound* FootstepSound =
		FootstepSoundDefinition->ResolveFootstepSound(SurfaceType);
	if (FootstepSound == nullptr || !FootstepSound->Sound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		FootstepSound->Sound,
		FootstepLocation,
		FootstepSound->VolumeMultiplier * FMath::Max(0.0f, VolumeMultiplier),
		FootstepSound->PitchMultiplier * FMath::Max(0.0f, PitchMultiplier));
}

EDominationCombatClass UFootstepComponent::ResolveCombatClass() const
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	const ADominationPlayerState* PlayerState = IsValid(PawnOwner)
		? PawnOwner->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	return IsValid(PlayerState) ? PlayerState->GetCombatClass() : EDominationCombatClass::Unselected;
}

void UFootstepComponent::PlayJumpSoundAtOwner(const EDominationCombatClass CombatClass) const
{
	if (!IsValid(FootstepSoundDefinition) || !IsValid(GetOwner()))
	{
		return;
	}

	if (USoundBase* JumpSound = FootstepSoundDefinition->ResolveJumpSound(CombatClass))
	{
		UGameplayStatics::PlaySoundAtLocation(this, JumpSound, GetOwner()->GetActorLocation());
	}
}

void UFootstepComponent::PlayStanceSound(const bool bEnteringCrouch)
{
	if (!IsValid(FootstepSoundDefinition))
	{
		return;
	}

	AActor* Owner = GetOwner();
	APawn* PawnOwner = Cast<APawn>(Owner);
	if (!IsValid(Owner) || !IsValid(PawnOwner))
	{
		return;
	}

	if (PawnOwner->IsLocallyControlled())
	{
		PlayStanceSoundAtOwner(bEnteringCrouch);
	}

	if (Owner->HasAuthority())
	{
		MulticastPlayStanceSound(bEnteringCrouch);
	}
	else
	{
		ServerPlayStanceSound(bEnteringCrouch);
	}
}

void UFootstepComponent::PlayStanceSoundAtOwner(const bool bEnteringCrouch) const
{
	if (!IsValid(FootstepSoundDefinition) || !IsValid(GetOwner()))
	{
		return;
	}

	USoundBase* StanceSound = bEnteringCrouch
		? FootstepSoundDefinition->ResolveCrouchSound()
		: FootstepSoundDefinition->ResolveStandSound();
	if (IsValid(StanceSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, StanceSound, GetOwner()->GetActorLocation());
	}
}
