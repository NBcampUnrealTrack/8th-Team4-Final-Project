#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Round/BaseRoundPlayerController.h"
#include "BombPlayerController.generated.h"

class UBaseBombGameFlowWidget;
class UDataJackInputConfig;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveInteractionStateChangedSignature, bool, bIsActive);

UCLASS()
class FPS_API ABombPlayerController : public ABaseRoundPlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool IsObjectiveInteractionActive() const;

	// Integration point for combat: fire and reload must query this gate.
	// Sustained fire must also stop when objective interaction becomes active.
	virtual bool CanUseCombatInput() const override;

	void SetObjectiveInteractionActive(bool bActive);

protected:
	virtual void SetupInputComponent() override;
	virtual void HandleRoundInputStateChanged(bool bEnabled) override;

	UFUNCTION(Client, Reliable)
	void ClientSetObjectiveInteractionActive(bool bActive);

	UFUNCTION(Server, Reliable)
	void ServerStartDataJackInteraction();

	UFUNCTION(Server, Reliable)
	void ServerStopDataJackInteraction();

	UFUNCTION(Server, Reliable)
	void ServerStartDataJackDrop();

	UFUNCTION(Server, Reliable)
	void ServerStopDataJackDrop();

private:
	void CreateGameFlowWidget();
	void RemoveGameFlowWidget();
	void ApplyObjectiveInteractionActive(bool bActive);
	void SetObjectiveMovementInputLockApplied(bool bApplied);
	void SetDataJackMappingContextEnabled(bool bEnabled);
	void Input_DataJackInteractionStarted();
	void Input_DataJackInteractionStopped();
	void Input_DataJackDropStarted();
	void Input_DataJackDropStopped();

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDataJackInputConfig> DataJackInputConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Game Flow|UI")
	TSubclassOf<UBaseBombGameFlowWidget> GameFlowWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UBaseBombGameFlowWidget> GameFlowWidget;

	bool bIsObjectiveInteractionActive = false;
	bool bIsObjectiveMovementInputLockApplied = false;
	bool bIsDataJackMappingContextEnabled = false;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Data Jack")
	FOnObjectiveInteractionStateChangedSignature OnObjectiveInteractionStateChanged;
};
