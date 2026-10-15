// Fill out your copyright notice in the Description page of Project Settings.

#include "Killcam/KillcamCameraActor.h"

#include "Camera/CameraComponent.h"
#include "Combat/CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon/WeaponBase.h"

namespace
{
	FRotator NormalizeReplicatedAimRotation(FRotator AimRotation)
	{
		if (AimRotation.Pitch > 90.f)
		{
			const FVector2D InRange(270.f, 360.f);
			const FVector2D OutRange(-90.f, 0.f);
			AimRotation.Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AimRotation.Pitch);
		}

		return AimRotation;
	}
}

AKillcamCameraActor::AKillcamCameraActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("KillcamCamera"));
	SetRootComponent(CameraComponent);
	CameraComponent->bUsePawnControlRotation = false;

	bFindCameraComponentWhenViewTarget = true;
	bReplicates = false;
	SetCanBeDamaged(false);
}

void AKillcamCameraActor::StartTracking(AActor* InTargetActor)
{
	StopTracking();
	TargetActor = InTargetActor;
	bHasValidView = false;
	if (IsValid(InTargetActor) && InTargetActor->Implements<UPlayerInterface>())
	{
		TargetCamera = IPlayerInterface::Execute_GetViewCamera(InTargetActor);
	}
	UpdateFirstPersonPresentation();

	FVector DesiredLocation;
	FRotator DesiredRotation;
	if (ComputeDesiredView(DesiredLocation, DesiredRotation))
	{
		SetActorLocationAndRotation(DesiredLocation, DesiredRotation);
		bHasValidView = true;
	}
	UpdateFirstPersonAttachment();
}

void AKillcamCameraActor::StopTracking()
{
	RestoreMeshPresentation();
	RestoreFirstPersonAttachment();
	TargetCamera.Reset();
	PresentedWeapon.Reset();
	TargetActor.Reset();
	bHasValidView = false;
}

void AKillcamCameraActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	UpdateFirstPersonPresentation();
	UpdateTracking(DeltaTime);
	UpdateFirstPersonAttachment();
	if (TargetCamera.IsValid())
	{
		CameraComponent->SetFieldOfView(TargetCamera->FieldOfView);
	}

	Super::CalcCamera(DeltaTime, OutResult);
}

void AKillcamCameraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopTracking();
	Super::EndPlay(EndPlayReason);
}

void AKillcamCameraActor::UpdateFirstPersonAttachment()
{
	AActor* Target = TargetActor.Get();
	if (!bHasValidView || !IsValid(Target) || !TargetCamera.IsValid()
		|| !Target->Implements<UPlayerInterface>())
	{
		return;
	}

	USkeletalMeshComponent* Mesh = IPlayerInterface::Execute_GetMesh1P(Target);
	if (AttachedFirstPersonMesh.Get() == Mesh)
	{
		return;
	}
	RestoreFirstPersonAttachment();
	if (!IsValid(Mesh))
	{
		return;
	}

	const FTransform CameraRelativeTransform = Mesh->GetComponentTransform().GetRelativeTransform(TargetCamera->GetComponentTransform());
	SavedFirstPersonParent = Mesh->GetAttachParent();
	SavedFirstPersonSocket = Mesh->GetAttachSocketName();
	SavedFirstPersonTransform = Mesh->GetRelativeTransform();
	if (Mesh->AttachToComponent(CameraComponent, FAttachmentTransformRules::KeepWorldTransform))
	{
		AttachedFirstPersonMesh = Mesh;
		Mesh->SetRelativeTransform(CameraRelativeTransform);
	}
}

void AKillcamCameraActor::RestoreFirstPersonAttachment()
{
	if (USkeletalMeshComponent* Mesh = AttachedFirstPersonMesh.Get())
	{
		if (SavedFirstPersonParent.IsValid())
		{
			Mesh->AttachToComponent(SavedFirstPersonParent.Get(), FAttachmentTransformRules::KeepRelativeTransform, SavedFirstPersonSocket);
			Mesh->SetRelativeTransform(SavedFirstPersonTransform);
		}
		else
		{
			Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
	AttachedFirstPersonMesh.Reset();
	SavedFirstPersonParent.Reset();
}

void AKillcamCameraActor::UpdateFirstPersonPresentation()
{
	AActor* Target = TargetActor.Get();
	if (!IsValid(Target) || !Target->Implements<UPlayerInterface>())
	{
		RestoreMeshPresentation();
		return;
	}

	AWeaponBase* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(Target);
	if (PresentedWeapon.Get() != Weapon)
	{
		RestoreMeshPresentation();
		PresentedWeapon = Weapon;
	}

	SetMeshPresentation(IPlayerInterface::Execute_GetMesh3P(Target), false);
	if (IsValid(Weapon))
	{
		SetMeshPresentation(Weapon->GetMesh3P(), false);
	}
	const UCombatComponent* Combat = Target->FindComponentByClass<UCombatComponent>();
	const bool bHideFirstPerson = IsValid(Combat)
		&& Combat->IsAiming()
		&& IsValid(Weapon)
		&& Weapon->ShouldHideFirstPersonPresentationWhileADS();
	SetMeshPresentation(IPlayerInterface::Execute_GetMesh1P(Target), !bHideFirstPerson);
	if (IsValid(Weapon))
	{
		SetMeshPresentation(Weapon->GetMesh1P(), !bHideFirstPerson);
	}
}

void AKillcamCameraActor::SetMeshPresentation(USceneComponent* Root, bool bFirstPerson)
{
	if (!IsValid(Root))
	{
		return;
	}
	TArray<USceneComponent*> Components;
	Root->GetChildrenComponents(true, Components);
	Components.Add(Root);
	for (USceneComponent* Child : Components)
	{
		UMeshComponent* Component = Cast<UMeshComponent>(Child);
		if (!IsValid(Component))
		{
			continue;
		}
		const bool bSaved = SavedMeshVisibility.ContainsByPredicate([Component](const FKillcamMeshVisibility& State)
		{
			return State.Component == Component;
		});
		if (!bSaved)
		{
			FKillcamMeshVisibility& State = SavedMeshVisibility.AddDefaulted_GetRef();
			State.Component = Component;
			State.bOnlyOwnerSee = Component->bOnlyOwnerSee;
			State.bOwnerNoSee = Component->bOwnerNoSee;
			State.bVisible = Component->IsVisible();
			State.bHiddenInGame = Component->bHiddenInGame;
			if (USkinnedMeshComponent* Mesh = Cast<USkinnedMeshComponent>(Component))
			{
				State.AnimTickOption = Mesh->VisibilityBasedAnimTickOption;
			}
		}
		Component->SetOnlyOwnerSee(false);
		Component->SetOwnerNoSee(false);
		Component->SetVisibility(bFirstPerson);
		Component->SetHiddenInGame(!bFirstPerson);
		if (USkinnedMeshComponent* Mesh = Cast<USkinnedMeshComponent>(Component))
		{
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	}
}


void AKillcamCameraActor::RestoreMeshPresentation()
{
	for (const FKillcamMeshVisibility& State : SavedMeshVisibility)
	{
		UPrimitiveComponent* Component = State.Component.Get();
		if (!IsValid(Component))
		{
			continue;
		}
		Component->SetOnlyOwnerSee(State.bOnlyOwnerSee);
		Component->SetOwnerNoSee(State.bOwnerNoSee);
		Component->SetVisibility(State.bVisible);
		Component->SetHiddenInGame(State.bHiddenInGame);
		if (USkinnedMeshComponent* Mesh = Cast<USkinnedMeshComponent>(Component))
		{
			Mesh->VisibilityBasedAnimTickOption = State.AnimTickOption;
		}
	}
	SavedMeshVisibility.Reset();
}

bool AKillcamCameraActor::ComputeDesiredView(FVector& OutLocation, FRotator& OutRotation) const
{
	AActor* Target = TargetActor.Get();
	if (!IsValid(Target))
	{
		return false;
	}

	FVector ViewLocation = Target->GetActorLocation();
	FRotator AimRotation = Target->GetActorRotation();

	if (Target->Implements<UPlayerInterface>())
	{
		if (const UCameraComponent* ViewCamera = IPlayerInterface::Execute_GetViewCamera(Target))
		{
			ViewLocation = ViewCamera->GetComponentLocation();
		}

		AimRotation = IPlayerInterface::Execute_GetFixedAimRotation(Target);
	}
	else if (const APawn* TargetPawn = Cast<APawn>(Target))
	{
		ViewLocation = TargetPawn->GetPawnViewLocation();
		AimRotation = NormalizeReplicatedAimRotation(TargetPawn->GetBaseAimRotation());
	}

	OutRotation = AimRotation;
	OutLocation = ViewLocation;
	return true;
}

void AKillcamCameraActor::UpdateTracking(float DeltaTime)
{
	FVector DesiredLocation;
	FRotator DesiredRotation;
	if (!ComputeDesiredView(DesiredLocation, DesiredRotation))
	{
		return;
	}

	if (!bHasValidView || DeltaTime <= 0.f)
	{
		SetActorLocationAndRotation(DesiredLocation, DesiredRotation);
		bHasValidView = true;
		return;
	}

	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaTime, RotationInterpSpeed);
	const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaTime, LocationInterpSpeed);
	SetActorLocationAndRotation(NewLocation, NewRotation);
}
