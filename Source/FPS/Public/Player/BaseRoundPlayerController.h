#pragma once

#include "CoreMinimal.h"
#include "Player/ShooterPlayerController.h"
#include "BaseRoundPlayerController.generated.h"

class UShooterUIInitComponent;

UCLASS(Abstract)
class FPS_API ABaseRoundPlayerController : public AShooterPlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	bool IsRoundInputEnabled() const;

	virtual bool CanProcessGameplayInput() const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	virtual bool CanUseCombatInput() const;

	void SetRoundInputEnabled(bool bEnabled);

	// 로컬 HUD와 인게임 입력 계층이 준비된 뒤 서버의 첫 라운드 게이트를 연다.
	UFUNCTION(BlueprintCallable, Category = "Game Flow|Round")
	void NotifyClientReadyForGameFlow();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void RefreshLoadingScreenGameStateBinding(
		ULoadingScreenSubsystem* LoadingScreenSubsystem) override;

	UFUNCTION(Client, Reliable)
	void ClientSetRoundInputEnabled(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void ServerNotifyClientReadyForGameFlow();

private:
	void Input_ToggleShop();
	UShooterUIInitComponent* GetUIInitComponent() const;

	void ApplyRoundInputEnabled(bool bEnabled);
	void SetRoundInputLockApplied(bool bApplied);
	void SetGameplayMappingContextEnabled(bool bEnabled);

	bool bIsRoundInputEnabled = true;
	bool bIsRoundInputLockApplied = false;

	// Temp log
	mutable uint32 LastTempInputMask = MAX_uint32;
};
