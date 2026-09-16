// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "KillcamCameraActor.generated.h"

class UCameraComponent;

UCLASS()
class FPS_API AKillcamCameraActor : public AActor
{
	GENERATED_BODY()

public:
	AKillcamCameraActor();

	void StartTracking(AActor* InTargetActor);

	void StopTracking();

	AActor* GetTrackedActor() const { return TargetActor.Get(); }

	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

private:
	bool ComputeDesiredView(FVector& OutLocation, FRotator& OutRotation) const;

	void UpdateTracking(float DeltaTime);

	UPROPERTY(VisibleAnywhere, Category = "Killcam")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Killcam")
	FVector ViewOffset = FVector(-45.f, 0.f, 10.f);

	UPROPERTY(EditDefaultsOnly, Category = "Killcam")
	float LocationInterpSpeed = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Killcam")
	float RotationInterpSpeed = 14.f;

	TWeakObjectPtr<AActor> TargetActor;

	bool bHasValidView = false;
};
