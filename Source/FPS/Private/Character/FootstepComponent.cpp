#include "Character/FootstepComponent.h"

#include "Audio/FootstepSoundDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
	constexpr float FootstepTraceDistance = 100.0f;
}

UFootstepComponent::UFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFootstepComponent::PlayFootstep(USkeletalMeshComponent* SourceMesh, const FName FootSocketName) const
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

	const ACharacter* CharacterOwner = Cast<ACharacter>(Owner);
	if (IsValid(CharacterOwner)
		&& IsValid(CharacterOwner->GetCharacterMovement())
		&& CharacterOwner->GetCharacterMovement()->IsFalling())
	{
		return;
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
		return;
	}

	const TEnumAsByte<EPhysicalSurface> SurfaceType =
		UPhysicalMaterial::DetermineSurfaceType(HitResult.PhysMaterial.Get());
	const FFootstepSurfaceSound* FootstepSound =
		FootstepSoundDefinition->ResolveFootstepSound(SurfaceType);
	if (FootstepSound == nullptr || !FootstepSound->Sound)
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		FootstepSound->Sound,
		HitResult.ImpactPoint,
		FootstepSound->VolumeMultiplier,
		FootstepSound->PitchMultiplier);
}
