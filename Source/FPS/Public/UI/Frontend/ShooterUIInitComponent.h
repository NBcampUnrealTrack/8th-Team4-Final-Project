// ShooterUIInitComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CommonActivatableWidget.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "ShooterUIInitComponent.generated.h"

class ABaseRoundGameState;
class UShooterUIManagerSubsystem;
struct FAugmentInventoryEntry;
class UShooterAugmentSelectScreen;
class UAugmentInventoryComponent;

/**
 * UI 초기화 트리거 겸 인게임 화면 진입점.
 * 화면을 여닫는 흐름은 전부 여기 있다.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FPS_API UShooterUIInitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// 증강 선택 화면을 Modal 레이어에 띄우고 화면을 띄운다.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowAugmentSelect(const TArray<FAugmentInventoryEntry>& InOffers);

	// 시간 초과, 데미지 입음 등으로 밖에서 닫아야 할 때 적용.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void HideAugmentSelect();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Player State가 준비 된 후에 화면을 띄운다.
	// 준비가 안 됐으면 Timer로 다시 들어온다.
	void TryStartInGameUI();

	FTimerHandle PlayerStateWaitTimerHandle;

	UFUNCTION()
	void HandleAugmentSelected(int32 OfferIndex);

	UFUNCTION()
	void HandleAugmentSelectionStateChanged();

	void RefreshAugmentSelection();
	void ScheduleInGameUIRetry();
	void SetAugmentSelectionInput(bool bSelecting);
	void UnbindRuntimeDelegates();

	// 구독 대상은 소유하지 않으며, PlayerState/GameState 교체 시 다시 바인딩한다.
	TWeakObjectPtr<UAugmentInventoryComponent> BoundInventory;
	TWeakObjectPtr<ABaseRoundGameState> BoundRoundGameState;
	bool bInGameUIReady = false;

	UShooterUIManagerSubsystem* GetUIManager() const;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass;

	// Controller가 뜰 때 처음 띄울 화면
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCommonActivatableWidget> InitialWidgetClass;
	// 처음 띄울 화면 Tag
	UPROPERTY(EditAnywhere, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag InitialWidgetLayer;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UShooterAugmentSelectScreen> AugmentSelectClass;
	// 화면에 띄워져 있는동안만 유효 없으면 비운다
	UPROPERTY(Transient)
	TObjectPtr<UShooterAugmentSelectScreen> AugmentSelectInstance;
};
