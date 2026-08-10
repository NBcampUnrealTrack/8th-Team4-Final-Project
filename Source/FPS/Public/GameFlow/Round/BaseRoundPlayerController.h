#pragma once

#include "CoreMinimal.h"
#include "Player/ShooterPlayerController.h"
#include "BaseRoundPlayerController.generated.h"

UCLASS(Abstract)
class FPS_API ABaseRoundPlayerController : public AShooterPlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	bool IsRoundInputEnabled() const;

	void SetRoundInputEnabled(bool bEnabled);

protected:
	UFUNCTION(Client, Reliable)
	void ClientSetRoundInputEnabled(bool bEnabled);

	virtual void HandleRoundInputStateChanged(bool bEnabled);

private:
	void ApplyRoundInputEnabled(bool bEnabled);
	void SetRoundInputLockApplied(bool bApplied);
	void SetGameplayMappingContextEnabled(bool bEnabled);

	bool bIsRoundInputEnabled = true;
	bool bIsRoundInputLockApplied = false;
};
