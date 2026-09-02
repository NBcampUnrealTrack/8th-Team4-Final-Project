// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/RogueUtilityHoldCueNotify.h"

#include "Animation/AnimInstance.h"
#include "Augment/AugmentDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"

namespace
{
	void PlayHoldMontage(USkeletalMeshComponent* Mesh, UAnimMontage* Montage, float PlayRate, FName JumpToSection)
	{
		UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!AnimInstance || !Montage)
		{
			return;
		}

		if (AnimInstance->Montage_IsPlaying(Montage))
		{
			return;
		}

		AnimInstance->Montage_Play(Montage, PlayRate);

		if (!JumpToSection.IsNone())
		{
			AnimInstance->Montage_JumpToSection(JumpToSection, Montage);
		}
	}

	void StopHoldMontage(USkeletalMeshComponent* Mesh, UAnimMontage* Montage, float BlendOutTime)
	{
		UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!AnimInstance || !Montage)
		{
			return;
		}

		AnimInstance->Montage_Stop(BlendOutTime, Montage);
	}
}

ARogueUtilityHoldCueNotify::ARogueUtilityHoldCueNotify()
{
	HeldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldMesh"));
	HeldMesh->SetupAttachment(GetRootComponent());
	HeldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldMesh->bReceivesDecals = false;
	HeldMesh->SetVisibility(false);
}

bool ARogueUtilityHoldCueNotify::OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	const bool bResult = Super::OnActive_Implementation(Target, Parameters);

	// 드는 모션은 OnActive 에서 재생
	if (IsValid(Target) && Target->Implements<UPlayerInterface>())
	{
		PlayHoldMontage(IPlayerInterface::Execute_GetMesh1P(Target), Montage1P, PlayRate, NAME_None);
		PlayHoldMontage(IPlayerInterface::Execute_GetMesh3P(Target), Montage3P, PlayRate, NAME_None);
	}

	AttachHeldMesh(Target, Parameters);

	return bResult;
}

bool ARogueUtilityHoldCueNotify::WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	const bool bResult = Super::WhileActive_Implementation(Target, Parameters);

	// OnActive 를 놓친 경우 WhileActive 에서 재생
	if (IsValid(Target) && Target->Implements<UPlayerInterface>())
	{
		PlayHoldMontage(IPlayerInterface::Execute_GetMesh1P(Target), Montage1P, PlayRate, LoopSectionName);
		PlayHoldMontage(IPlayerInterface::Execute_GetMesh3P(Target), Montage3P, PlayRate, LoopSectionName);
	}

	AttachHeldMesh(Target, Parameters);

	return bResult;
}

bool ARogueUtilityHoldCueNotify::OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters)
{
	const bool bResult = Super::OnRemove_Implementation(Target, Parameters);

	if (IsValid(Target) && Target->Implements<UPlayerInterface>())
	{
		StopHoldMontage(IPlayerInterface::Execute_GetMesh1P(Target), Montage1P, StopBlendOutTime);
		StopHoldMontage(IPlayerInterface::Execute_GetMesh3P(Target), Montage3P, StopBlendOutTime);
	}

	ResetHeldMesh();

	return bResult;
}

bool ARogueUtilityHoldCueNotify::Recycle()
{
	ResetHeldMesh();

	return Super::Recycle();
}

void ARogueUtilityHoldCueNotify::AttachHeldMesh(AActor* Target, const FGameplayCueParameters& Parameters)
{
	if (!IsValid(HeldMesh) || !IsValid(Target) || !Target->Implements<UPlayerInterface>())
	{
		return;
	}

	const UAugmentDefinition* Definition = Cast<UAugmentDefinition>(Parameters.SourceObject.Get());
	if (!Definition)
	{
		return;
	}

	const FAugmentHeldVisual& Visual = Definition->HeldVisual;
	UStaticMesh* Mesh = Visual.Mesh.LoadSynchronous();
	if (!Mesh)
	{
		return;
	}

	// 시전자 본인인지 아닌지에 따라 1P/3P 메시를 선택
	const APawn* TargetPawn = Cast<APawn>(Target);
	const bool bFirstPerson = IsValid(TargetPawn) && TargetPawn->IsLocallyControlled();

	USkeletalMeshComponent* AttachTo = bFirstPerson
		? IPlayerInterface::Execute_GetMesh1P(Target)
		: IPlayerInterface::Execute_GetMesh3P(Target);
	if (!IsValid(AttachTo))
	{
		return;
	}

	HeldMesh->SetStaticMesh(Mesh);
	HeldMesh->SetCastShadow(!bFirstPerson);
	HeldMesh->AttachToComponent(
		AttachTo,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		bFirstPerson ? Visual.GripSocketName1P : Visual.GripSocketName3P);

	HeldMesh->SetRelativeLocation(bFirstPerson ? Visual.GripOffsetLocation1P : Visual.GripOffsetLocation3P);
	HeldMesh->SetRelativeRotation(bFirstPerson ? Visual.GripOffsetRotation1P : Visual.GripOffsetRotation3P);
	HeldMesh->SetVisibility(true);
}

void ARogueUtilityHoldCueNotify::ResetHeldMesh()
{
	if (!IsValid(HeldMesh))
	{
		return;
	}

	HeldMesh->SetVisibility(false);
	HeldMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	HeldMesh->SetStaticMesh(nullptr);
}
