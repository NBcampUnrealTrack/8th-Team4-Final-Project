// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Actor/RogueThrowableActor.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ARogueThrowableActor::ARogueThrowableActor()
{
	// 회전을 속도에 맞추기 위해 틱 활성화
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	SetRootComponent(Collision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);

	Movement->InitialSpeed = 2000.f;
	Movement->MaxSpeed     = 3000.f;
	Movement->ProjectileGravityScale = 0.6f;
	Movement->bShouldBounce = true;
	Movement->Bounciness    = 0.3f;
	Movement->Friction      = 0.35f;
	Movement->bRotationFollowsVelocity = false;
	Movement->bBounceAngleAffectsFriction = true;
	Movement->MinFrictionFraction = 0.4f;
	Movement->BounceVelocityStopSimulatingThreshold = 20.f;

	Spin = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spin"));
	Spin->SetUpdatedComponent(Collision);
	Spin->RotationRate = FRotator(180.f, 360.f, 90.f);
}

bool ARogueThrowableActor::ShouldStartFuseOnBeginPlay() const
{
	return Super::ShouldStartFuseOnBeginPlay() && !bStartFuseOnImpact;
}

void ARogueThrowableActor::BeginPlay()
{
	if (AActor* Thrower = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Thrower, true);

		// 던진 사람의 이동속도를 더함
		Movement->Velocity += Thrower->GetVelocity() * InheritVelocityScale;
	}

	Movement->OnProjectileBounce.AddDynamic(this, &ThisClass::HandleBounce);
	Movement->OnProjectileStop.AddDynamic(this, &ThisClass::HandleStop);

	BaseSpinRate = Spin->RotationRate;

	Super::BeginPlay();
}

void ARogueThrowableActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Reference = Movement->InitialSpeed;
	const float Ratio = Reference > 0.f
		? FMath::Clamp(Movement->Velocity.Size() / Reference, 0.f, 1.f)
		: 0.f;

	Spin->RotationRate = BaseSpinRate * Ratio;
}

void ARogueThrowableActor::HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	const float ImpactSpeed = ImpactVelocity.Size();

	if (BounceSound && ImpactSpeed >= MinBounceSoundSpeed)
	{
		const float Reference = FMath::Max(Movement->InitialSpeed, 1.f);
		const float Volume = FMath::Clamp(ImpactSpeed / Reference, 0.2f, 1.f);

		UGameplayStatics::PlaySoundAtLocation(this, BounceSound, ImpactResult.ImpactPoint, Volume);
	}

	OnBounced(ImpactResult, ImpactSpeed);

	HandleImpact();
}

void ARogueThrowableActor::HandleStop(const FHitResult& ImpactResult)
{
	HandleImpact();
}

void ARogueThrowableActor::HandleImpact()
{
	if (bImpacted)
	{
		return;
	}

	bImpacted = true;

	if (HasAuthority() && bStartFuseOnImpact)
	{
		StartFuse();
	}
}
