#include "Character/FootstepComponent.h"

#include "Audio/FootstepSoundDefinition.h"
#include "Audio/CharacterSoundPlayback.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Perception/AISense_Hearing.h"
#include "Player/DominationPlayerState.h"
#include "Sound/SoundBase.h"

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
	if (!IsValid(Owner) || !ShouldEmitMovementSound())
	{
		return;
	}

	const APawn* PawnOwner = Cast<APawn>(Owner);
	const bool bLocallyControlled = IsValid(PawnOwner) && PawnOwner->IsLocallyControlled();

	FVector FootstepLocation = FVector::ZeroVector;
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;
	if (!BuildFootstepData(SourceMesh, FootSocketName, FootstepLocation, SurfaceType))
	{
		return;
	}

	const float ResolvedVolumeMultiplier = ResolveFootstepVolumeMultiplier(VolumeMultiplier);

	if (bLocallyControlled && GetNetMode() != NM_DedicatedServer)
	{
		PlayFootstepSound(FootstepLocation, SurfaceType, ResolvedVolumeMultiplier, PitchMultiplier);
	}

	if (Owner->HasAuthority())
	{
		ReportFootstepNoise(FootstepLocation);
		MulticastPlayFootstep(FootstepLocation, SurfaceType, ResolvedVolumeMultiplier, PitchMultiplier);
	}
	else
	{
		// Send the original notify volume; the server applies its own stance once.
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
	if (!IsValid(Owner) || !IsValid(PawnOwner) || !ShouldEmitMovementSound())
	{
		return;
	}

	const EDominationCombatClass CombatClass = ResolveCombatClass();
	if (PawnOwner->IsLocallyControlled() && GetNetMode() != NM_DedicatedServer)
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

	const ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
	USkeletalMeshComponent* SourceMesh = IsValid(CharacterOwner)
		? CharacterOwner->GetMesh()
		: Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!IsValid(SourceMesh))
	{
		return;
	}

	FVector FootstepLocation = FVector::ZeroVector;
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;
	if (BuildFootstepData(SourceMesh, FootSocketName, FootstepLocation, SurfaceType))
	{
		const float ResolvedVolumeMultiplier = ResolveFootstepVolumeMultiplier(VolumeMultiplier);
		ReportFootstepNoise(FootstepLocation);
		MulticastPlayFootstep(FootstepLocation, SurfaceType, ResolvedVolumeMultiplier, PitchMultiplier);
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
	const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, FMath::Max(1.0f, FootstepTraceDistance));

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

bool UFootstepComponent::ShouldEmitMovementSound() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return false;
	}

	const APawn* PawnOwner = Cast<APawn>(Owner);
	// Players originate events on their owning client; AI originates on the server.
	return IsValid(PawnOwner)
		? PawnOwner->IsLocallyControlled() || (Owner->HasAuthority() && !PawnOwner->IsPlayerControlled())
		: Owner->HasAuthority();
}

void UFootstepComponent::ReportFootstepNoise(const FVector& FootstepLocation) const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	const ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
	const float Speed = IsValid(CharacterOwner) ? CharacterOwner->GetVelocity().Size2D() : 0.0f;
	const float WalkThreshold = FMath::Max(0.0f, WalkingNoiseSpeedThreshold);
	const float RunThreshold = FMath::Max(WalkThreshold, RunningNoiseSpeedThreshold);
	const float Loudness = Speed >= RunThreshold ? RunningFootstepNoiseLoudness
		: (Speed >= WalkThreshold ? WalkingFootstepNoiseLoudness : SlowFootstepNoiseLoudness);
	UAISense_Hearing::ReportNoiseEvent(
		Owner, FootstepLocation, FMath::Max(0.0f, Loudness), Owner,
		FMath::Max(0.0f, FootstepNoiseMaxRange), TEXT("Footstep"));
}

float UFootstepComponent::ResolveFootstepVolumeMultiplier(const float VolumeMultiplier) const
{
	const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner());
	const float StanceVolumeMultiplier = IsValid(CharacterOwner) && CharacterOwner->bIsCrouched
		? FMath::Clamp(CrouchFootstepVolumeMultiplier, 0.0f, 1.0f)
		: 1.0f;
	return FMath::Max(0.0f, VolumeMultiplier) * StanceVolumeMultiplier;
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

	CharacterSoundPlayback::PlayAtLocation(
		GetOwner(),
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
		CharacterSoundPlayback::PlayAtLocation(GetOwner(), JumpSound, GetOwner()->GetActorLocation());
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
	if (!IsValid(Owner) || !IsValid(PawnOwner) || !ShouldEmitMovementSound())
	{
		return;
	}

	if (PawnOwner->IsLocallyControlled() && GetNetMode() != NM_DedicatedServer)
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
		CharacterSoundPlayback::PlayAtLocation(GetOwner(), StanceSound, GetOwner()->GetActorLocation());
	}
}
