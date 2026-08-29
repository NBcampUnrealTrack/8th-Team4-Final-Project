#include "Actor/DominationCapturePoint.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"

ADominationCapturePoint::ADominationCapturePoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CaptureZone = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureZone"));
	CaptureZone->SetupAttachment(SceneRoot);
	CaptureZone->SetSphereRadius(CaptureRadius);
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

	CaptureZone->SetSphereRadius(CaptureRadius);
}

EDominationPointId ADominationCapturePoint::GetPointId() const
{
	return PointId;
}

float ADominationCapturePoint::GetCaptureRadius() const
{
	return CaptureRadius;
}

bool ADominationCapturePoint::ContainsWorldLocation(const FVector& WorldLocation) const
{
	return FVector::DistSquared(CaptureZone->GetComponentLocation(), WorldLocation)
		<= FMath::Square(CaptureZone->GetScaledSphereRadius());
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
