#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameMode/Types/DominationSandstormTypes.h"
#include "UI/Frontend/CombatUIInitComponent.h"
#include "DominationUIInitComponent.generated.h"

class ADominationGameState;
class UDominationSandstormWarningWidget;

UCLASS(ClassGroup = (UI))
class FPS_API UDominationUIInitComponent : public UCombatUIInitComponent
{
    GENERATED_BODY()
public:
    virtual void ResetWorldBinding() override;
protected:
    virtual void BeginPlay() override;
    virtual bool PrepareModeBindings() override;
    virtual bool CanInitializeCombatHUD(const ADominationPlayerState& ClassState) const override;
    virtual bool ShouldPresentClassSelection(const ADominationPlayerState& ClassState) const override;
    virtual bool IsAugmentModeActive() const override;
    virtual bool IsSurrenderRequestAllowed() const override;
private:
    UFUNCTION()
    void HandleDominationFlowStateChanged();
    UFUNCTION()
    void HandleMatchStateChanged();
    UFUNCTION()
    void HandleRewardRoundChanged(EGameTeam Team, int32 NewScore);
    void TryCaptureMatchResult();
    UFUNCTION()
    void HandleSandstormStateChanged(const FDominationSandstormState& SandstormState);
    void UpdateSandstormWarning();
    void ClearSandstormWarning();
    UPROPERTY(Transient)
    TObjectPtr<UDominationSandstormWarningWidget> SandstormWarningWidget;
    FTimerHandle SandstormWarningTimer;
    void BindToGameState(ADominationGameState* DominationGameState);
    void UnbindFromGameState();
    TWeakObjectPtr<ADominationGameState> BoundGameState;
    int32 LastObservedRewardRound = 0;
    EDominationMatchPhase LastObservedMatchPhase = EDominationMatchPhase::WaitingForPlayers;
};
