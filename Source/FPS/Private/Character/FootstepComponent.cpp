#include "Character/FootstepComponent.h"

#include "Audio/FootstepSoundDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

static constexpr float FootstepTraceDistance = 100.0f;

UFootstepComponent::UFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UFootstepComponent::PlayFootstep(USkeletalMeshComponent* SourceMesh, const FName FootSocketName)
{
	if (!IsValid(SourceMesh)
		|| !IsValid(FootstepSoundDefinition)
		|| GetNetMode() == NM_DedicatedServer)
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
		PlayFootstepSound(FootstepLocation, SurfaceType);
	}

	if (Owner->HasAuthority())
	{
		MulticastPlayFootstep(FootstepLocation, SurfaceType);
	}
	else
	{
		ServerPlayFootstep(FootSocketName);
	}
}

void UFootstepComponent::ServerPlayFootstep_Implementation(const FName FootSocketName)
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
		MulticastPlayFootstep(FootstepLocation, SurfaceType);
	}
}

void UFootstepComponent::MulticastPlayFootstep_Implementation(
	const FVector_NetQuantize FootstepLocation,
	const TEnumAsByte<EPhysicalSurface> SurfaceType)
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

	PlayFootstepSound(FootstepLocation, SurfaceType);
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
	const TEnumAsByte<EPhysicalSurface> SurfaceType) const
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
		FootstepSound->VolumeMultiplier,
		FootstepSound->PitchMultiplier);
}
