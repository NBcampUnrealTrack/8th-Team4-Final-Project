// Fill out your copyright notice in the Description page of Project Settings.

#include "Killcam/KillcamCameraActor.h"

#include "Camera/CameraComponent.h"
#include "Combat/CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogKillcamPresentation, Log, All);

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
		SetLoosePartPresentation(Weapon, !bHideFirstPerson);
	}

	// TEMP: 킬캠 총 미표시 원인 확인용. 상태가 바뀔 때만 남긴다.
	const USkeletalMeshComponent* WeaponMesh1P = IsValid(Weapon) ? Weapon->GetMesh1P() : nullptr;
	const FString DiagnosticState = FString::Printf(
		TEXT("Weapon=%s Aiming=%d HideFP=%d ActorHidden=%d Mesh1PVisible=%d Mesh1PHidden=%d Mesh1PParent=%s"),
		*GetNameSafe(Weapon),
		IsValid(Combat) && Combat->IsAiming() ? 1 : 0,
		bHideFirstPerson ? 1 : 0,
		IsValid(Weapon) && Weapon->IsHidden() ? 1 : 0,
		IsValid(WeaponMesh1P) && WeaponMesh1P->IsVisible() ? 1 : 0,
		IsValid(WeaponMesh1P) && WeaponMesh1P->bHiddenInGame ? 1 : 0,
		IsValid(WeaponMesh1P) ? *GetNameSafe(WeaponMesh1P->GetAttachParent()) : TEXT("None"));
	if (DiagnosticState != LastDiagnosticState)
	{
		LastDiagnosticState = DiagnosticState;
		UE_LOG(LogKillcamPresentation, Log, TEXT("[KillcamDiag] %s"), *DiagnosticState);
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
		ApplyComponentPresentation(Cast<UMeshComponent>(Child), bFirstPerson, Child == Root);
	}
}

void AKillcamCameraActor::SetLoosePartPresentation(const AWeaponBase* Weapon, bool bFirstPerson)
{
	if (!IsValid(Weapon))
	{
		return;
	}
	const USceneComponent* Mesh1P = Weapon->GetMesh1P();
	const USceneComponent* Mesh3P = Weapon->GetMesh3P();
	TArray<UMeshComponent*> Components;
	Weapon->GetComponents<UMeshComponent>(Components, true);
	for (UMeshComponent* Component : Components)
	{
		if (!IsValid(Component)
			|| (IsValid(Mesh1P) && (Component == Mesh1P || Component->IsAttachedTo(Mesh1P)))
			|| (IsValid(Mesh3P) && (Component == Mesh3P || Component->IsAttachedTo(Mesh3P))))
		{
			continue;
		}

		// Mesh1P/Mesh3P 밖(SceneRoot 등)에 붙은 BP 파츠는 트리로 1P/3P를 알 수 없어 원래 오너 플래그로 가른다.
		// 킬캠 시점 액터는 오너가 아니라서 OwnerNoSee 3P 스코프가 그대로 보이던 문제.
		const FKillcamMeshVisibility* Saved = SavedMeshVisibility.FindByPredicate([Component](const FKillcamMeshVisibility& State)
		{
			return State.Component == Component;
		});
		const bool bOwnerNoSee = Saved ? Saved->bOwnerNoSee : Component->bOwnerNoSee;
		const bool bOnlyOwnerSee = Saved ? Saved->bOnlyOwnerSee : Component->bOnlyOwnerSee;
		if (bOwnerNoSee)
		{
			ApplyComponentPresentation(Component, false, false);
		}
		else if (bOnlyOwnerSee)
		{
			ApplyComponentPresentation(Component, bFirstPerson, false);
		}
	}
}

void AKillcamCameraActor::ApplyComponentPresentation(UMeshComponent* Component, bool bFirstPerson, bool bIsRoot)
{
	if (!IsValid(Component))
	{
		return;
	}
	FKillcamMeshVisibility* State = SavedMeshVisibility.FindByPredicate([Component](const FKillcamMeshVisibility& Saved)
	{
		return Saved.Component == Component;
	});
	if (!State)
	{
		State = &SavedMeshVisibility.AddDefaulted_GetRef();
		State->Component = Component;
		State->bOnlyOwnerSee = Component->bOnlyOwnerSee;
		State->bOwnerNoSee = Component->bOwnerNoSee;
		State->bVisible = Component->IsVisible();
		State->bHiddenInGame = Component->bHiddenInGame;
		if (USkinnedMeshComponent* Mesh = Cast<USkinnedMeshComponent>(Component))
		{
			State->AnimTickOption = Mesh->VisibilityBasedAnimTickOption;
		}
	}
	Component->SetOnlyOwnerSee(false);
	Component->SetOwnerNoSee(false);
	if (bIsRoot)
	{
		Component->SetVisibility(bFirstPerson);
		Component->SetHiddenInGame(!bFirstPerson);
		State->bVisibilityOverridden = true;
	}
	else
	{
		// 자식(스코프/사이트/홀스터 무기)은 HiddenInGame으로만 숨기고 보일 땐 원래 값으로 되돌린다.
		// Visibility는 ADS/무기 교체 로직이 관리하므로 건드리면 ADS 해제나 무기 교체 후 파츠가 안 돌아온다.
		// 킬캠은 오너 시점을 흉내 내므로 원래 OwnerNoSee였던 파츠(Mesh1P 아래 SM_sight_002 등)는 오너처럼 숨긴다.
		Component->SetHiddenInGame(bFirstPerson && !State->bOwnerNoSee ? State->bHiddenInGame : true);
	}
	if (USkinnedMeshComponent* Mesh = Cast<USkinnedMeshComponent>(Component))
	{
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
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
		if (State.bVisibilityOverridden)
		{
			Component->SetVisibility(State.bVisible);
		}
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
