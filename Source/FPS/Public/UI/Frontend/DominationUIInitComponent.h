#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DominationUIInitComponent.generated.h"

class ADominationGameState;
class UShooterPlayerHUD;
class UShooterPrimaryGameLayout;
class UShooterUIManagerSubsystem;

/**
 * Domination의 공통 전투 HUD 수명주기만 담당한다.
 * Round 전용 상점, 항복, 증강 선택 화면은 연결하지 않는다.
 */
UCLASS(ClassGroup = (UI))
class FPS_API UDominationUIInitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDominationUIInitComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleDominationFlowStateChanged();

	void TryInitializePlayerHUD();
	void ScheduleInitializationRetry();
	void ClearInitializationRetry();
	void BindToGameState(ADominationGameState* DominationGameState);
	void UnbindFromGameState();
	UShooterUIManagerSubsystem* GetUIManager() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterPlayerHUD> InitialWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true", Categories = "UI.Layer"))
	FGameplayTag InitialWidgetLayer;

	UPROPERTY(Transient)
	TObjectPtr<UShooterPlayerHUD> InitialWidgetInstance;

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	FTimerHandle InitializationRetryTimerHandle;
};
