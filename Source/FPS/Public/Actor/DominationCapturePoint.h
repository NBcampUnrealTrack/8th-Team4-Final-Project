#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationCapturePoint.generated.h"

class USceneComponent;
class UBoxComponent;
class UPrimitiveComponent;
class APawn;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDominationCapturePointPawnChanged, APawn*);

/**
 * 맵에 배치되는 Domination 거점이다.
 * 위치와 점령 범위만 소유하며 점령 규칙은 ObjectiveManager가 처리한다.
 */
UCLASS(Blueprintable)
class FPS_API ADominationCapturePoint : public AActor
{
	GENERATED_BODY()

public:
	ADominationCapturePoint();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	EDominationPointId GetPointId() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	FVector GetCaptureBoxExtent() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	bool ContainsWorldLocation(const FVector& WorldLocation) const;

	void GetOverlappingPawns(TArray<APawn*>& OutPawns) const;
	void SetObjectiveActive(bool bNewActive);

	FOnDominationCapturePointPawnChanged OnPawnEntered;
	FOnDominationCapturePointPawnChanged OnPawnExited;

private:
	UFUNCTION()
	void HandleCaptureZoneBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleCaptureZoneEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> CaptureZone;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true"))
	EDominationPointId PointId = EDominationPointId::None;

	// 거점 점령 구역의 절반 크기(X/Y/Z). 박스 컴포넌트의 Box Extent와 동일한 의미다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm"))
	FVector CaptureBoxExtent = FVector(1000.0f, 1000.0f, 300.0f);

	bool bObjectiveActive = false;
};
