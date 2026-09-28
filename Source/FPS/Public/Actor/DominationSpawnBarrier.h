#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "DominationSpawnBarrier.generated.h"

class AGameStateBase;
class ADominationCapturePoint;
class ADominationGameState;
class ADominationSpawnVolume;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class FPS_API ADominationSpawnBarrier : public AActor
{
	GENERATED_BODY()

public:
	ADominationSpawnBarrier();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 현재 이 장벽이 보호 중인 팀. 비활성 상태면 None을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Domination|SpawnBarrier")
	EGameTeam GetOwnerTeam() const { return CurrentGuardedTeam; }
	static const TArray<TWeakObjectPtr<ADominationSpawnBarrier>>& GetActiveBarriers() { return ActiveBarriers; }

private:
	// GameState는 UFUNCTION 델리게이트가 아니라 World의 일반 이벤트라 AddUObject로 바인딩
	// 클라이언트는 BeginPlay 시점에 GameState 리플리케이션이 아직 안 끝났을 수 있어서 필요
	void HandleGameStateSet(AGameStateBase* NewGameState);

	// BoundGameState의 현재 ActivePointId로 다시 계산, 값이 아직 None이면 스스로 재호출
	void RefreshFromCurrentGameState();

	UFUNCTION()
	void HandleActivePointChanged(EDominationPointId ActivePointId);

	UFUNCTION()
	void HandleDetectionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void RefreshBarrierState(EDominationPointId ActivePointId);
	void SetBarrierActive(EGameTeam NewGuardedTeam);
	// 메시 비지빌리티가 안보이는 현상 개선
	void ForceRefreshMeshVisibility();
	
	void ApplyMeshVisibility(bool bNewHidden);
	bool FindTeamSpawnVolume(EDominationPointId ActivePointId, EGameTeam Team, ADominationSpawnVolume*& OutSpawnVolume) const;
	const ADominationCapturePoint* FindCapturePoint(EDominationPointId PointId) const;

	// Pawn이면 자신의 PlayerState, 아니면(수류탄 등 투척물) GetInstigator()의 PlayerState에서 팀을 구함
	static EGameTeam ResolveActorTeam(const AActor* Actor);

	// 실제 차단 콜리전. Pawn과 히트스캔 트레이스 채널(ECC_Visibility)을 막음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|SpawnBarrier", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BarrierZone;

	// 순수 오버랩 감지용. BarrierZone보다 살짝 크게 잡아 아군 진입을 감지해 통과 허용 목록에 등록
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|SpawnBarrier", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> DetectionZone;

	// 연출 전용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|SpawnBarrier", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BarrierMesh;

	// 이 장벽이 짝지어진 스폰 볼륨
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Domination|SpawnBarrier", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ADominationSpawnVolume> BoundSpawnVolume;

	UPROPERTY(Transient)
	EGameTeam CurrentGuardedTeam = EGameTeam::None;

	// 현재 통과 허용이 등록된 액터 목록. 보호 팀이 바뀌면 이걸 기준으로 예전 등록을 전부 해제한다.
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> IgnoringActors;

	UPROPERTY(Transient)
	TObjectPtr<ADominationGameState> BoundGameState;

	FTimerHandle GameStateRetryTimer;
	
	FVector MeshVisibleScale = FVector::OneVector;

	static TArray<TWeakObjectPtr<ADominationSpawnBarrier>> ActiveBarriers;
};
