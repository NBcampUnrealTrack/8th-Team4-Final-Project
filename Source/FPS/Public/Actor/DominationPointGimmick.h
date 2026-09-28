#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/TeamTypes.h"
#include "TimerManager.h"
#include "DominationPointGimmick.generated.h"

class ADominationCapturePoint;
class APawn;
class APlayerController;
class USceneComponent;
class USphereComponent;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EDominationGimmickType : uint8
{
	Recon UMETA(DisplayName = "Recon Terminal"),
	RespawnRelay UMETA(DisplayName = "Respawn Relay Beacon")
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnDominationGimmickInteractionEnded,
	APlayerController*,
	bool /*bCompleted*/);

UCLASS(Blueprintable)
class FPS_API ADominationPointGimmick : public AActor
{
	GENERATED_BODY()

public:
	ADominationPointGimmick();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버: 연결된 거점이 활성 상태가 바뀔 때 호출한다.
	void SetGimmickActive(bool bActive, const ADominationCapturePoint* OwnerPoint);

	// 서버·클라 공용: 복제된 상태만 사용한다.
	bool CanBeUsedBy(const APawn* Pawn) const;
	static ADominationPointGimmick* FindUsableGimmick(const APawn* Pawn);

	// 서버: 홀딩 어빌리티가 호출한다.
	bool BeginInteraction(APlayerController* Controller);
	void CancelInteraction(APlayerController* Controller);

	FOnDominationGimmickInteractionEnded OnInteractionEnded;

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	float GetInteractionProgress() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	float GetCooldownRemaining() const;

protected:
	// 복제 상태가 바뀔 때마다(서버 포함) 호출된다. 마커·이펙트·사운드는 BP에서 처리한다.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnGimmickStateChanged();

	// 상호작용이 끝났을 때 모든 클라이언트에서 호출된다(완료/중단 사운드용).
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnInteractionEnded(bool bCompleted, EGameTeam Team);

private:
	UFUNCTION()
	void OnRep_GimmickState();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastInteractionEnded(bool bCompleted, EGameTeam Team);

	void HandleInteractionTick();
	void EndInteraction(bool bCompleted);
	void ApplyRecon(EGameTeam Team);
	void RunReconScan();
	void StopRecon();
	void ApplyRespawnRelay(EGameTeam Team);
	void ClearRespawnRelay();
	void NotifyStateChanged();
	float GetServerTime() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	EDominationGimmickType GimmickType = EDominationGimmickType::Recon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float HoldDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|Recon", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", Units = "cm"))
	float ReconRadius = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|Recon", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 ReconScanCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|Recon", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float ReconScanInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|Recon", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float ReconCooldown = 30.0f;

	// State.Combat.Revealed 를 1.5초 부여하는 GameplayEffect (GE_Domination_ReconReveal)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|Recon", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> RevealEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|Relay", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float RespawnDelayReduction = 2.0f;

	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	bool bGimmickActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	EGameTeam InteractingTeam = EGameTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	float InteractionStartServerTime = 0.0f;

	// 중계 비콘 소유 팀
	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	EGameTeam OwningTeam = EGameTeam::None;

	// 정찰 스캔 중인 팀 (상대 팀 경고 UI용)
	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	EGameTeam ReconTeam = EGameTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	float ReconEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	float CooldownEndServerTime = 0.0f;

	TWeakObjectPtr<APlayerController> InteractingController;
	FVector ScanCenter = FVector::ZeroVector;
	int32 ScansRemaining = 0;
	FTimerHandle InteractionTickHandle;
	FTimerHandle ReconScanHandle;

	static constexpr float InteractionTickInterval = 0.1f;
};
