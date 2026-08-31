#include "Actor/DominationSpawnVolume.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"

namespace
{
	constexpr float SpawnSlotExtentRatio = 0.6f;
	const FVector2D SpawnSlotDirections[] =
	{
		FVector2D(1.0f, 1.0f),
		FVector2D(1.0f, -1.0f),
		FVector2D(-1.0f, 1.0f),
		FVector2D(-1.0f, -1.0f)
	};
}

ADominationSpawnVolume::ADominationSpawnVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnArea"));
	SpawnArea->SetupAttachment(SceneRoot);
	SpawnArea->SetBoxExtent(FVector(250.0f, 250.0f, 100.0f));
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetCanEverAffectNavigation(false);
}

void ADominationSpawnVolume::GetSpawnCandidateTransforms(
	TArray<FTransform>& OutCandidateTransforms) const
{
	OutCandidateTransforms.Reset(UE_ARRAY_COUNT(SpawnSlotDirections));
	if (!IsValid(SpawnArea))
	{
		return;
	}

	const FVector BoxExtent = SpawnArea->GetUnscaledBoxExtent();
	const FTransform& SpawnAreaTransform = SpawnArea->GetComponentTransform();
	for (const FVector2D& SlotDirection : SpawnSlotDirections)
	{
		const FVector LocalSpawnLocation(
			BoxExtent.X * SpawnSlotExtentRatio * SlotDirection.X,
			BoxExtent.Y * SpawnSlotExtentRatio * SlotDirection.Y,
			0.0f);
		OutCandidateTransforms.Emplace(
			SpawnAreaTransform.GetRotation(),
			SpawnAreaTransform.TransformPosition(LocalSpawnLocation),
			FVector::OneVector);
	}
}
