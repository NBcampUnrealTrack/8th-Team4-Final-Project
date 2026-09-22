#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "UI/Frontend/CombatUIInitComponent.h"
#include "DominationUIInitComponent.generated.h"

class ADominationGameState;

UCLASS(ClassGroup = (UI))
class FPS_API UDominationUIInitComponent : public UCombatUIInitComponent
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
    void BindToGameState(ADominationGameState* DominationGameState);
    void UnbindFromGameState();
    TWeakObjectPtr<ADominationGameState> BoundGameState;
    int32 LastObservedRewardRound = 0;
    EDominationMatchPhase LastObservedMatchPhase = EDominationMatchPhase::WaitingForPlayers;
};
