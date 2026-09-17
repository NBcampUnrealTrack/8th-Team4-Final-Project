// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkinnedMeshComponent.h"
#include "GameFramework/Actor.h"

#include "KillcamCameraActor.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class AWeaponBase;

struct FKillcamMeshVisibility
{
	TWeakObjectPtr<UPrimitiveComponent> Component;
	bool bOnlyOwnerSee = false;
	bool bOwnerNoSee = false;
	bool bVisible = false;
	bool bHiddenInGame = false;
	EVisibilityBasedAnimTickOption AnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
};

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool ComputeDesiredView(FVector& OutLocation, FRotator& OutRotation) const;

	void UpdateTracking(float DeltaTime);
	void UpdateFirstPersonAttachment();
	void RestoreFirstPersonAttachment();
	void UpdateFirstPersonPresentation();
	void SetMeshPresentation(USceneComponent* Root, bool bFirstPerson);
	void RestoreMeshPresentation();
	void TraceWeaponPresentation() const;
	void TraceLivePresentation() const;
	void TraceSkeletalMesh(const TCHAR* Label, USkeletalMeshComponent* Mesh, USceneComponent* ExpectedParent, FName ExpectedSocket) const;

	UPROPERTY(VisibleAnywhere, Category = "Killcam")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Killcam")
	float RotationInterpSpeed = 14.f;

	UPROPERTY(EditDefaultsOnly, Category = "Killcam", meta = (ClampMin = "0.0"))
	float LocationInterpSpeed = 14.f;

	TWeakObjectPtr<AActor> TargetActor;
	TWeakObjectPtr<AWeaponBase> PresentedWeapon;
	TWeakObjectPtr<UCameraComponent> TargetCamera;
	TWeakObjectPtr<USkeletalMeshComponent> AttachedFirstPersonMesh;
	TWeakObjectPtr<USceneComponent> SavedFirstPersonParent;
	FName SavedFirstPersonSocket;
	FTransform SavedFirstPersonTransform = FTransform::Identity;
	TArray<FKillcamMeshVisibility> SavedMeshVisibility;

	bool bHasValidView = false;
	float PresentationTraceElapsed = 1.0f;
	int32 PresentationTraceCount = 0;
};
