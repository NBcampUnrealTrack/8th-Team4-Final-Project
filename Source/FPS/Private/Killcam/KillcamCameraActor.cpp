// Fill out your copyright notice in the Description page of Project Settings.

#include "Killcam/KillcamCameraActor.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"

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
	TargetActor = InTargetActor;
	bHasValidView = false;

	FVector DesiredLocation;
	FRotator DesiredRotation;
	if (ComputeDesiredView(DesiredLocation, DesiredRotation))
	{
		SetActorLocationAndRotation(DesiredLocation, DesiredRotation);
		bHasValidView = true;
	}
}

void AKillcamCameraActor::StopTracking()
{
	TargetActor.Reset();
	bHasValidView = false;
}

void AKillcamCameraActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	UpdateTracking(DeltaTime);

	Super::CalcCamera(DeltaTime, OutResult);
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
	OutLocation = ViewLocation + AimRotation.RotateVector(ViewOffset);
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

	const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaTime, LocationInterpSpeed);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaTime, RotationInterpSpeed);
	SetActorLocationAndRotation(NewLocation, NewRotation);
}
