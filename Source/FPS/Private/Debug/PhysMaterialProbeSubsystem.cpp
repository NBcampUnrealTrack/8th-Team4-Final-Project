#include "Debug/PhysMaterialProbeSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPMProbe(
		TEXT("PM.Probe"),
		0,
		TEXT("Show the physical material under the crosshair. 0: off, 1: on"));

	TAutoConsoleVariable<float> CVarPMProbeDistance(
		TEXT("PM.Probe.Distance"),
		10000.0f,
		TEXT("Trace distance for PM.Probe"));

	constexpr uint64 SimpleMessageKey = 0x504D50524F424501;
	constexpr uint64 ComplexMessageKey = 0x504D50524F424502;

	bool IsDefaultSurface(const FHitResult& Hit)
	{
		return !Hit.PhysMaterial.IsValid()
			|| UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()) == SurfaceType_Default;
	}
}

void UPhysMaterialProbeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() || !GEngine)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * CVarPMProbeDistance.GetValueOnGameThread();
	const AActor* IgnoredActor = PlayerController->GetPawn();

	FHitResult SimpleHit;
	if (TraceFromView(ViewLocation, TraceEnd, false, IgnoredActor, SimpleHit))
	{
		const bool bDefault = IsDefaultSurface(SimpleHit);
		const FColor Color = bDefault ? FColor::Red : FColor::Green;
		DrawDebugPoint(World, SimpleHit.ImpactPoint, 12.0f, Color, false, -1.0f, SDPG_Foreground);
		GEngine->AddOnScreenDebugMessage(SimpleMessageKey, 0.0f, Color, DescribeHit(SimpleHit, false));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(SimpleMessageKey, 0.0f, FColor::White, TEXT("[Simple] No hit"));
	}

	FHitResult ComplexHit;
	if (TraceFromView(ViewLocation, TraceEnd, true, IgnoredActor, ComplexHit))
	{
		const bool bDefault = IsDefaultSurface(ComplexHit);
		const FColor Color = bDefault ? FColor::Orange : FColor::Cyan;
		DrawDebugPoint(World, ComplexHit.ImpactPoint, 6.0f, Color, false, -1.0f, SDPG_Foreground);
		GEngine->AddOnScreenDebugMessage(ComplexMessageKey, 0.0f, Color, DescribeHit(ComplexHit, true));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(ComplexMessageKey, 0.0f, FColor::White, TEXT("[Complex] No hit"));
	}
}

TStatId UPhysMaterialProbeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPhysMaterialProbeSubsystem, STATGROUP_Tickables);
}

bool UPhysMaterialProbeSubsystem::IsTickable() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return CVarPMProbe.GetValueOnGameThread() > 0;
#endif
}

bool UPhysMaterialProbeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UPhysMaterialProbeSubsystem::TraceFromView(
	const FVector& Start,
	const FVector& End,
	const bool bTraceComplex,
	const AActor* IgnoredActor,
	FHitResult& OutHit) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysMaterialProbe), bTraceComplex, IgnoredActor);
	QueryParams.bReturnPhysicalMaterial = true;
	QueryParams.bReturnFaceIndex = bTraceComplex;

	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);
}

FString UPhysMaterialProbeSubsystem::DescribeHit(const FHitResult& Hit, const bool bTraceComplex)
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	const UMaterialInterface* Material = nullptr;
	if (IsValid(Component))
	{
		if (bTraceComplex && Hit.FaceIndex != INDEX_NONE)
		{
			int32 SectionIndex = INDEX_NONE;
			Material = Component->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, SectionIndex);
		}
		else
		{
			Material = Component->GetMaterial(0);
		}
	}

	const UPhysicalMaterial* PhysMaterial = Hit.PhysMaterial.Get();
	return FString::Printf(TEXT("[%s] Actor=%s Comp=%s %s=%s PM=%s Surface=%s"),
		bTraceComplex ? TEXT("Complex") : TEXT("Simple"),
		IsValid(Hit.GetActor()) ? *Hit.GetActor()->GetActorNameOrLabel() : TEXT("None"),
		*GetNameSafe(Component),
		bTraceComplex ? TEXT("Mat") : TEXT("Mat0"),
		*GetNameSafe(Material),
		*GetNameSafe(PhysMaterial),
		*GetSurfaceName(UPhysicalMaterial::DetermineSurfaceType(PhysMaterial)));
}

FString UPhysMaterialProbeSubsystem::GetSurfaceName(const EPhysicalSurface SurfaceType)
{
	if (SurfaceType == SurfaceType_Default)
	{
		return TEXT("Default");
	}

	for (const FPhysicalSurfaceName& SurfaceName : UPhysicsSettings::Get()->PhysicalSurfaces)
	{
		if (SurfaceName.Type == SurfaceType)
		{
			return SurfaceName.Name.ToString();
		}
	}

	return FString::Printf(TEXT("SurfaceType%d"), static_cast<int32>(SurfaceType));
}
