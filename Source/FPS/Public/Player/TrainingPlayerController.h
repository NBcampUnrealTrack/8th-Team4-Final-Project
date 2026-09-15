#pragma once

#include "CoreMinimal.h"
#include "Interfaces/RogueCombatClassSelector.h"
#include "Player/ShooterPlayerController.h"
#include "TrainingPlayerController.generated.h"

class UTrainingUIInitComponent;

// A sibling of DominationPlayerController: all shooting input still lives in ShooterPlayerController.
UCLASS()
class FPS_API ATrainingPlayerController : public AShooterPlayerController, public IRogueCombatClassSelector
{
    GENERATED_BODY()
public:
    ATrainingPlayerController();
    virtual bool CanProcessGameplayInput() const override;
    UFUNCTION(BlueprintCallable, Category = "Training|Class")
    virtual void SelectCombatClass(EDominationCombatClass RequestedClass) override;
    UFUNCTION(BlueprintCallable, Category = "Training|Class")
    virtual void SetPreviewCombatClass(EDominationCombatClass RequestedClass) override;
    virtual float GetCombatClassSelectionTimeRemaining() const override;
protected:
    virtual void SetupInputComponent() override;
private:
    UFUNCTION(Server, Reliable)
    void ServerSelectCombatClass(EDominationCombatClass RequestedClass);
    UFUNCTION(Server, Reliable)
    void ServerSetPreviewCombatClass(EDominationCombatClass RequestedClass);
    UFUNCTION(Client, Reliable)
    void ClientCombatClassSelectionFailed(const FText& Reason);
    void Input_ToggleAugmentSelect();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|UI", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTrainingUIInitComponent> UIInitComponent;
};
