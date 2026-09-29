#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/TeamTypes.h"
#include "TimerManager.h"
#include "DominationPointGimmick.generated.h"

class ADominationCapturePoint;
class APawn;
class APlayerController;
class APlayerState;
class USceneComponent;
class UPrimitiveComponent;
class USphereComponent;
class UGameplayEffect;
class UAudioComponent;
class UNiagaraSystem;
class USoundBase;

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

// 기믹 표시 상태가 바뀌었을 때(복제 값 변경, 상호작용 범위 출입) 모든 머신에서 호출된다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDominationGimmickStateChanged,
	ADominationPointGimmick*,
	Gimmick);

// 기믹 확보가 완료됐을 때 모든 클라이언트에서 1회 호출된다. bTakeover: 상대가 소유하던 중계 비콘을 빼앗은 경우
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnDominationGimmickSecured,
	ADominationPointGimmick*, Gimmick,
	EGameTeam, SecuringTeam,
	bool, bTakeover);

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

	// 서버·클라 공용: 복제된 상태만 사용한다. UI의 [F] 안내 표시 판단에도 사용.
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	bool CanBeUsedBy(const APawn* Pawn) const;
	static ADominationPointGimmick* FindUsableGimmick(const APawn* Pawn);

	// UI용: 현재 켜져 있는 기믹(활성 거점의 기믹). 없으면 nullptr
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick", meta = (WorldContext = "WorldContextObject"))
	static ADominationPointGimmick* GetActiveGimmick(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	EDominationGimmickType GetGimmickType() const { return GimmickType; }

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	bool IsGimmickActive() const { return bGimmickActive; }

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	EGameTeam GetInteractingTeam() const { return InteractingTeam; }

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	APlayerState* GetInteractingPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	EGameTeam GetOwningTeam() const { return OwningTeam; }

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	EGameTeam GetReconTeam() const { return ReconTeam; }

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	float GetReconTimeRemaining() const;

	// 서버: 홀딩 어빌리티가 호출한다.
	bool BeginInteraction(APlayerController* Controller);
	void CancelInteraction(APlayerController* Controller);

	FOnDominationGimmickInteractionEnded OnInteractionEnded;

	// UI 갱신용. 틱 대신 이 이벤트로 표시를 바꾼다.
	UPROPERTY(BlueprintAssignable, Category = "Domination|Gimmick")
	FOnDominationGimmickStateChanged OnGimmickStateChanged;

	// UI 알림용. 확보 완료 순간에만 호출된다(중단은 호출되지 않음).
	UPROPERTY(BlueprintAssignable, Category = "Domination|Gimmick")
	FOnDominationGimmickSecured OnGimmickSecured;

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	float GetInteractionProgress() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick")
	float GetCooldownRemaining() const;

protected:
	// --- 상태 전환 이벤트 (이펙트·사운드용) ---
	// 모든 머신에서 값이 바뀐 순간 한 번씩만 호출된다. 늦게 접속한 클라이언트도 현재 상태 기준으로 한 번 받는다.

	// 거점이 활성화되어 기믹이 켜짐 / 꺼짐
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnActivated();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnDeactivated();

	// 누군가 누르기 시작함 (진행 루프 시작)
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnInteractionStarted(EGameTeam Team);

	// 상호작용이 끝남 (루프 정지 + 완료/중단 사운드). 모든 클라이언트에서 호출된다.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnInteractionEnded(bool bCompleted, EGameTeam Team);

	// 정찰 스캔 시작 / 종료 (정찰 단말)
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnReconStarted(EGameTeam Team);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnReconEnded();

	// 소유 팀 변경 (중계 비콘). 소유가 풀리면 None
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick|AV")
	void BP_OnOwnerChanged(EGameTeam NewTeam);

private:
	UFUNCTION()
	void OnRep_GimmickState();

	// 로컬 플레이어의 [F] 안내 표시 갱신용
	UFUNCTION()
	void HandleInteractionSphereOverlapChanged(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleInteractionSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	// 드물게 보내는 신호라 유실되지 않도록 Reliable
	UFUNCTION(NetMulticast, Reliable)
	void MulticastInteractionEnded(bool bCompleted, EGameTeam Team, bool bTakeover);

	// 이펙트·사운드 재생. 데디케이티드 서버에서는 아무것도 하지 않는다.
	bool ShouldPlayCosmetics() const;
	void PlayInteractionStartedCosmetics();
	void PlayInteractionEndedCosmetics(bool bCompleted);
	void PlayReconStartedCosmetics();
	void StopInteractionLoopSound();

	void HandleInteractionTick();
	void EndInteraction(bool bCompleted);
	void ApplyRecon(EGameTeam Team);
	void RunReconScan();
	void StopRecon();
	void ApplyRespawnRelay(EGameTeam Team);
	void ClearRespawnRelay();
	void NotifyStateChanged();
	// 마지막으로 알린 값과 비교해 바뀐 것만 상태 전환 이벤트로 알린다.
	void BroadcastStateTransitions();
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

	// --- 이펙트·사운드 (에셋만 지정하면 재생된다. 비워 두면 재생하지 않는다) ---
	// 상호작용 시작 순간 1회
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|AV", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> InteractionStartSound;

	// 누르고 있는 동안 반복 (루프 설정된 사운드). 기믹에 붙어 재생되고 끝나면 페이드아웃
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|AV", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> InteractionLoopSound;

	// 확보 완료 순간 1회
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|AV", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> InteractionCompleteSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|AV", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
	float InteractionLoopFadeOutDuration = 0.15f;

	// 정찰 단말 확보 완료(스캔 시작) 순간 스캔 중심(거점)에서 1회 재생. Niagara 사용자 변수 User.Radius(cm)에 스캔 반경을 넘긴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gimmick|AV", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> ReconScanEffect;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> InteractionLoopAudio;

	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	bool bGimmickActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	EGameTeam InteractingTeam = EGameTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	float InteractionStartServerTime = 0.0f;

	// 상호작용 중인 플레이어 (UI: 내가 누르는 중일 때만 진행 바 표시)
	UPROPERTY(ReplicatedUsing = OnRep_GimmickState, BlueprintReadOnly, Category = "Domination|Gimmick", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APlayerState> InteractingPlayerState;

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

	// 상태 전환 이벤트 비교용 (마지막으로 알린 값)
	bool bNotifiedActive = false;
	EGameTeam NotifiedInteractingTeam = EGameTeam::None;
	EGameTeam NotifiedReconTeam = EGameTeam::None;
	EGameTeam NotifiedOwningTeam = EGameTeam::None;
	// 스캔 중심(연결된 거점 위치). 클라이언트도 스캔 이펙트 위치로 쓰므로 복제한다.
	UPROPERTY(Replicated)
	FVector_NetQuantize ScanCenter = FVector::ZeroVector;
	int32 ScansRemaining = 0;
	FTimerHandle InteractionTickHandle;
	FTimerHandle ReconScanHandle;

	static constexpr float InteractionTickInterval = 0.1f;
};
