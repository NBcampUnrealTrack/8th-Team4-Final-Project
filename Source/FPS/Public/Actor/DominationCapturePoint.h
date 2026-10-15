#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationCapturePoint.generated.h"

class USceneComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class APawn;
class ADominationPointGimmick;

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
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	EDominationPointId GetPointId() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	FVector GetCaptureBoxExtent() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	bool ContainsWorldLocation(const FVector& WorldLocation) const;

	// 서버 점령 판정(캡슐이 구역에 겹치면 점령)과 같은 기준으로 폰이 구역 안인지 본다.
	// ContainsWorldLocation(중심점)만 쓰면 가장자리나 높이가 낮은 구역에서 점령 중인데도 "밖"으로 판정된다.
	// 클라이언트는 CaptureZone 콜리전이 꺼져 있을 수 있어 오버랩 대신 캡슐 크기만큼 넓힌 박스로 계산한다.
	UFUNCTION(BlueprintPure, Category = "Domination|Objective")
	bool ContainsPawn(const APawn* Pawn) const;

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

	UFUNCTION()
	void HandleActivePointChanged(EDominationPointId ActivePointId);

	UFUNCTION()
	void HandleCaptureStateChanged(FDominationCaptureState CaptureState);

	void UpdateBorderColor();
	
	void PlaceBorderDecalOnGround();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> CaptureZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Objective|Border", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDecalComponent> BorderDecal;

	// BorderColor(Vector), BorderThickness(Scalar) 파라미터를 갖는 Masked 데칼 머티리얼을 지정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective|Border", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> BorderDecalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective|Border", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "0.5"))
	float BorderThickness = 0.05f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective|Border", meta = (AllowPrivateAccess = "true", ClampMin = "5.0", Units = "cm"))
	float BorderDecalProjectionDepth = 5.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BorderDecalMID;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true"))
	EDominationPointId PointId = EDominationPointId::None;

	// 거점 점령 구역의 절반 크기(X/Y/Z). 박스 컴포넌트의 Box Extent와 동일한 의미다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm"))
	FVector CaptureBoxExtent = FVector(1000.0f, 1000.0f, 300.0f);

	// 이 거점이 활성화된 동안만 켜지는 기믹. C·B·D에만 지정하고 A/E는 비워 둔다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ADominationPointGimmick> Gimmick;

	bool bObjectiveActive = false;
};
