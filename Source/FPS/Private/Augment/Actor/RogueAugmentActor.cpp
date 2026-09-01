// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Actor/RogueAugmentActor.h"

#include "Engine/World.h"
#include "TimerManager.h"

ARogueAugmentActor::ARogueAugmentActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
}

void ARogueAugmentActor::InitializePayload(const FRogueEffectPayload& InPayload)
{
	Payload = InPayload;
}

void ARogueAugmentActor::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (LifeSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(LifeTimer, this, &ThisClass::K2_DestroyActor, LifeSeconds, false);
	}

	if (ShouldStartFuseOnBeginPlay())
	{
		StartFuse();
	}
}

void ARogueAugmentActor::StartFuse()
{
	if (!HasAuthority())
	{
		return;
	}

	if (DetonateInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			FuseTimer, this, &ThisClass::Detonate, DetonateInterval, true, FuseSeconds);
		return;
	}

	if (FuseSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(FuseTimer, this, &ThisClass::Detonate, FuseSeconds, false);
		return;
	}

	Detonate();
}

void ARogueAugmentActor::Detonate()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bRepeating = DetonateInterval > 0.f;

	if (!bRepeating && bDetonated)
	{
		return;
	}

	bDetonated = true;

	Payload.Origin = GetActorLocation();
	Payload.Causer = this;

	URogueEffectApplier::Apply(Payload);

	Multicast_Detonated();

	if (bRepeating)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(FuseTimer);

	if (bDestroyOnDetonate)
	{
		Destroy();
	}
}

void ARogueAugmentActor::Multicast_Detonated_Implementation()
{
	OnDetonated();
}
