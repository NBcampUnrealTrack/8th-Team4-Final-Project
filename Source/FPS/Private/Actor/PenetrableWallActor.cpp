#include "Actor/PenetrableWallActor.h"

#include "Components/StaticMeshComponent.h"

APenetrableWallActor::APenetrableWallActor()
{
	PrimaryActorTick.bCanEverTick = false;

	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	SetRootComponent(WallMesh);

	WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallMesh->SetCollisionResponseToAllChannels(ECR_Block);
	WallMesh->SetGenerateOverlapEvents(false);
}

bool APenetrableWallActor::CanPenetrate_Implementation() const
{
	return bCanPenetrate;
}

float APenetrableWallActor::GetPenetrationDamageMultiplier_Implementation() const
{
	return FMath::Clamp(PenetrationDamageMultiplier, 0.0f, 1.0f);
}
