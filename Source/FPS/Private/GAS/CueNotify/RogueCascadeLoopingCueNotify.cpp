// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/CueNotify/RogueCascadeLoopingCueNotify.h"

#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"

bool ARogueCascadeLoopingCueNotify::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	const bool bResult = Super::WhileActive_Implementation(Target, Parameters);

	UWorld* World = IsValid(Target) ? Target->GetWorld() : GetWorld();
	if (!World)
	{
		return bResult;
	}

	StopSpawnedParticles();

	const FRotator Rotation = Parameters.Normal.IsNearlyZero() ? FRotator::ZeroRotator : Parameters.Normal.Rotation();
	USceneComponent* AttachTo = bAttachToTarget && IsValid(Target) ? Target->GetRootComponent() : nullptr;

	for (const FRogueCascadeCueEntry& Entry : Particles)
	{
		if (!Entry.Particle)
		{
			continue;
		}

		float Scale = Entry.Scale;
		if (Entry.bScaleByRadius && ReferenceRadius > 0.f && Parameters.RawMagnitude > 0.f)
		{
			Scale *= Parameters.RawMagnitude / ReferenceRadius;
		}

		const FVector ScaleVector(Scale);

		UParticleSystemComponent* Spawned = nullptr;
		if (AttachTo)
		{
			Spawned = UGameplayStatics::SpawnEmitterAttached(Entry.Particle, AttachTo, AttachSocket,
				Entry.Offset, Rotation, ScaleVector, EAttachLocation::KeepRelativeOffset, false);
		}
		else
		{
			Spawned = UGameplayStatics::SpawnEmitterAtLocation(World, Entry.Particle,
				Parameters.Location + Entry.Offset, Rotation, ScaleVector, false);
		}

		if (Spawned)
		{
			SpawnedParticles.Add(Spawned);
		}
	}

	return bResult;
}

bool ARogueCascadeLoopingCueNotify::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	StopSpawnedParticles();

	return Super::OnRemove_Implementation(Target, Parameters);
}

void ARogueCascadeLoopingCueNotify::StopSpawnedParticles()
{
	for (UParticleSystemComponent* Component : SpawnedParticles)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		// 남은 입자가 수명대로 사라진 뒤 자동 정리
		Component->bAutoDestroy = true;
		Component->DeactivateSystem();
	}

	SpawnedParticles.Reset();
}
