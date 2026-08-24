// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/LagCompensationComponent.h"

#include "Combat/CombatHitZoneDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "Tags/GameplayTags.h"
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

bool ULagCompensationComponent::GetFramePackageAtTime(float ServerTime,
													  FLagCompensationFramePackage& OutFramePackage) const
{
	if (FrameHistory.IsEmpty())
	{
		return false;
	}

	if (FrameHistory.Num() == 1 || ServerTime <= FrameHistory[0].ServerTime)
	{
		OutFramePackage = FrameHistory[0];
		return true;
	}

	const int32 LastIndex = FrameHistory.Num() - 1;
	if (ServerTime >= FrameHistory[LastIndex].ServerTime)
	{
		OutFramePackage = FrameHistory[LastIndex];
		return true;
	}

	for (int32 NewerIndex = 1; NewerIndex < FrameHistory.Num(); ++NewerIndex)
	{
		const FLagCompensationFramePackage& NewerFrame = FrameHistory[NewerIndex];
		if (NewerFrame.ServerTime < ServerTime)
		{
			continue;
		}

		const FLagCompensationFramePackage& OlderFrame = FrameHistory[NewerIndex - 1];
		const float TimeRange = NewerFrame.ServerTime - OlderFrame.ServerTime;
		if (TimeRange <= KINDA_SMALL_NUMBER)
		{
			OutFramePackage = NewerFrame;
			return true;
		}

		const float Alpha = FMath::Clamp((ServerTime - OlderFrame.ServerTime) / TimeRange, 0.0f, 1.0f);
		OutFramePackage.ServerTime = ServerTime;
		OutFramePackage.HitBoxes.Reset();
		OutFramePackage.HitBoxes.Reserve(OlderFrame.HitBoxes.Num());

		for (const TPair<FName, FLagCompensatedHitBoxInfo>& OlderHitBoxPair : OlderFrame.HitBoxes)
		{
			const FLagCompensatedHitBoxInfo* NewerHitBox = NewerFrame.HitBoxes.Find(OlderHitBoxPair.Key);
			if (!NewerHitBox)
			{
				continue;
			}

			FLagCompensatedHitBoxInfo InterpolatedHitBox;
			InterpolatedHitBox.Location = FMath::Lerp(OlderHitBoxPair.Value.Location, NewerHitBox->Location, Alpha);
			InterpolatedHitBox.Rotation = FMath::Lerp(OlderHitBoxPair.Value.Rotation, NewerHitBox->Rotation, Alpha);
			InterpolatedHitBox.BoxExtent = FMath::Lerp(OlderHitBoxPair.Value.BoxExtent, NewerHitBox->BoxExtent, Alpha);
			InterpolatedHitBox.HitZoneTag =
				OlderHitBoxPair.Value.HitZoneTag.IsValid() ? OlderHitBoxPair.Value.HitZoneTag : NewerHitBox->HitZoneTag;
			OutFramePackage.HitBoxes.Add(OlderHitBoxPair.Key, InterpolatedHitBox);
		}

		return !OutFramePackage.HitBoxes.IsEmpty();
	}

	return false;
}

bool ULagCompensationComponent::ApplyFramePackageForTrace(float ServerTime, ECollisionChannel TraceChannel,
														  FLagCompensationRewindRestoreState& OutRestoreState)
{
	OutRestoreState = FLagCompensationRewindRestoreState();

	if (CachedHitBoxes.IsEmpty())
	{
		CacheHitBoxComponents();
	}

	FLagCompensationFramePackage FramePackage;
	if (!GetFramePackageAtTime(ServerTime, FramePackage) || FramePackage.HitBoxes.IsEmpty())
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent) || PrimitiveComponent->ComponentHasTag(HitBoxComponentTag))
		{
			continue;
		}

		FLagCompensationPrimitiveRestoreState RestoreState;
		RestoreState.PrimitiveComponent = PrimitiveComponent;
		RestoreState.CollisionEnabled = PrimitiveComponent->GetCollisionEnabled();
		OutRestoreState.DisabledPrimitives.Add(RestoreState);

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	for (const TObjectPtr<UBoxComponent>& BoxComponent : CachedHitBoxes)
	{
		UBoxComponent* HitBox = BoxComponent.Get();
		if (!IsValid(HitBox))
		{
			continue;
		}

		const FLagCompensatedHitBoxInfo* HitBoxInfo = FramePackage.HitBoxes.Find(HitBox->GetFName());
		if (!HitBoxInfo)
		{
			continue;
		}

		FLagCompensationBoxRestoreState RestoreState;
		RestoreState.BoxComponent = HitBox;
		RestoreState.Transform = HitBox->GetComponentTransform();
		RestoreState.BoxExtent = HitBox->GetUnscaledBoxExtent();
		RestoreState.CollisionEnabled = HitBox->GetCollisionEnabled();
		RestoreState.CollisionObjectType = HitBox->GetCollisionObjectType();
		RestoreState.CollisionResponses = HitBox->GetCollisionResponseToChannels();
		OutRestoreState.HitBoxes.Add(RestoreState);

		HitBox->SetWorldLocationAndRotation(HitBoxInfo->Location, HitBoxInfo->Rotation);
		HitBox->SetBoxExtent(HitBoxInfo->BoxExtent, false);
		HitBox->SetCollisionObjectType(ECC_WorldDynamic);
		HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		HitBox->SetCollisionResponseToChannel(TraceChannel, ECR_Block);
		HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	if (OutRestoreState.HitBoxes.IsEmpty())
	{
		RestoreAfterTrace(OutRestoreState);
		OutRestoreState = FLagCompensationRewindRestoreState();
		return false;
	}

	return true;
}

void ULagCompensationComponent::RestoreAfterTrace(const FLagCompensationRewindRestoreState& RestoreState)
{
	for (const FLagCompensationBoxRestoreState& HitBoxState : RestoreState.HitBoxes)
	{
		UBoxComponent* HitBox = HitBoxState.BoxComponent.Get();
		if (!IsValid(HitBox))
		{
			continue;
		}

		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitBox->SetWorldTransform(HitBoxState.Transform);
		HitBox->SetBoxExtent(HitBoxState.BoxExtent, false);
		HitBox->SetCollisionObjectType(HitBoxState.CollisionObjectType);
		HitBox->SetCollisionResponseToChannels(HitBoxState.CollisionResponses);
		HitBox->SetCollisionEnabled(HitBoxState.CollisionEnabled);
	}

	for (const FLagCompensationPrimitiveRestoreState& PrimitiveState : RestoreState.DisabledPrimitives)
	{
		UPrimitiveComponent* PrimitiveComponent = PrimitiveState.PrimitiveComponent.Get();
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(PrimitiveState.CollisionEnabled);
	}
}

bool ULagCompensationComponent::TryGetHitZoneForComponent(const UPrimitiveComponent* HitComponent,
														  FGameplayTag& OutHitZoneTag) const
{
	OutHitZoneTag = FGameplayTag();
	if (!IsValid(HitComponent))
	{
		return false;
	}

	const FGameplayTag* HitZoneTag = HitBoxZoneTags.Find(HitComponent->GetFName());
	if (!HitZoneTag || !HitZoneTag->IsValid())
	{
		return false;
	}

	OutHitZoneTag = *HitZoneTag;
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
		World->GetTimerManager().SetTimer(SnapshotTimerHandle, this, &ThisClass::SaveFramePackage, SnapshotInterval,
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
	HitBoxZoneTags.Reset();

	CreateHitBoxesFromHitZoneDefinition();
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

void ULagCompensationComponent::CreateHitBoxesFromHitZoneDefinition()
{
	const UCombatHitZoneDefinition* OwnerHitZoneDefinition = GetOwnerHitZoneDefinition();
	if (!IsValid(OwnerHitZoneDefinition))
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

	for (const FCombatHitZoneRule& ZoneRule : OwnerHitZoneDefinition->GetZoneRules())
	{
		if (!ZoneRule.HitZoneTag.IsValid())
		{
			continue;
		}

		for (const FCombatHitBoxDefinition& Definition : ZoneRule.HitBoxes)
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

			HitBoxZoneTags.Add(HitBox->GetFName(), ZoneRule.HitZoneTag);

			GeneratedHitBoxes.Add(HitBox);
			CachedHitBoxes.Add(HitBox);
		}
	}
}

const UCombatHitZoneDefinition* ULagCompensationComponent::GetOwnerHitZoneDefinition() const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->Implements<URogueDamageableTarget>())
	{
		return nullptr;
	}

	return IRogueDamageableTarget::Execute_GetCombatHitZoneDefinition(Owner);
}

void ULagCompensationComponent::CaptureBoxComponent(const UBoxComponent* BoxComponent,
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
	if (const FGameplayTag* HitZoneTag = HitBoxZoneTags.Find(BoxComponent->GetFName()))
	{
		HitBoxInfo.HitZoneTag = *HitZoneTag;
	}

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
	HitBoxInfo.BoxExtent =
		FVector(CapsuleComponent->GetScaledCapsuleRadius(), CapsuleComponent->GetScaledCapsuleRadius(),
				CapsuleComponent->GetScaledCapsuleHalfHeight());
	HitBoxInfo.HitZoneTag = RogueTags::Hit_Zone_Torso;

	OutFramePackage.HitBoxes.Add(CapsuleComponent->GetFName(), HitBoxInfo);
}

FColor ULagCompensationComponent::GetDebugColorForHitZone(const FGameplayTag& HitZoneTag, const FColor& FallbackColor)
{
	if (HitZoneTag.MatchesTag(RogueTags::Hit_Zone_Head))
	{
		return FColor::Red;
	}
	if (HitZoneTag.MatchesTag(RogueTags::Hit_Zone_Torso))
	{
		return FColor::Green;
	}
	if (HitZoneTag.MatchesTag(RogueTags::Hit_Zone_Limb))
	{
		return FColor::Blue;
	}

	return FallbackColor;
}

void ULagCompensationComponent::DrawFramePackage(const FLagCompensationFramePackage& FramePackage) const
{
	if (!bDrawDebugHitBoxes)
	{
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
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
		const FColor HitBoxColor = GetDebugColorForHitZone(HitBoxInfo.HitZoneTag, DebugDrawColor);
		DrawDebugBox(World, HitBoxInfo.Location, HitBoxInfo.BoxExtent, FQuat(HitBoxInfo.Rotation), HitBoxColor, false,
					 DebugDrawDuration);

		if (bDrawDebugHitBoxLabels)
		{
			FString Label = HitBoxPair.Key.ToString();
			if (HitBoxInfo.HitZoneTag.IsValid())
			{
				Label = FString::Printf(TEXT("%s | %s"), *HitBoxPair.Key.ToString(), *HitBoxInfo.HitZoneTag.ToString());
			}

			DrawDebugString(World, HitBoxInfo.Location, Label, nullptr, HitBoxColor, DebugDrawDuration, false);
		}
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
