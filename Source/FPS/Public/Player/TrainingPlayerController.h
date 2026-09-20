#pragma once

#include "CoreMinimal.h"
#include "Interfaces/RogueCombatClassSelector.h"
#include "Player/ShooterPlayerController.h"
#include "TrainingPlayerController.generated.h"

class UTrainingUIInitComponent;

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
    void RequestTrainingClassSelection();

    void BeginRespawnFade(float FadeOutSeconds);
    void FinishRespawnFade(float FadeInSeconds);
protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnUnPossess() override;
private:
    void CombatClassSelectionFailed(const FText& Reason);
    void OpenTrainingClassSelection();
    void TrainingClassSelectionSucceeded();
    void Input_ToggleAugmentSelect();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|UI", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTrainingUIInitComponent> UIInitComponent;
};
