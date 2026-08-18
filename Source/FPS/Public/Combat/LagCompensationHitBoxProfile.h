// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LagCompensationHitBoxProfile.generated.h"

USTRUCT(BlueprintType)
struct FLagCompensationHitBoxDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation")
	FName HitBoxName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation")
	FName AttachBoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation")
	FVector BoxExtent = FVector(10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation")
	FRotator RelativeRotation = FRotator::ZeroRotator;
};

UCLASS(BlueprintType)
class FPS_API ULagCompensationHitBoxProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Lag Compensation")
	TArray<FLagCompensationHitBoxDefinition> HitBoxes;
};
