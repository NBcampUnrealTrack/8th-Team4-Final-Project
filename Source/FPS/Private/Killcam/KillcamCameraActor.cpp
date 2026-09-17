// Fill out your copyright notice in the Description page of Project Settings.

#include "Killcam/KillcamCameraActor.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Weapon/WeaponBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UObject/UObjectIterator.h"

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
	PresentationTraceElapsed = 1.0f;
	PresentationTraceCount = 0;
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
	PresentationTraceElapsed += DeltaTime;
	if (PresentationTraceCount < 5 && PresentationTraceElapsed >= 1.0f)
	{
		PresentationTraceElapsed = 0.0f;
		++PresentationTraceCount;
		TraceWeaponPresentation();
		TraceLivePresentation();
	}
}

void AKillcamCameraActor::TraceLivePresentation() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	APlayerController* ViewingPC = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (IsValid(PC) && PC->IsLocalController() && PC->GetViewTarget() == this)
		{
			ViewingPC = PC;
			break;
		}
	}
	TSet<FPrimitiveComponentId> KillcamHiddenComponents;
	if (IsValid(ViewingPC))
	{
		ViewingPC->UpdateHiddenComponents(GetActorLocation(), KillcamHiddenComponents);
	}
	UE_LOG(LogTemp, Warning, TEXT("[KillcamLiveTrace] Sample=%d WorldTime=%.3f Camera=%s Location=%s Rotation=%s ViewingPC=%s ReplayKiller=%s"),
		PresentationTraceCount, World->GetTimeSeconds(), *GetPathName(), *GetActorLocation().ToString(), *GetActorRotation().ToString(),
		*GetPathNameSafe(ViewingPC), *GetPathNameSafe(TargetActor.Get()));
	const FLevelCollection* Live = World->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	const FLevelCollection* Replay = World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	UE_LOG(LogTemp, Warning, TEXT("[KillcamLiveTrace] LiveCollectionVisible=%d ReplayCollectionVisible=%d"), Live && Live->IsVisible(), Replay && Replay->IsVisible());
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		UPrimitiveComponent* Component = *It;
		if (!IsValid(Component) || Component->IsTemplate() || Component->GetWorld() != World)
		{
			continue;
		}
		AActor* Actor = Component->GetOwner();
		if (!IsValid(Actor))
		{
			continue;
		}
		APawn* RelatedPawn = Cast<APawn>(Actor);
		if (!RelatedPawn)
		{
			RelatedPawn = Cast<APawn>(Actor->GetOwner());
		}
		for (USceneComponent* Parent = Component->GetAttachParent(); !RelatedPawn && IsValid(Parent); Parent = Parent->GetAttachParent())
		{
			RelatedPawn = Cast<APawn>(Parent->GetOwner());
		}
		if (!RelatedPawn && !Actor->IsA<AWeaponBase>())
		{
			continue;
		}
		const TCHAR* CollectionName = TEXT("Other");
		ULevel* ActorLevel = Actor->GetLevel();
		if (Live && Live->GetLevels().Contains(TObjectPtr<ULevel>(ActorLevel)))
		{
			CollectionName = TEXT("Live");
		}
		else if (Replay && Replay->GetLevels().Contains(TObjectPtr<ULevel>(ActorLevel)))
		{
			CollectionName = TEXT("Replay");
		}
		FString PlayerName = TEXT("None");
		if (IsValid(RelatedPawn) && IsValid(RelatedPawn->GetPlayerState()))
		{
			PlayerName = RelatedPawn->GetPlayerState()->GetPlayerName();
		}
		const FVector RelativeToCamera = GetActorTransform().InverseTransformPosition(Component->Bounds.Origin);
		const float LastOnScreen = Component->GetLastRenderTimeOnScreen();
		UE_LOG(LogTemp, Warning, TEXT("[KillcamLiveTrace] Collection=%s Component=%s Class=%s Level=%s Owner=%s RelatedPawn=%s Player=%s Parent=%s"),
			CollectionName, *Component->GetPathName(), *Component->GetClass()->GetName(), *GetPathNameSafe(ActorLevel),
			*GetPathNameSafe(Actor->GetOwner()), *GetPathNameSafe(RelatedPawn), *PlayerName, *GetPathNameSafe(Component->GetAttachParent()));
		UE_LOG(LogTemp, Warning, TEXT("[KillcamLiveTrace] Flags Component=%s RawVisible=%d EffectiveVisible=%d Hidden=%d ActorHidden=%d OnlyOwner=%d OwnerNoSee=%d Registered=%d PCHiddenActor=%d PCHiddenComponent=%d KillcamViewHidden=%d"),
			*Component->GetPathName(), Component->GetVisibleFlag(), Component->IsVisible(), Component->bHiddenInGame, Actor->IsHidden(),
			Component->bOnlyOwnerSee, Component->bOwnerNoSee, Component->IsRegistered(),
			IsValid(ViewingPC) && ViewingPC->HiddenActors.Contains(Actor), IsValid(ViewingPC) && ViewingPC->HiddenPrimitiveComponents.Contains(Component),
			KillcamHiddenComponents.Contains(Component->GetPrimitiveSceneId()));
		UE_LOG(LogTemp, Warning, TEXT("[KillcamLiveTrace] Render Component=%s LastOnScreen=%.3f Age=%.3f BoundsOrigin=%s CameraLocal=%s"),
			*Component->GetPathName(), LastOnScreen, World->GetTimeSeconds() - LastOnScreen,
			*Component->Bounds.Origin.ToString(), *RelativeToCamera.ToString());
	}
}

void AKillcamCameraActor::TraceSkeletalMesh(const TCHAR* Label, USkeletalMeshComponent* Mesh, USceneComponent* ExpectedParent, FName ExpectedSocket) const
{
	if (!IsValid(Mesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("[KillcamWeaponTrace] %s Mesh=None"), Label);
		return;
	}
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	FString AnimClass = TEXT("None");
	FString MontageName = TEXT("None");
	float MontagePosition = 0.0f;
	if (IsValid(Anim))
	{
		AnimClass = Anim->GetClass()->GetPathName();
		if (UAnimMontage* Montage = Anim->GetCurrentActiveMontage())
		{
			MontageName = Montage->GetPathName();
			MontagePosition = Anim->Montage_GetPosition(Montage);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[KillcamWeaponTrace] %s Mesh=%s Asset=%s Parent=%s Socket=%s ExpectedParent=%s ParentMatches=%d ExpectedSocket=%s SocketExists=%d"),
		Label, *Mesh->GetPathName(), *GetPathNameSafe(Mesh->GetSkinnedAsset()), *GetPathNameSafe(Mesh->GetAttachParent()),
		*Mesh->GetAttachSocketName().ToString(), *GetPathNameSafe(ExpectedParent), Mesh->GetAttachParent() == ExpectedParent,
		*ExpectedSocket.ToString(), IsValid(ExpectedParent) && ExpectedParent->DoesSocketExist(ExpectedSocket));
	UE_LOG(LogTemp, Warning, TEXT("[KillcamWeaponTrace] %s World=%s Relative=%s Visible=%d Hidden=%d OnlyOwner=%d OwnerNoSee=%d SimPhysics=%d"),
		Label, *Mesh->GetComponentTransform().ToHumanReadableString(), *Mesh->GetRelativeTransform().ToHumanReadableString(),
		Mesh->IsVisible(), Mesh->bHiddenInGame, Mesh->bOnlyOwnerSee, Mesh->bOwnerNoSee, Mesh->IsSimulatingPhysics());
	UE_LOG(LogTemp, Warning, TEXT("[KillcamWeaponTrace] %s AnimClass=%s Mode=%d Leader=%s TickEnabled=%d TickOption=%d Paused=%d Montage=%s Position=%.3f"),
		Label, *AnimClass, static_cast<int32>(Mesh->GetAnimationMode()), *GetPathNameSafe(Mesh->LeaderPoseComponent.Get()),
		Mesh->IsComponentTickEnabled(), static_cast<int32>(Mesh->VisibilityBasedAnimTickOption), Mesh->bPauseAnims, *MontageName, MontagePosition);
	if (IsValid(ExpectedParent) && ExpectedParent->DoesSocketExist(ExpectedSocket))
	{
		UE_LOG(LogTemp, Warning, TEXT("[KillcamWeaponTrace] %s ExpectedSocketWorld=%s"), Label,
			*ExpectedParent->GetSocketTransform(ExpectedSocket).ToHumanReadableString());
	}
}

void AKillcamCameraActor::TraceWeaponPresentation() const
{
	UWorld* World = GetWorld();
	AActor* Target = TargetActor.Get();
	if (!IsValid(World) || !IsValid(Target) || !Target->Implements<UPlayerInterface>())
	{
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[KillcamWeaponTrace] Sample=%d Target=%s CurrentWeapon=%s Camera=%s"),
		PresentationTraceCount, *Target->GetPathName(), *GetPathNameSafe(IPlayerInterface::Execute_GetCurrentWeapon(Target)), *GetActorTransform().ToHumanReadableString());
	TraceSkeletalMesh(TEXT("KillerArms"), IPlayerInterface::Execute_GetMesh1P(Target), CameraComponent, NAME_None);
	TraceSkeletalMesh(TEXT("KillerBody"), IPlayerInterface::Execute_GetMesh3P(Target), Target->GetRootComponent(), NAME_None);
	for (ELevelCollectionType Type : { ELevelCollectionType::DynamicSourceLevels, ELevelCollectionType::DynamicDuplicatedLevels })
	{
		const TCHAR* CollectionName = TEXT("Live");
		if (Type == ELevelCollectionType::DynamicDuplicatedLevels)
		{
			CollectionName = TEXT("Replay");
		}
		const FLevelCollection* Collection = World->FindCollectionByType(Type);
		if (!Collection)
		{
			continue;
		}
		for (ULevel* Level : Collection->GetLevels())
		{
			if (!IsValid(Level))
			{
				continue;
			}
			for (AActor* Actor : Level->Actors)
			{
				AWeaponBase* Weapon = Cast<AWeaponBase>(Actor);
				if (!IsValid(Weapon))
				{
					continue;
				}
				AActor* WeaponOwner = Weapon->GetOwner();
				USceneComponent* ExpectedArms = nullptr;
				USceneComponent* ExpectedBody = nullptr;
				if (IsValid(WeaponOwner) && WeaponOwner->Implements<UPlayerInterface>())
				{
					ExpectedArms = IPlayerInterface::Execute_GetMesh1P(WeaponOwner);
					ExpectedBody = IPlayerInterface::Execute_GetMesh3P(WeaponOwner);
				}
				USceneComponent* Root = Weapon->GetRootComponent();
				USceneComponent* RootParent = nullptr;
				if (IsValid(Root))
				{
					RootParent = Root->GetAttachParent();
				}
				UE_LOG(LogTemp, Warning, TEXT("[KillcamWeaponTrace] Collection=%s CollectionVisible=%d Weapon=%s Owner=%s OwnerIsKiller=%d ActorHidden=%d Root=%s RootParent=%s"),
					CollectionName, Collection->IsVisible(), *Weapon->GetPathName(), *GetPathNameSafe(WeaponOwner), WeaponOwner == Target,
					Weapon->IsHidden(), *GetPathNameSafe(Root), *GetPathNameSafe(RootParent));
				TraceSkeletalMesh(TEXT("Weapon1P"), Weapon->GetMesh1P(), ExpectedArms, Weapon->GetGripSocketName1P());
				TraceSkeletalMesh(TEXT("Weapon3P"), Weapon->GetMesh3P(), ExpectedBody, Weapon->GetGripSocketName3P());
			}
		}
	}
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
	SetMeshPresentation(IPlayerInterface::Execute_GetMesh1P(Target), true);
	if (IsValid(Weapon))
	{
		SetMeshPresentation(Weapon->GetMesh1P(), true);
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
		if (!bFirstPerson || Component == Root)
		{
			Component->SetVisibility(bFirstPerson);
			Component->SetHiddenInGame(!bFirstPerson);
		}
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
