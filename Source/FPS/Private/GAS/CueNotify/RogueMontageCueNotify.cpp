// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/CueNotify/RogueMontageCueNotify.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/PlayerInterface.h"
#include "TimerManager.h"

namespace
{
	void PlayMontageOnMesh(USkeletalMeshComponent* Mesh, UAnimMontage* Montage, float PlayRate)
	{
		if (!Mesh || !Montage)
		{
			return;
		}

		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage, PlayRate);
		}
	}
}

bool URogueMontageCueNotify::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	const bool bResult = Super::OnExecute_Implementation(Target, Parameters);

	if (Target && Target->Implements<UPlayerInterface>())
	{
		USkeletalMeshComponent* Mesh1P = IPlayerInterface::Execute_GetMesh1P(Target);
		USkeletalMeshComponent* Mesh3P = IPlayerInterface::Execute_GetMesh3P(Target);
		PlayMontageOnMesh(Mesh1P, Montage1P, PlayRate);
		PlayMontageOnMesh(Mesh3P, ResolveMontage3P(Target), PlayRate);
		SpawnProp(Mesh1P, PropAnim1P, PropOffset1P, true);
		SpawnProp(Mesh3P, PropAnim3P, PropOffset3P, false);
	}

	return bResult;
}

UAnimMontage* URogueMontageCueNotify::ResolveMontage3P(AActor* Target) const
{
	return Montage3P;
}

void URogueMontageCueNotify::SpawnProp(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Anim, const FTransform& Offset, bool bFirstPerson) const
{
	if (!PropClass || !Mesh)
	{
		return;
	}

	UWorld* World = Mesh->GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Mesh->GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Prop = World->SpawnActor<AActor>(PropClass, Mesh->GetSocketTransform(PropSocket), SpawnParams);
	if (!Prop)
	{
		return;
	}


	TArray<UPrimitiveComponent*> Primitives;
	Prop->GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetSimulatePhysics(false);
		Primitive->SetFirstPersonPrimitiveType(Mesh->FirstPersonPrimitiveType);
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetOnlyOwnerSee(bFirstPerson);
		Primitive->SetOwnerNoSee(!bFirstPerson);
		Primitive->SetCastShadow(!bFirstPerson);
	}

	Prop->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, PropSocket);
	Prop->SetActorRelativeTransform(Offset);

	if (USkeletalMeshComponent* PropMesh = Prop->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (Anim)
		{
			PropMesh->PlayAnimation(Anim, false);
		}
	}

	if (PropVisibilityDelay > 0.f)
	{
		Prop->SetActorHiddenInGame(true);
		FTimerHandle ShowHandle;
		World->GetTimerManager().SetTimer(ShowHandle, FTimerDelegate::CreateWeakLambda(Prop, [Prop]()
		{
			Prop->SetActorHiddenInGame(false);
		}), PropVisibilityDelay, false);
	}

	const float Lifetime = PropLifetime > 0.f ? PropLifetime : (Anim ? Anim->GetPlayLength() : 0.f);
	if (Lifetime > 0.f)
	{
		Prop->SetLifeSpan(Lifetime);
	}
}
