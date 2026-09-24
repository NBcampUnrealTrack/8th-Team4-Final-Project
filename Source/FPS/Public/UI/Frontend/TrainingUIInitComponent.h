#pragma once

#include "CoreMinimal.h"
#include "UI/Frontend/CombatUIInitComponent.h"
#include "TrainingUIInitComponent.generated.h"

UCLASS(ClassGroup = (UI))
class FPS_API UTrainingUIInitComponent : public UCombatUIInitComponent
{
    GENERATED_BODY()
public:
    virtual bool SupportsScoreboard() const override { return false; }
    bool OpenClassSelectionForTraining();
    void CloseClassSelectionForTraining();
protected:
    virtual bool PrepareModeBindings() override;
    virtual bool CanInitializeCombatHUD(const ADominationPlayerState& ClassState) const override;
    virtual bool ShouldPresentClassSelection(const ADominationPlayerState& ClassState) const override;
    virtual bool IsAugmentModeActive() const override;
    virtual bool IsSurrenderRequestAllowed() const override;
    virtual void OnCombatHUDCreated(UShooterPlayerHUD* HUD) override;
    virtual void OnClassSelectionPresented() override;
    virtual void RequestReturnToOutGame(AShooterPlayerController& PlayerController) override;
private:
    void CompleteLocalLoading();
};
