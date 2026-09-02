#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AugmentPhaseBarrier.generated.h"

class ABaseRoundGameState;
class UStaticMeshComponent;

/**
 * 증강 선택 페이즈 동안 베이스 출구를 막는 장벽이다.
 *
 * BP 자식 클래스에서 메시와 충돌을 지정하고 맵에 배치한다.
 * 배치 인스턴스의 Transform을 그대로 사용해 페이즈용 장벽을 생성하므로,
 * 각 인스턴스의 위치/회전/스케일은 에디터에서 자유롭게 조절할 수 있다.
 */
UCLASS(Blueprintable)
class FPS_API AAugmentPhaseBarrier : public AActor
{
	GENERATED_BODY()

public:
	AAugmentPhaseBarrier();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleRoundStateChanged();

	UFUNCTION()
	void OnRep_IsRuntimeBarrier();

	void UpdateForCurrentPhase();
	void SpawnRuntimeBarrier();
	void DestroyRuntimeBarrier();
	void ApplyInstanceRole();

	// BP 자식 클래스에서 장벽 메시와 충돌 프리셋을 지정한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow|Augment Barrier",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BarrierMesh;

	UPROPERTY(Transient)
	TObjectPtr<ABaseRoundGameState> BoundRoundGameState;

	// 맵 배치 인스턴스가 현재 페이즈에 생성한 실제 장벽이다.
	UPROPERTY(Transient)
	TObjectPtr<AAugmentPhaseBarrier> RuntimeBarrier;

	UPROPERTY(ReplicatedUsing = OnRep_IsRuntimeBarrier)
	bool bIsRuntimeBarrier = false;
};


