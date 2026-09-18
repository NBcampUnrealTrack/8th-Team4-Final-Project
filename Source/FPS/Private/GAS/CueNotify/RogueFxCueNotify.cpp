// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/CueNotify/RogueFxCueNotify.h"

#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Particles/ParticleSystem.h"

bool URogueFxCueNotify::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	const bool bResult = Super::OnExecute_Implementation(Target, Parameters);

	UWorld* World = IsValid(Target) ? Target->GetWorld() : nullptr;
	if (!World)
	{
		return bResult;
	}

	// Normal 이 방향을 담고 있으면 그 방향으로 정렬
	const FRotator Rotation = Parameters.Normal.IsNearlyZero()
		? FRotator::ZeroRotator
		: Parameters.Normal.Rotation();

	USceneComponent* AttachTo = bAttachToTarget ? Target->GetRootComponent() : nullptr;

	for (const FRogueFxCueEntry& Entry : Particles)
	{
		if (!Entry.Particle && !Entry.Niagara)
		{
			continue;
		}

		float Scale = Entry.Scale;
		if (Entry.bScaleByRadius && ReferenceRadius > 0.f && Parameters.RawMagnitude > 0.f)
		{
			Scale *= Parameters.RawMagnitude / ReferenceRadius;
		}
		const FVector ScaleVector(Scale);

		if (Entry.Particle)
		{
			if (AttachTo)
			{
				UGameplayStatics::SpawnEmitterAttached(
					Entry.Particle, AttachTo, AttachSocket,
					Entry.Offset, Rotation, ScaleVector,
					EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ true);
			}
			else
			{
				UGameplayStatics::SpawnEmitterAtLocation(
					World, Entry.Particle,
					Parameters.Location + Entry.Offset, Rotation, ScaleVector,
					/*bAutoDestroy*/ true);
			}
		}

		if (Entry.Niagara)
		{
			UNiagaraComponent* Spawned = nullptr;
			if (AttachTo)
			{
				Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
					Entry.Niagara, AttachTo, AttachSocket,
					Entry.Offset, Rotation, EAttachLocation::KeepRelativeOffset,
					/*bAutoDestroy*/ true, /*bAutoActivate*/ true);
			}
			else
			{
				Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					World, Entry.Niagara,
					Parameters.Location + Entry.Offset, Rotation, ScaleVector,
					/*bAutoDestroy*/ true, /*bAutoActivate*/ true);
			}

			if (Spawned)
			{
				if (AttachTo)
				{
					Spawned->SetRelativeScale3D(ScaleVector);
				}
				if (Entry.bOverrideColor)
				{
					Spawned->SetVariableLinearColor(TEXT("User.Color"), Entry.Color);
				}
			}
		}
	}

	return bResult;
}
