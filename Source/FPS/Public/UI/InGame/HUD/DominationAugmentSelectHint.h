// DominationAugmentSelectHint.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DominationAugmentSelectHint.generated.h"

class UCommonTextBlock;
class UAugmentInventoryComponent;
class UCombatUIInitComponent;

/**
 * 증강 선택 화면을 보류한 동안 HUD에 뜨는 힌트
 * 증강 선택 시간과 B키 안내 문구 표시
 */
UCLASS(Abstract, Blueprintable)
class FPS_API UDominationAugmentSelectHint : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Dominaition|Augment UI",
		meta = (DisplayName = "힌트 표시 상태 변경"))
	void BP_OnHintVisibilityChanged(bool bShouldShow);
	UFUNCTION(BlueprintImplementableEvent, Category = "Augment|Banner",
	meta = (DisplayName = "남은 시간 머터리얼 갱신"))
	void BP_OnCountdownProgressChanged(float InProgress);
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCommonTextBlock> Text_RemainingSeconds;
	
private:
	void SchedulePresentationRefresh();
	void TryBindDependencies();
	void ShutDownForOtherMode();
	void ResetTransientState();
	void RefreshPresentation();
	void RefreshCountdown();
	
	void StartCountdown();
	void StopCountdown();
		
	UFUNCTION()
	void HandleOffersChanged();
	UFUNCTION()
	void HandleDismissStateChanged();
	UFUNCTION()
	void HandlePresentationChanged();
	
	UAugmentInventoryComponent* GetInventory() const;
	UCombatUIInitComponent* GetUIInitComp() const;
	
	TWeakObjectPtr<UAugmentInventoryComponent> BoundInv;
	TWeakObjectPtr<UCombatUIInitComponent> BoundUIInit;
	FTimerHandle DependencyBindRetryTimerHandle;
	FTimerHandle PresentationRefreshTimerHandle;
	FTimerHandle CountdownTimerHandle;
	
	// ResetTransientState()가 초기화
	bool bHasVisibilityState = false;
	bool bLastShouldShow = false;
	bool bPresentationRefreshPending = false;
	int32 DisplaySeconds = 0;
	
	static constexpr float DependencyBindRetryInterval = 0.05f;
	static constexpr float TotalDuration = 20.f;
};
