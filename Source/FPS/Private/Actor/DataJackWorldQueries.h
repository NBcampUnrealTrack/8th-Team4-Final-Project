#pragma once

#include "CoreMinimal.h"

class AActor;
class ADataJackSiteVolume;
class APawn;
class UStaticMeshComponent;
class UWorld;

namespace DataJackWorldQueries
{
	// DataJack 상태를 소유하거나 변경하지 않는 월드 질의와 배치 계산만 담당한다.
	struct FDefuseSettings
	{
		float Radius = 200.0f;
		bool bRequireLineOfSight = true;
		ECollisionChannel TraceChannel = ECC_Visibility;
	};

	struct FDropSettings
	{
		float ForwardDistance = 100.0f;
		float ObstacleClearance = 15.0f;
		float FloorTraceHeight = 100.0f;
		float FloorTraceDepth = 400.0f;
		ECollisionChannel TraceChannel = ECC_Visibility;
	};

	struct FGroundingSettings
	{
		float SurfaceClearance = 1.0f;
	};

	bool CanPawnPlant(
		const APawn* PlayerPawn,
		const ADataJackSiteVolume* SiteVolume);

	bool CanPawnDefuse(
		UWorld* World,
		const AActor& DataJackActor,
		const FVector& InteractionLocation,
		const APawn* PlayerPawn,
		const FDefuseSettings& Settings);

	FTransform BuildDropTransform(
		UWorld* World,
		const AActor& DataJackActor,
		const UStaticMeshComponent* DataJackMesh,
		const APawn* SourceCarrierPawn,
		const FDropSettings& DropSettings,
		const FGroundingSettings& GroundingSettings);

	FTransform BuildGroundedTransform(
		const AActor& DataJackActor,
		const UStaticMeshComponent* DataJackMesh,
		const FHitResult& SurfaceHit,
		const APawn* OrientationSource,
		const FGroundingSettings& Settings);
}
