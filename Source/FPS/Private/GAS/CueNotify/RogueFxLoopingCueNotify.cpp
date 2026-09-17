// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/CueNotify/RogueFxLoopingCueNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Particles/ParticleSystemComponent.h"

bool ARogueFxLoopingCueNotify::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
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

		if (Entry.Niagara)
		{
			UNiagaraComponent* Spawned = nullptr;
			if (AttachTo)
			{
				Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(Entry.Niagara, AttachTo, AttachSocket,
					Entry.Offset, Rotation, EAttachLocation::KeepRelativeOffset, false, true);
			}
			else
			{
				Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Entry.Niagara,
					Parameters.Location + Entry.Offset, Rotation, ScaleVector, false, true);
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
				SpawnedNiagara.Add(Spawned);
			}
		}
	}

	ApplyOverlay(Target);

	return bResult;
}

bool ARogueFxLoopingCueNotify::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	ClearOverlay();
	StopSpawnedParticles();

	return Super::OnRemove_Implementation(Target, Parameters);
}

void ARogueFxLoopingCueNotify::StopSpawnedParticles()
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

	for (UNiagaraComponent* Component : SpawnedNiagara)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		Component->SetAutoDestroy(true);
		Component->Deactivate();
	}

	SpawnedNiagara.Reset();
}

void ARogueFxLoopingCueNotify::ApplyOverlay(AActor* Target)
{
	if (!OverlayMaterial || !IsValid(Target) || !Target->Implements<UPlayerInterface>())
	{
		return;
	}

	// 상대에게 보이는 정보라 3인칭 메시에만 입힌다
	USkeletalMeshComponent* Mesh = IPlayerInterface::Execute_GetMesh3P(Target);
	if (!IsValid(Mesh))
	{
		return;
	}

	Mesh->SetOverlayMaterial(OverlayMaterial);
	OverlayMesh = Mesh;
}

void ARogueFxLoopingCueNotify::ClearOverlay()
{
	if (USkeletalMeshComponent* Mesh = OverlayMesh.Get())
	{
		Mesh->SetOverlayMaterial(nullptr);
	}

	OverlayMesh.Reset();
}
