#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameMode/Types/DominationSandstormTypes.h"
#include "UI/Frontend/CombatUIInitComponent.h"
#include "DominationUIInitComponent.generated.h"

class ADominationGameState;
class UShooterMatchOutcomeScreen;
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
    // 매치 종료 처리. 
    void TryPresentMatchEnd();
    void TryPresentMatchOutcome();
    void TryScheduleMatchExitTransition();
    void BeginMatchExitTransition(bool bInstant);
    // 예약 타이머가 느린 프레임으로 travel 보다 늦었을 때의 안전망
    void HandleSeamlessTravelStart(UWorld* TravelingWorld, const FString& MapName);
    UFUNCTION()
    void HandleSandstormStateChanged(const FDominationSandstormState& SandstormState);
    void UpdateSandstormWarning();
    void ClearSandstormWarning();
    UPROPERTY(Transient)
    TObjectPtr<UDominationSandstormWarningWidget> SandstormWarningWidget;
    FTimerHandle SandstormWarningTimer;
    void BindToGameState(ADominationGameState* DominationGameState);
    void UnbindFromGameState();

    UPROPERTY(EditDefaultsOnly, Category = "UI|Match End")
    TSubclassOf<UShooterMatchOutcomeScreen> MatchOutcomeScreenClass;

    UPROPERTY(Transient)
    TObjectPtr<UShooterMatchOutcomeScreen> MatchOutcomeScreen;

    // Anim_Cover 길이 이상이어야 함
    UPROPERTY(EditDefaultsOnly, Category = "UI|Match End", meta = (ClampMin = "0.1", Units = "s"))
    float MatchExitCoverLeadSeconds = 0.7f;

    FTimerHandle MatchExitTransitionTimer;
    bool bMatchExitTransitionAttempted = false;
    FDelegateHandle SeamlessTravelStartHandle;

    TWeakObjectPtr<ADominationGameState> BoundGameState;
    int32 LastObservedRewardRound = 0;
    EDominationMatchPhase LastObservedMatchPhase = EDominationMatchPhase::WaitingForPlayers;
};
