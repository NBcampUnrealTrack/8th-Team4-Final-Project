#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "DominationZoneWall.generated.h"

class AGameStateBase;
class ADominationGameState;
class UBoxComponent;
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

	// 실제 차단 콜리전. 활성일 때만 QueryOnly로 켜진다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|ZoneWall", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> WallZone;

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
