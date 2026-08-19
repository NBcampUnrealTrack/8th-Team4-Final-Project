#include "Actor/DataJackWorldQueries.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Actor/DataJackSiteVolume.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

namespace DataJackWorldQueries
{
	bool CanPawnPlant(const APawn* PlayerPawn, const ADataJackSiteVolume* SiteVolume)
	{
		if (!IsValid(PlayerPawn) || !IsValid(SiteVolume) || !SiteVolume->ContainsPawn(PlayerPawn))
		{
			return false;
		}

		FHitResult PlantSurfaceHit;
		return SiteVolume->FindPlantSurface(PlayerPawn, PlantSurfaceHit);
	}

	bool CanPawnDefuse(
		UWorld* World,
		const AActor& DataJackActor,
		const FVector& InteractionLocation,
		const APawn* PlayerPawn,
		const FDefuseSettings& Settings)
	{
		if (!IsValid(PlayerPawn) || !IsValid(World))
		{
			return false;
		}

		if (FVector::DistSquared(PlayerPawn->GetActorLocation(), InteractionLocation) > FMath::Square(Settings.Radius))
		{
			return false;
		}

		if (!Settings.bRequireLineOfSight)
		{
			return true;
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DataJackDefuseVisibility), false, PlayerPawn);
		QueryParams.AddIgnoredActor(&DataJackActor);

		FHitResult VisibilityHit;
		return !World->LineTraceSingleByChannel(
			VisibilityHit,
			PlayerPawn->GetPawnViewLocation(),
			InteractionLocation,
			Settings.TraceChannel,
			QueryParams);
	}

	FTransform BuildDropTransform(
		UWorld* World,
		const AActor& DataJackActor,
		const UStaticMeshComponent* DataJackMesh,
		const APawn* SourceCarrierPawn,
		const FDropSettings& DropSettings,
		const FGroundingSettings& GroundingSettings)
	{
		FVector DropLocation = IsValid(SourceCarrierPawn)
			? SourceCarrierPawn->GetActorLocation()
			: DataJackActor.GetActorLocation();
		const FRotator DropRotation = IsValid(SourceCarrierPawn)
			? SourceCarrierPawn->GetActorRotation()
			: DataJackActor.GetActorRotation();

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DataJackDrop), false, &DataJackActor);
		if (IsValid(SourceCarrierPawn))
		{
			QueryParams.AddIgnoredActor(SourceCarrierPawn);
			const FVector DesiredDropLocation = DropLocation
				+ SourceCarrierPawn->GetActorForwardVector() * DropSettings.ForwardDistance;
			FHitResult ObstacleHit;
			if (World != nullptr
				&& World->LineTraceSingleByChannel(
					ObstacleHit,
					DropLocation,
					DesiredDropLocation,
					DropSettings.TraceChannel,
					QueryParams))
			{
				DropLocation = ObstacleHit.ImpactPoint
					+ ObstacleHit.ImpactNormal * DropSettings.ObstacleClearance;
			}
			else
			{
				DropLocation = DesiredDropLocation;
			}
		}

		const FVector TraceStart = DropLocation + FVector::UpVector * DropSettings.FloorTraceHeight;
		const FVector TraceEnd = DropLocation - FVector::UpVector * DropSettings.FloorTraceDepth;
		FHitResult FloorHit;
		if (World != nullptr
			&& World->LineTraceSingleByChannel(
				FloorHit,
				TraceStart,
				TraceEnd,
				DropSettings.TraceChannel,
				QueryParams))
		{
			return BuildGroundedTransform(
				DataJackActor,
				DataJackMesh,
				FloorHit,
				SourceCarrierPawn,
				GroundingSettings);
		}

		return FTransform(
			FRotator(0.0f, DropRotation.Yaw, 0.0f),
			DropLocation,
			DataJackActor.GetActorScale3D());
	}

	FTransform BuildGroundedTransform(
		const AActor& DataJackActor,
		const UStaticMeshComponent* DataJackMesh,
		const FHitResult& SurfaceHit,
		const APawn* OrientationSource,
		const FGroundingSettings& Settings)
	{
		FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
		if (SurfaceNormal.IsNearlyZero())
		{
			SurfaceNormal = FVector::UpVector;
		}

		FVector Forward = IsValid(OrientationSource)
			? OrientationSource->GetActorForwardVector()
			: DataJackActor.GetActorForwardVector();
		Forward = FVector::VectorPlaneProject(Forward, SurfaceNormal).GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			Forward = FVector::VectorPlaneProject(FVector::ForwardVector, SurfaceNormal).GetSafeNormal();
		}
		if (Forward.IsNearlyZero())
		{
			Forward = FVector::CrossProduct(FVector::RightVector, SurfaceNormal).GetSafeNormal();
		}

		FTransform GroundedTransform(
			FRotationMatrix::MakeFromXZ(Forward, SurfaceNormal).Rotator(),
			SurfaceHit.ImpactPoint,
			DataJackActor.GetActorScale3D());

		const UStaticMesh* StaticMesh = IsValid(DataJackMesh) ? DataJackMesh->GetStaticMesh() : nullptr;
		if (!IsValid(StaticMesh))
		{
			GroundedTransform.AddToTranslation(SurfaceNormal * Settings.SurfaceClearance);
			return GroundedTransform;
		}

		const FBoxSphereBounds LocalBounds = StaticMesh->GetBounds();
		const FVector BoundsMin = LocalBounds.Origin - LocalBounds.BoxExtent;
		const FVector BoundsMax = LocalBounds.Origin + LocalBounds.BoxExtent;
		const FVector Corners[8] =
		{
			FVector(BoundsMin.X, BoundsMin.Y, BoundsMin.Z),
			FVector(BoundsMin.X, BoundsMin.Y, BoundsMax.Z),
			FVector(BoundsMin.X, BoundsMax.Y, BoundsMin.Z),
			FVector(BoundsMin.X, BoundsMax.Y, BoundsMax.Z),
			FVector(BoundsMax.X, BoundsMin.Y, BoundsMin.Z),
			FVector(BoundsMax.X, BoundsMin.Y, BoundsMax.Z),
			FVector(BoundsMax.X, BoundsMax.Y, BoundsMin.Z),
			FVector(BoundsMax.X, BoundsMax.Y, BoundsMax.Z)
		};

		const FTransform MeshRelativeTransform = DataJackMesh->GetRelativeTransform();
		float MinimumSurfaceDistance = BIG_NUMBER;
		for (const FVector& Corner : Corners)
		{
			const FVector ActorSpaceCorner = MeshRelativeTransform.TransformPosition(Corner);
			const FVector WorldCorner = GroundedTransform.TransformPosition(ActorSpaceCorner);
			MinimumSurfaceDistance = FMath::Min(
				MinimumSurfaceDistance,
				FVector::DotProduct(WorldCorner - SurfaceHit.ImpactPoint, SurfaceNormal));
		}

		GroundedTransform.AddToTranslation(
			SurfaceNormal * (Settings.SurfaceClearance - MinimumSurfaceDistance));
		return GroundedTransform;
	}
}
