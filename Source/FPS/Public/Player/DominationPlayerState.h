#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "Player/ShooterPlayerState.h"
#include "DominationPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDominationCombatClassChangedSignature,
	EDominationCombatClass,
	SelectedClass);

// 부활 대기 표시 갱신용
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDominationRespawnTimerChangedSignature);

UCLASS()
class FPS_API ADominationPlayerState : public AShooterPlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Domination|Class")
	EDominationCombatClass GetCombatClass() const;
	UFUNCTION(BlueprintPure, Category = "Domination|Class")
	EDominationCombatClass GetPreviewCombatClass() const;
	
	UFUNCTION(BlueprintPure, Category = "Domination|Class")
	bool HasSelectedCombatClass() const;
	UFUNCTION(BlueprintPure, Category = "Domination|Class")
	bool HasPreviewCombatClass() const;
	
	bool TryConfirmCombatClass(EDominationCombatClass RequestedClass);
	// 변경 허용 시점은 GameMode가 검증한다. 최초 선택의 잠금 규칙은 유지한다.
	bool TryChangeCombatClass(EDominationCombatClass RequestedClass);
	void RestoreCombatClassForReconnect(EDominationCombatClass RestoredClass);
	void ResetCombatClassSelection();
	// 확정 전 임시 선택
	bool TrySetPreviewCombatClass(EDominationCombatClass RequestedClass);
	
	UPROPERTY(BlueprintAssignable, Category = "Domination|Class")
	FOnDominationCombatClassChangedSignature OnCombatClassChanged;
	UPROPERTY(BlueprintAssignable, Category = "Domination|Class")
	FOnDominationCombatClassChangedSignature OnPreviewCombatClassChanged;

	// 서버: 부활 예약 시 호출, DelaySeconds는 감소가 적용된 실제 대기 시간
	void SetRespawnTimer(float DelaySeconds, float ReductionSeconds);

	// 부활까지 남은 초. 대기 중이 아니면 0
	UFUNCTION(BlueprintPure, Category = "Domination|Respawn")
	float GetRespawnSecondsRemaining() const;

	// 이번 부활 대기에 적용된 감소 시간(중계 비콘). 없으면 0
	UFUNCTION(BlueprintPure, Category = "Domination|Respawn")
	float GetRespawnReductionSeconds() const;

	// 서버,클라 모두에서 값이 바뀔 때 호출
	UPROPERTY(BlueprintAssignable, Category = "Domination|Respawn")
	FOnDominationRespawnTimerChangedSignature OnRespawnTimerChanged;

private:
	UFUNCTION()
	void OnRep_CombatClass();
	UFUNCTION()
	void OnRep_PreviewCombatClass();
	UFUNCTION()
	void OnRep_RespawnTimer();

	float GetServerTimeSeconds() const;

	UPROPERTY(ReplicatedUsing = OnRep_CombatClass, VisibleInstanceOnly, Category = "Domination|Class")
	EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;
	UPROPERTY(ReplicatedUsing = OnRep_PreviewCombatClass, VisibleInstanceOnly, Category = "Domination|Class")
	EDominationCombatClass PreviewCombatClass = EDominationCombatClass::Unselected;

	// 부활 예정 서버 시각. 살아 있거나 대기 중이 아니면 지난 값이 남아 있어도 남은 시간은 0
	UPROPERTY(ReplicatedUsing = OnRep_RespawnTimer)
	float RespawnEndServerTime = 0.0f;
	UPROPERTY(ReplicatedUsing = OnRep_RespawnTimer)
	float RespawnReductionSeconds = 0.0f;
};
