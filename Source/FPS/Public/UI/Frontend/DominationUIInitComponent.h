#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "UI/Frontend/CombatUIInitComponent.h"
#include "DominationUIInitComponent.generated.h"

class ADominationGameState;
class UShooterMatchOutcomeScreen;

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
    // 매치 종료 처리. 
    void TryPresentMatchEnd();
    void TryPresentMatchOutcome();
    void BindToGameState(ADominationGameState* DominationGameState);
    void UnbindFromGameState();

    UPROPERTY(EditDefaultsOnly, Category = "UI|Match End")
    TSubclassOf<UShooterMatchOutcomeScreen> MatchOutcomeScreenClass;

    UPROPERTY(Transient)
    TObjectPtr<UShooterMatchOutcomeScreen> MatchOutcomeScreen;

    TWeakObjectPtr<ADominationGameState> BoundGameState;
    int32 LastObservedRewardRound = 0;
    EDominationMatchPhase LastObservedMatchPhase = EDominationMatchPhase::WaitingForPlayers;
};
