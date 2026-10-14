// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/CueNotify/RogueFxCueNotify.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Weapon/WeaponBase.h"

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

	if (bAttachToWeapon)
	{
		SpawnOnWeapon(Target, Rotation);
		return bResult;
	}

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
		const FRotator EntryRotation = Entry.bIgnoreCueNormal ? FRotator::ZeroRotator : Rotation;

		if (Entry.Particle)
		{
			if (AttachTo)
			{
				UGameplayStatics::SpawnEmitterAttached(
					Entry.Particle, AttachTo, AttachSocket,
					Entry.Offset, EntryRotation, ScaleVector,
					EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ true);
			}
			else
			{
				UGameplayStatics::SpawnEmitterAtLocation(
					World, Entry.Particle,
					Parameters.Location + Entry.Offset, EntryRotation, ScaleVector,
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
					Entry.Offset, EntryRotation, EAttachLocation::KeepRelativeOffset,
					/*bAutoDestroy*/ true, /*bAutoActivate*/ true);
			}
			else
			{
				Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					World, Entry.Niagara,
					Parameters.Location + Entry.Offset, EntryRotation, ScaleVector,
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

void URogueFxCueNotify::SpawnOnWeapon(AActor* Target, const FRotator& Rotation) const
{
	AWeaponBase* Weapon = IsValid(Target) && Target->Implements<UPlayerInterface>()
		? IPlayerInterface::Execute_GetCurrentWeapon(Target)
		: nullptr;
	if (!IsValid(Weapon))
	{
		return;
	}

	USkeletalMeshComponent* Meshes[] = { Weapon->GetMesh1P(), Weapon->GetMesh3P() };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Meshes); ++Index)
	{
		USkeletalMeshComponent* Mesh = Meshes[Index];
		if (!IsValid(Mesh))
		{
			continue;
		}

		// 1인칭은 본인만, 3인칭은 본인 제외
		const bool bFirstPerson = (Index == 0);

		for (const FRogueFxCueEntry& Entry : Particles)
		{
			const FVector ScaleVector(Entry.Scale);

			if (Entry.Particle)
			{
				if (UParticleSystemComponent* Spawned = UGameplayStatics::SpawnEmitterAttached(Entry.Particle, Mesh,
						WeaponSocket, Entry.Offset, FRotator::ZeroRotator, ScaleVector,
						EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ true))
				{
					Spawned->SetWorldRotation(Rotation);
					Spawned->SetOnlyOwnerSee(bFirstPerson);
					Spawned->SetOwnerNoSee(!bFirstPerson);
				}
			}

			if (Entry.Niagara)
			{
				if (UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(Entry.Niagara, Mesh,
						WeaponSocket, Entry.Offset, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
						/*bAutoDestroy*/ true, /*bAutoActivate*/ true))
				{
					Spawned->SetRelativeScale3D(ScaleVector);
					Spawned->SetWorldRotation(Rotation);
					Spawned->SetOnlyOwnerSee(bFirstPerson);
					Spawned->SetOwnerNoSee(!bFirstPerson);
					if (Entry.bOverrideColor)
					{
						Spawned->SetVariableLinearColor(TEXT("User.Color"), Entry.Color);
					}
				}
			}
		}
	}
}
