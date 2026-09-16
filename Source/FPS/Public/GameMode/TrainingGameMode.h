#pragma once

#include "CoreMinimal.h"
#include "GameMode/ShooterGameMode.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "TrainingGameMode.generated.h"

class ATrainingPlayerController;
class ADominationPlayerState;
class UAugmentDraftManager;
class UAugmentDraftProfile;
class UDominationLoadoutManager;
class UPlayerRespawnProtection;

UCLASS()
class FPS_API ATrainingGameMode : public AShooterGameMode
{
    GENERATED_BODY()
public:
    ATrainingGameMode();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Logout(AController* Exiting) override;
    virtual void NotifyPlayerReadyForGameFlow(APlayerController* ReadyPlayer) override;
    bool RequestCombatClass(ATrainingPlayerController* Requester,
        EDominationCombatClass RequestedClass, FText& OutFailure);
    bool RequestTrainingCombatClass(ATrainingPlayerController* Requester,
        EDominationCombatClass RequestedClass, FText& OutFailure);
    bool UsesManualAugmentSelection() const { return bUseManualAugmentSelection; }
    void RequestPreviewCombatClass(ATrainingPlayerController* Requester,
        EDominationCombatClass RequestedClass);
protected:
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
    virtual bool PlayerCanRestart_Implementation(APlayerController* Requester) override;
    virtual void NotifyPlayerEliminatedBy(AController* EliminatedController,
        AShooterPlayerState* KillerPlayerState) override;
private:
    void TryOpenClassSelection(ATrainingPlayerController* ReadyPlayer);
    bool RestartTrainingPlayer(ATrainingPlayerController* WaitingPlayer, bool bInitialSpawn);
    void HandleRespawnTimer(TWeakObjectPtr<ATrainingPlayerController> WaitingPlayer);
    void HandleAugmentDependenciesReady();
    void GrantInitialAugments(ATrainingPlayerController* ReadyPlayer);

    UPROPERTY(EditDefaultsOnly, Category = "Training|Respawn", meta = (ClampMin = "0.1", Units = "s"))
    float RespawnDelay = 5.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Respawn", meta = (ClampMin = "0.0", Units = "s"))
    float RespawnProtectionDuration = 2.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Augment")
    bool bUseManualAugmentSelection = true;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Augment", meta = (EditCondition = "!bUseManualAugmentSelection"))
    bool bEnableAugmentDraft = true;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Augment", meta = (EditCondition = "bEnableAugmentDraft"))
    TObjectPtr<UAugmentDraftProfile> AugmentDraftProfile;

    UPROPERTY(Transient)
    TObjectPtr<UDominationLoadoutManager> LoadoutManager;
    UPROPERTY(Transient)
    TObjectPtr<UAugmentDraftManager> AugmentDraftManager;
    UPROPERTY(Transient)
    TObjectPtr<UPlayerRespawnProtection> RespawnProtection;
    TSet<TWeakObjectPtr<ATrainingPlayerController>> InitializedPlayers;
    TSet<TWeakObjectPtr<ATrainingPlayerController>> InitialAugmentsGranted;
    TMap<TWeakObjectPtr<ATrainingPlayerController>, FTimerHandle> PendingRespawns;
    bool bConfigurationReady = false;
};
