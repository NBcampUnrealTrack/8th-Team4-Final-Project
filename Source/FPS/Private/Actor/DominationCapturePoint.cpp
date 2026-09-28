#include "Actor/DominationCapturePoint.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"

ADominationCapturePoint::ADominationCapturePoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CaptureZone = CreateDefaultSubobject<UBoxComponent>(TEXT("CaptureZone"));
	CaptureZone->SetupAttachment(SceneRoot);
	CaptureZone->SetBoxExtent(CaptureBoxExtent);
	CaptureZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CaptureZone->SetCollisionObjectType(ECC_WorldDynamic);
	CaptureZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CaptureZone->SetGenerateOverlapEvents(false);
	CaptureZone->SetCanEverAffectNavigation(false);
	CaptureZone->OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandleCaptureZoneBeginOverlap);
	CaptureZone->OnComponentEndOverlap.AddDynamic(
		this,
		&ThisClass::HandleCaptureZoneEndOverlap);
}

void ADominationCapturePoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	CaptureZone->SetBoxExtent(CaptureBoxExtent);
}

EDominationPointId ADominationCapturePoint::GetPointId() const
{
	return PointId;
}

FVector ADominationCapturePoint::GetCaptureBoxExtent() const
{
	return CaptureBoxExtent;
}

bool ADominationCapturePoint::ContainsWorldLocation(const FVector& WorldLocation) const
{
	const FVector LocalLocation = CaptureZone->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector LocalExtent = CaptureZone->GetUnscaledBoxExtent();
	return FMath::Abs(LocalLocation.X) <= LocalExtent.X
		&& FMath::Abs(LocalLocation.Y) <= LocalExtent.Y
		&& FMath::Abs(LocalLocation.Z) <= LocalExtent.Z;
}

void ADominationCapturePoint::GetOverlappingPawns(TArray<APawn*>& OutPawns) const
{
	OutPawns.Reset();

	TArray<AActor*> OverlappingActors;
	CaptureZone->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (APawn* Pawn = Cast<APawn>(OverlappingActor))
		{
			OutPawns.Add(Pawn);
		}
	}
}

void ADominationCapturePoint::SetObjectiveActive(const bool bNewActive)
{
	bObjectiveActive = bNewActive;
	CaptureZone->SetGenerateOverlapEvents(bObjectiveActive);
	CaptureZone->SetCollisionEnabled(
		bObjectiveActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ADominationCapturePoint::HandleCaptureZoneBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		OnPawnEntered.Broadcast(Pawn);
	}
}

void ADominationCapturePoint::HandleCaptureZoneEndOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		OnPawnExited.Broadcast(Pawn);
	}
}
