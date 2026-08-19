#include "Actor/DataJackSiteVolume.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ADataJackSiteVolume::ADataJackSiteVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	SiteArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SiteArea"));
	SetRootComponent(SiteArea);
	SiteArea->SetBoxExtent(FVector(300.0f, 300.0f, 120.0f));
	SiteArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SiteArea->SetCollisionObjectType(ECC_WorldDynamic);
	SiteArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	SiteArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SiteArea->SetGenerateOverlapEvents(true);
	SiteArea->SetCanEverAffectNavigation(false);
}

FName ADataJackSiteVolume::GetSiteId() const
{
	return SiteId;
}

bool ADataJackSiteVolume::ContainsPawn(const APawn* Pawn) const
{
	return IsValid(Pawn) && ContainsWorldLocation(Pawn->GetActorLocation(), SiteBoundsTolerance);
}

bool ADataJackSiteVolume::FindPlantSurface(const APawn* Pawn, FHitResult& OutSurfaceHit) const
{
	OutSurfaceHit = FHitResult();
	if (!IsValid(Pawn) || GetWorld() == nullptr)
	{
		return false;
	}

	const FVector TraceOrigin = Pawn->GetActorLocation();
	const FVector TraceStart = TraceOrigin + FVector::UpVector * FloorTraceHeight;
	const FVector TraceEnd = TraceOrigin - FVector::UpVector * FloorTraceDepth;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DataJackPlantFloor), false, Pawn);
	QueryParams.AddIgnoredActor(this);
	if (!GetWorld()->LineTraceSingleByChannel(OutSurfaceHit, TraceStart, TraceEnd, PlantSurfaceTraceChannel, QueryParams)
		|| !OutSurfaceHit.bBlockingHit)
	{
		return false;
	}

	const FVector SurfaceNormal = OutSurfaceHit.ImpactNormal.GetSafeNormal();
	return ContainsWorldLocation(OutSurfaceHit.ImpactPoint, SiteBoundsTolerance)
		&& FVector::DotProduct(SurfaceNormal, FVector::UpVector) >= MinimumPlantSurfaceUpDot;
}

bool ADataJackSiteVolume::ContainsWorldLocation(const FVector& WorldLocation, const float Tolerance) const
{
	if (!IsValid(SiteArea))
	{
		return false;
	}

	const FVector LocalLocation = SiteArea->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const float ClampedTolerance = FMath::Max(0.0f, Tolerance);
	const FVector BoxExtent = SiteArea->GetUnscaledBoxExtent() + FVector::OneVector * ClampedTolerance;
	return FMath::Abs(LocalLocation.X) <= BoxExtent.X
		&& FMath::Abs(LocalLocation.Y) <= BoxExtent.Y
		&& FMath::Abs(LocalLocation.Z) <= BoxExtent.Z;
}
