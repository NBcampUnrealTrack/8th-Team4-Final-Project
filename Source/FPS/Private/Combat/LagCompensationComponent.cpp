// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/LagCompensationComponent.h"

#include "Combat/LagCompensationHitBoxProfile.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

ULagCompensationComponent::ULagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();

	StartRecording();
}

void ULagCompensationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRecording();

	Super::EndPlay(EndPlayReason);
}

bool ULagCompensationComponent::GetClosestFramePackage(float ServerTime, FLagCompensationFramePackage& OutFramePackage) const
{
	if (FrameHistory.IsEmpty())
	{
		return false;
	}

	int32 ClosestIndex = INDEX_NONE;
	float ClosestDelta = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < FrameHistory.Num(); ++Index)
	{
		const float Delta = FMath::Abs(FrameHistory[Index].ServerTime - ServerTime);
		if (Delta < ClosestDelta)
		{
			ClosestDelta = Delta;
			ClosestIndex = Index;
		}
	}

	if (ClosestIndex == INDEX_NONE)
	{
		return false;
	}

	OutFramePackage = FrameHistory[ClosestIndex];
	return true;
}

void ULagCompensationComponent::StartRecording()
{
	if (!ShouldRecordHistory())
	{
		return;
	}

	CacheHitBoxComponents();
	SaveFramePackage();

	if (SnapshotInterval <= 0.0f)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SnapshotTimerHandle,
			this,
			&ThisClass::SaveFramePackage,
			SnapshotInterval,
			true);
	}
}

void ULagCompensationComponent::StopRecording()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SnapshotTimerHandle);
	}
}

void ULagCompensationComponent::SaveFramePackage()
{
	if (!ShouldRecordHistory())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (CachedHitBoxes.IsEmpty())
	{
		CacheHitBoxComponents();
	}

	FLagCompensationFramePackage FramePackage;
	FramePackage.ServerTime = World->GetTimeSeconds();

	for (const TObjectPtr<UBoxComponent>& BoxComponent : CachedHitBoxes)
	{
		CaptureBoxComponent(BoxComponent.Get(), FramePackage);
	}

	if (FramePackage.HitBoxes.IsEmpty())
	{
		CaptureFallbackCapsule(FramePackage);
	}

	DrawFramePackage(FramePackage);

	FrameHistory.Add(MoveTemp(FramePackage));
	TrimFrameHistory(World->GetTimeSeconds());
}

void ULagCompensationComponent::CacheHitBoxComponents()
{
	CachedHitBoxes.Reset();
	GeneratedHitBoxes.Reset();

	CreateHitBoxesFromProfile();
	if (!CachedHitBoxes.IsEmpty())
	{
		return;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UBoxComponent*> BoxComponents;
	Owner->GetComponents<UBoxComponent>(BoxComponents);

	for (UBoxComponent* BoxComponent : BoxComponents)
	{
		if (!IsValid(BoxComponent))
		{
			continue;
		}

		if (!BoxComponent->ComponentHasTag(HitBoxComponentTag))
		{
			continue;
		}

		CachedHitBoxes.Add(BoxComponent);
	}
}

void ULagCompensationComponent::CreateHitBoxesFromProfile()
{
	if (!IsValid(HitBoxProfile.Get()))
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();
	if (!IsValid(MeshComponent))
	{
		return;
	}

	for (const FLagCompensationHitBoxDefinition& Definition : HitBoxProfile->HitBoxes)
	{
		if (Definition.HitBoxName.IsNone() || Definition.AttachBoneName.IsNone())
		{
			continue;
		}

		UBoxComponent* HitBox = NewObject<UBoxComponent>(OwnerCharacter, Definition.HitBoxName);
		if (!IsValid(HitBox))
		{
			continue;
		}

		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitBox->SetGenerateOverlapEvents(false);
		HitBox->ComponentTags.AddUnique(HitBoxComponentTag);
		HitBox->SetBoxExtent(Definition.BoxExtent, false);
		HitBox->SetupAttachment(MeshComponent, Definition.AttachBoneName);
		HitBox->RegisterComponent();
		HitBox->SetRelativeLocationAndRotation(Definition.RelativeLocation, Definition.RelativeRotation);

		GeneratedHitBoxes.Add(HitBox);
		CachedHitBoxes.Add(HitBox);
	}
}

void ULagCompensationComponent::CaptureBoxComponent(
	const UBoxComponent* BoxComponent,
	FLagCompensationFramePackage& OutFramePackage) const
{
	if (!IsValid(BoxComponent))
	{
		return;
	}

	FLagCompensatedHitBoxInfo HitBoxInfo;
	HitBoxInfo.Location = BoxComponent->GetComponentLocation();
	HitBoxInfo.Rotation = BoxComponent->GetComponentRotation();
	HitBoxInfo.BoxExtent = BoxComponent->GetScaledBoxExtent();

	OutFramePackage.HitBoxes.Add(BoxComponent->GetFName(), HitBoxInfo);
}

void ULagCompensationComponent::CaptureFallbackCapsule(FLagCompensationFramePackage& OutFramePackage) const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	const UCapsuleComponent* CapsuleComponent = OwnerCharacter->GetCapsuleComponent();
	if (!IsValid(CapsuleComponent))
	{
		return;
	}

	FLagCompensatedHitBoxInfo HitBoxInfo;
	HitBoxInfo.Location = CapsuleComponent->GetComponentLocation();
	HitBoxInfo.Rotation = CapsuleComponent->GetComponentRotation();
	HitBoxInfo.BoxExtent = FVector(
		CapsuleComponent->GetScaledCapsuleRadius(),
		CapsuleComponent->GetScaledCapsuleRadius(),
		CapsuleComponent->GetScaledCapsuleHalfHeight());

	OutFramePackage.HitBoxes.Add(CapsuleComponent->GetFName(), HitBoxInfo);
}

void ULagCompensationComponent::DrawFramePackage(const FLagCompensationFramePackage& FramePackage) const
{
	if (!bDrawDebugHitBoxes)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const TPair<FName, FLagCompensatedHitBoxInfo>& HitBoxPair : FramePackage.HitBoxes)
	{
		const FLagCompensatedHitBoxInfo& HitBoxInfo = HitBoxPair.Value;
		DrawDebugBox(
			World,
			HitBoxInfo.Location,
			HitBoxInfo.BoxExtent,
			FQuat(HitBoxInfo.Rotation),
			DebugDrawColor,
			false,
			DebugDrawDuration);
	}
}

void ULagCompensationComponent::TrimFrameHistory(float CurrentServerTime)
{
	for (int32 Index = FrameHistory.Num() - 1; Index >= 0; --Index)
	{
		if (CurrentServerTime - FrameHistory[Index].ServerTime <= MaxRecordTime)
		{
			continue;
		}

		FrameHistory.RemoveAt(Index);
	}
}

bool ULagCompensationComponent::ShouldRecordHistory() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}
