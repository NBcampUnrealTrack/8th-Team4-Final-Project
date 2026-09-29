#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "DominationRespawnTimerWidget.generated.h"

class ADominationPlayerState;
class UWidget;

/**
 * 사망 후 부활까지 남은 시간을 보여주는 HUD.
 * - 중계 비콘으로 부활 대기가 줄어들면 그 감소량도 함께 보여준다.
 * - Tick을 쓰지 않는다. 부활 예약·생존 상태 변화 이벤트로 켜고 끄고, 대기 중에만 RefreshInterval 간격으로 갱신한다.
 * - 계산은 C++에서 하고 모양·배치는 Blueprint에서 한다.
 */
UCLASS(Abstract, Blueprintable)
class FPS_API UDominationRespawnTimerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 죽어서 부활을 기다리는 중인지
	UFUNCTION(BlueprintPure, Category = "Domination|Respawn HUD")
	bool IsWaitingForRespawn() const;

	// 부활까지 남은 초(올림). 대기 중이 아니면 0
	UFUNCTION(BlueprintPure, Category = "Domination|Respawn HUD")
	int32 GetRespawnSecondsRemaining() const;

	// 이번 대기에 적용된 감소 시간(초, 반올림). 중계 비콘이 없으면 0
	UFUNCTION(BlueprintPure, Category = "Domination|Respawn HUD")
	int32 GetRespawnReductionSeconds() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 표시(남은 초, 감소 안내)를 다시 그릴 때 호출된다. 대기 시작·종료 시 즉시 + 대기 중 주기적으로.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Respawn HUD")
	void BP_OnRespawnTimerRefresh();

	// 타이머 루트. 대기 중일 때만 보이도록 C++이 표시 여부를 설정한다.
	UPROPERTY(BlueprintReadOnly, Category = "Domination|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TimerRoot;

private:
	UFUNCTION()
	void HandleRespawnStateChanged();

	UFUNCTION()
	void HandleAliveStateChanged(bool bNewIsAlive);

	void TryBindPlayerState();
	void UnbindPlayerState();
	void Refresh();
	void HandleRefreshTimer();
	ADominationPlayerState* GetBoundPlayerState() const;

	TWeakObjectPtr<ADominationPlayerState> BoundPlayerState;
	FTimerHandle RefreshTimerHandle;
	FTimerHandle BindRetryTimerHandle;

	// 대기 중 남은 초 갱신 간격 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Respawn HUD", meta = (ClampMin = "0.05", Units = "s"))
	float RefreshInterval = 0.25f;
};
