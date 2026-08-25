// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "FallZoneVolume.generated.h"

/**
 * 
 */
UCLASS()
class FPS_API AFallZoneVolume : public ATriggerBox
{
	GENERATED_BODY()
	
public:
	AFallZoneVolume();

private:
	UFUNCTION()
	void HandleBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UPROPERTY(EditAnywhere, Category = "Out Of Bounds", 	meta = (ClampMin = "0.0"))
	float DamageAmount = 100000.0f;
};
