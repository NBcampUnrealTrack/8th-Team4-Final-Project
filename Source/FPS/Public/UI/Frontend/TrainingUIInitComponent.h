#pragma once

#include "CoreMinimal.h"
#include "UI/Frontend/CombatUIInitComponent.h"
#include "TrainingUIInitComponent.generated.h"

class UUserWidget;
class UWidget;

UCLASS(ClassGroup = (UI))
class FPS_API UTrainingUIInitComponent : public UCombatUIInitComponent
{
    GENERATED_BODY()
public:
    UTrainingUIInitComponent();
    virtual bool SupportsScoreboard() const override { return false; }
protected:
    virtual bool PrepareModeBindings() override;
    virtual bool CanInitializeCombatHUD(const ADominationPlayerState& ClassState) const override;
    virtual bool ShouldPresentClassSelection(const ADominationPlayerState& ClassState) const override;
    virtual bool IsAugmentModeActive() const override;
    virtual bool IsSurrenderRequestAllowed() const override;
    virtual void OnCombatHUDCreated(UShooterPlayerHUD* HUD) override;
    virtual void OnClassSelectionPresented() override;
private:
    void HideRuleOnlyWidgets(UUserWidget& RootWidget);
    void CompleteLocalLoading();
    UPROPERTY(EditDefaultsOnly, Category = "Training|HUD")
    TArray<TSubclassOf<UWidget>> SuppressedHUDWidgetClasses;
    UPROPERTY(EditDefaultsOnly, Category = "Training|HUD")
    TArray<FName> SuppressedHUDWidgetNames;
};
