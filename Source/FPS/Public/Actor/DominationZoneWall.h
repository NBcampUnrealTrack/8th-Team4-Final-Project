#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "DominationZoneWall.generated.h"

class AGameStateBase;
class ADominationGameState;
class APawn;
class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

// 팀 구분 없이 모두를 막는 구역 제한 벽. ActiveDuringPoints에 든 거점이 활성일 때만 켜진다.
// 배치하고 각각 해당 거점 ID를 지정하면, 거점이 넘어갈 때 이전 벽은 사라지고 다음 벽이 켜진다.
UCLASS(Blueprintable)
class FPS_API ADominationZoneWall : public AActor
{
	GENERATED_BODY()

public:
	ADominationZoneWall();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Domination|ZoneWall")
	bool IsWallActive() const { return bWallActive; }

private:
	void HandleGameStateSet(AGameStateBase* NewGameState);
	void RefreshFromCurrentGameState();

	UFUNCTION()
	void HandleActivePointChanged(EDominationPointId ActivePointId);

	void SetWallActive(bool bNewActive);

	// 벽이 켜질 때 이미 벽 바깥에 있던 플레이어가 맵 안으로 돌아올 수 있도록 한 방향 통과를 허용한다.
	UFUNCTION()
	void HandleDetectionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleDetectionEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// 벽의 앞(Forward) 방향을 맵 안쪽으로 보고, 폰이 그 반대편에 있으면 바깥으로 판단한다.
	bool IsOutsideOfWall(const AActor* Actor) const;

	void SetIgnoredByPawn(APawn* Pawn, bool bIgnore);
	void ClearIgnoringActors();

	// 실제 차단 콜리전. 활성일 때만 QueryOnly로 켜진다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|ZoneWall", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> WallZone;

	// 화살표 추가
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|ZoneWall", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArrowComponent> InsideArrow;

	// 순수 오버랩 감지용. WallZone보다 두께 방향으로 DetectionMargin만큼 크게 잡아 접근하는 폰을 감지한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|ZoneWall", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DetectionZone;

	// DetectionZone이 WallZone보다 두께(X) 방향으로 양쪽에 더 커지는 길이(cm).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|ZoneWall", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float DetectionMargin = 100.0f;

	// 현재 이 벽을 무시하도록 등록된 폰 목록. 벽이 꺼지거나 폰이 벽을 벗어나면 해제한다.
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<APawn>> IgnoringPawns;

	// 연출 전용. 비활성일 때는 숨긴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|ZoneWall", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WallMesh;

	// 이 벽이 켜져 있어야 하는 활성 거점 목록. 비어 있으면 절대 켜지지 않는다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Domination|ZoneWall", meta = (AllowPrivateAccess = "true"))
	TArray<EDominationPointId> ActiveDuringPoints;

	// true면 히트스캔(ECC_Visibility)도 막는다. 기본은 플레이어 이동만 막는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|ZoneWall", meta = (AllowPrivateAccess = "true"))
	bool bBlockBullets = false;

	UPROPERTY(Transient)
	bool bWallActive = false;

	UPROPERTY(Transient)
	TObjectPtr<ADominationGameState> BoundGameState;

	FTimerHandle GameStateRetryTimer;

	FVector WallVisibleScale = FVector::OneVector;
};
