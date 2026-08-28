#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FieldPickup.generated.h"

class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UAbilitySystemComponent;

/**
 * 맵에 상시 배치되는 공용 GAS 픽업 액터다.
 *
 * 서버가 오버랩, GameplayEffect 적용, 소비 및 리스폰을 판정한다.
 * 체력/보호막 같은 효과 의미와 실제 수치 보정은 GameplayEffect와 AttributeSet이 담당한다.
 * BP 자식 클래스에서는 메시, GE, 수치, 리스폰 시간과 연출만 지정한다.
 */
UCLASS(Blueprintable)
class FPS_API AFieldPickup : public AActor
{
	GENERATED_BODY()

public:
	AFieldPickup();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Field Pickup")
	bool IsAvailable() const { return bIsAvailable; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandlePickupTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_IsAvailable();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayPickupEffects();

	bool TryConsume(AActor* CandidateActor);
	bool TryApplyGameplayEffect(UAbilitySystemComponent& TargetAbilitySystem) const;
	bool HasValidConfiguration() const;

	void ConsumePickup();
	void HandleRespawnTimerExpired();
	void EvaluateOverlappingPawns();
	void SetAvailable(bool bNewAvailable);
	void ApplyAvailabilityState();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Pickup|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Pickup|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PresentationRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Pickup|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Pickup|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> AmbientNiagara;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Pickup|Components",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> PickupTrigger;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Pickup|Effect",
		meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> PickupGameplayEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Pickup|Effect",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float PickupMagnitude = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Pickup|Respawn",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.1", Units = "s"))
	float RespawnDelay = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Pickup|Presentation",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> PickupSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Pickup|Presentation",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> PickupNiagara;

	UPROPERTY(ReplicatedUsing = OnRep_IsAvailable, VisibleInstanceOnly, BlueprintReadOnly,
		Category = "Field Pickup|Runtime", meta = (AllowPrivateAccess = "true"))
	bool bIsAvailable = true;

	// 같은 프레임의 중복 오버랩과 GE 적용 중 재진입을 막는 서버 로컬 가드다.
	bool bConsumeInProgress = false;
	FTimerHandle RespawnTimerHandle;
};
