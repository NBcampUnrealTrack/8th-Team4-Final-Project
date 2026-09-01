// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Actor/RogueThrowableActor.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ARogueThrowableActor::ARogueThrowableActor()
{
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
	Movement->Bounciness    = 0.15f;
	Movement->Friction      = 0.8f;
	Movement->bRotationFollowsVelocity = false;
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

	Super::BeginPlay();
}

void ARogueThrowableActor::HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	HandleImpact();
}

void ARogueThrowableActor::HandleStop(const FHitResult& ImpactResult)
{
	HandleImpact();
}

void ARogueThrowableActor::HandleImpact()
{
	if (!HasAuthority() || bImpacted || !bStartFuseOnImpact)
	{
		return;
	}

	bImpacted = true;
	StartFuse();
}
