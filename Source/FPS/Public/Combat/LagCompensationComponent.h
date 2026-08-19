// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "LagCompensationComponent.generated.h"

class UBoxComponent;
class UCombatHitZoneDefinition;

USTRUCT()
struct FLagCompensatedHitBoxInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector BoxExtent = FVector::ZeroVector;

	UPROPERTY()
	FGameplayTag HitZoneTag;
};

USTRUCT()
struct FLagCompensationFramePackage
{
	GENERATED_BODY()

	UPROPERTY()
	float ServerTime = 0.0f;

	UPROPERTY()
	TMap<FName, FLagCompensatedHitBoxInfo> HitBoxes;
};

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FPS_API ULagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULagCompensationComponent();

	bool GetClosestFramePackage(float ServerTime, FLagCompensationFramePackage& OutFramePackage) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Lag Compensation")
	int32 GetFrameHistoryCount() const { return FrameHistory.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation", meta = (ClampMin = "0.1"))
	float MaxRecordTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation", meta = (ClampMin = "0.01"))
	float SnapshotInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation")
	FName HitBoxComponentTag = TEXT("LagCompensationHitBox");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation")
	TObjectPtr<UCombatHitZoneDefinition> HitZoneDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation|Debug")
	bool bDrawDebugHitBoxes = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation|Debug", meta = (ClampMin = "0.0"))
	float DebugDrawDuration = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation|Debug")
	FColor DebugDrawColor = FColor::Red;

private:
	void StartRecording();
	void StopRecording();
	void SaveFramePackage();
	void CacheHitBoxComponents();
	void CreateHitBoxesFromHitZoneDefinition();
	void CaptureBoxComponent(const UBoxComponent* BoxComponent, FLagCompensationFramePackage& OutFramePackage) const;
	void CaptureFallbackCapsule(FLagCompensationFramePackage& OutFramePackage) const;
	void DrawFramePackage(const FLagCompensationFramePackage& FramePackage) const;
	void TrimFrameHistory(float CurrentServerTime);

	bool ShouldRecordHistory() const;

	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> CachedHitBoxes;

	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> GeneratedHitBoxes;

	UPROPERTY()
	TMap<FName, FGameplayTag> HitBoxZoneTags;

	UPROPERTY()
	TArray<FLagCompensationFramePackage> FrameHistory;

	FTimerHandle SnapshotTimerHandle;
};
