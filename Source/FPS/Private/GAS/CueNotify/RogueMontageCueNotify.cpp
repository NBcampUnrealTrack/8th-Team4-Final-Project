// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/CueNotify/RogueMontageCueNotify.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/PlayerInterface.h"

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
		PlayMontageOnMesh(IPlayerInterface::Execute_GetMesh1P(Target), Montage1P, PlayRate);
		PlayMontageOnMesh(IPlayerInterface::Execute_GetMesh3P(Target), Montage3P, PlayRate);
	}

	return bResult;
}
