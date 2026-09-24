#pragma once

#include "CoreMinimal.h"
#include "Training/UI/CQBTrainingDialogWidget.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingModeDefinition.h"
#include "Training/TrainingLoadoutSnapshot.h"
#include "Training/UI/TrainingPlayerPresentation.h"
#include "Interfaces/RogueCombatFeedbackPolicy.h"
#include "TrainingPlayerComponent.generated.h"

class APlayerController;
class UAugmentInventoryComponent;
class ATrainingInteractionActor;
class ATrainingArena;
class ACQBTrainingSpawner;
class ACQBTrainingEndActor;
class ACQBTrainingPlayerSpawnPoint;
class UTrainingDamageFeedbackWidget;
class UTrainingDialogWidget;
class UTrainingStartFeedbackWidget;
class UTrainingSessionHUDWidget;
class UTrainingResultsWidget;
class UUserWidget;
class USoundBase;
class USoundAttenuation;
class UShooterUIManagerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTrainingMessage, const FText&, Message);

UCLASS(ClassGroup = (Training), meta = (BlueprintSpawnableComponent))
class FPS_API UTrainingPlayerComponent : public UActorComponent, public IRogueCombatFeedbackPolicy
{
    GENERATED_BODY()

public:
    UTrainingPlayerComponent();
    void ResetTrainingState();
    void HandleParticipantDeath();
    void NotifyMessage(const FText& Message);
    void OpenDialogFor(ATrainingInteractionActor& Interactable);
    void OpenCQBStartDialog(ACQBTrainingSpawner& Spawner);
    void OpenCQBCancelDialog(ACQBTrainingSpawner& Spawner);
    void OpenCQBEndDialog(ACQBTrainingEndActor& EndActor);
    void SubmitTrainingDestination(int32 RequestId, ETrainingDestination Destination);
    void SubmitCQBStart(int32 RequestId);
    void SubmitCQBCancel(int32 RequestId);
    void SubmitCQBClassSelection(int32 RequestId);
    void ResumeCQBStartDialogAfterClassSelection();
    void SubmitCQBReturnToOrigin(int32 RequestId);
    void SubmitCQBEndToStart(int32 RequestId);
    void SubmitCQBEndToOrigin(int32 RequestId);
    void DismissCQBDialog(int32 RequestId);
    void NotifyCQBDialogRemoved(UCQBTrainingDialogWidget& Dialog);
    void NotifyCQBStartRequestResolved(ACQBTrainingSpawner& Spawner, int32 RequestId, bool bSuccess, const FText& Failure);
    void NotifyCQBLoading(ACQBTrainingSpawner& Spawner);
    void NotifyCQBStarted(ACQBTrainingSpawner& Spawner, int32 TotalTroopers, TSubclassOf<UTrainingSessionHUDWidget> HUDWidgetClass);
    void NotifyCQBProgress(int32 KilledTroopers, int32 TotalTroopers, bool bComplete);
    void NotifyCQBStopped(ACQBTrainingSpawner& Spawner);
    void SubmitConfirmation(int32 RequestId);
    void SubmitMode(int32 RequestId, ETrainingMode Mode, ETrainingDistancePreset Preset, float CustomMeters);
    UFUNCTION(BlueprintPure, Category = "Training")
    bool ResolveModePreview(ETrainingMode Mode, FTrainingModeConfig& OutConfig) const;
    void ReturnToPreTrainingPosition(int32 RequestId);
    void SubmitAugments(int32 RequestId, const TArray<FPrimaryAssetId>& Selection);
    void SubmitCombatClass(int32 RequestId, EDominationCombatClass RequestedClass);
    bool GetAugmentAvailabilityTags(FGameplayTagContainer& OutTags) const;
    void DismissDialog(int32 RequestId);
    void NotifyDialogRemoved(UTrainingDialogWidget& Dialog);
    void NotifySessionPhase(int32 SessionId, ETrainingFeedbackPhase Phase, double PhaseStartSeconds,
        ETrainingMode Mode, float DurationSeconds);
    void NotifySessionHit(int32 SessionId, int32 Kills, float Damage, bool bKilled, const FVector& WorldPosition);
    void NotifyTrainingDamage(float Damage, bool bKilled, const FVector& WorldPosition);
    void NotifySessionFinished(const FTrainingSessionResult& Result);
    void NotifyBotSound(int32 SessionId, USoundBase* Sound, USoundAttenuation* Attenuation,
        const FVector& WorldPosition, float Volume);
    void DismissResults(UTrainingResultsWidget& Widget);
    void NotifyResultsRemoved(UTrainingResultsWidget& Widget);
    void ApplyButtonSounds(UUserWidget& Widget) const;

    virtual bool UsesStandardEliminationFeedback() const override;
    bool IsCQBTrainingActive() const;
    bool IsCQBTrainingActiveWith(const ACQBTrainingSpawner& Spawner) const;
    bool TryGetCQBRespawnTransform(FTransform& OutTransform) const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingDialogWidget> ConfirmationWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingDialogWidget> ModeSelectionWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingDialogWidget> EndSessionWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingStartFeedbackWidget> StartFeedbackWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingSessionHUDWidget> SessionHUDWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingDamageFeedbackWidget> DamageFeedbackWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingResultsWidget> ResultsWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundBase> ButtonHoverSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundBase> ButtonClickSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundBase> WidgetOpenSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundBase> WidgetCloseSound;

    UPROPERTY(BlueprintAssignable, Category = "Training|UI")
    FTrainingMessage OnTrainingMessage;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    friend class UTrainingPlayerPresentation;
    APlayerController* GetParticipantController() const;
    UAugmentInventoryComponent* GetAugmentInventory() const;
    FTrainingLoadoutSnapshot BuildLoadoutSnapshot() const;
    bool ValidateLoadoutRequest(int32 RequestId, FText& OutFailure) const;
    UShooterUIManagerSubsystem* GetUIManager() const;
    bool ValidatePendingRequest(int32 RequestId, FText& OutFailure) const;
    void ClearPendingRequest();
    void CloseActiveDialog();
    void CloseResults(bool bPlayCloseSound = false);
    void HandleWorldLeaving(UWorld* DepartingWorld);
    void HandleSeamlessTravel(UWorld* DepartingWorld, const FString& Destination);
    bool TeleportParticipant(const FTransform& Destination, const FRotator& TargetControlRotation);
    bool TeleportToSavedPreTrainingPosition(FText& OutFailure, bool bClearSavedPosition);
    void CloseActiveCQBDialog();
    void ClearPendingCQBRequest();
    bool ValidatePendingCQBRequest(int32 RequestId, FText& OutFailure) const;

    void ShowDialog(ETrainingDialogKind Kind, int32 RequestId, USoundBase* InteractionSound, float SoundVolume,
        TSubclassOf<UTrainingDialogWidget> WidgetClassOverride, const FTrainingLoadoutSnapshot& LoadoutSnapshot);

    void ResolveDialogRequest(int32 RequestId, bool bSuccess, const FText& Failure);

    void ShowCQBDialog(ECQBTrainingDialogKind Kind, int32 RequestId,
        TSubclassOf<UCQBTrainingDialogWidget> WidgetClass);

    void ResolveCQBRequest(int32 RequestId, bool bSuccess, const FText& Failure);

    void SuspendCQBDialogForClassSelection(int32 RequestId);

    void CancelCQBDialogRequest(int32 RequestId);

    void ResolveAugmentRequest(int32 RequestId, bool bSuccess,
        const FTrainingLoadoutSnapshot& LoadoutSnapshot, const FText& Failure);

    void ResolveCombatClassRequest(int32 RequestId, bool bSuccess,
        const FTrainingLoadoutSnapshot& LoadoutSnapshot, const FText& Failure);

    void CancelDialogRequest(int32 RequestId);

    UPROPERTY(Transient)
    TObjectPtr<UTrainingPlayerPresentation> Presentation;

    TWeakObjectPtr<UTrainingResultsWidget> ResultsWidget;
    int32 LastStoppedSessionId = 0;

    TWeakObjectPtr<ATrainingInteractionActor> PendingInteractable;
    TWeakObjectPtr<UTrainingDialogWidget> ActiveDialog;
    TWeakObjectPtr<ACQBTrainingSpawner> PendingCQBSpawner;
    TWeakObjectPtr<ACQBTrainingSpawner> ActiveCQBSpawner;
    TWeakObjectPtr<ACQBTrainingEndActor> PendingCQBEndActor;
    TWeakObjectPtr<UCQBTrainingDialogWidget> ActiveCQBDialog;
    ETrainingDialogKind PendingKind = ETrainingDialogKind::EnterArena;
    int32 PendingRequestId = 0;
    int32 RequestSequence = 0;
    int32 PendingSessionId = 0;
    int32 PendingCQBRequestId = 0;
    int32 SuspendedCQBRequestId = 0;
    UPROPERTY(Transient)
    TSubclassOf<UCQBTrainingDialogWidget> SuspendedCQBDialogClass;

    TWeakObjectPtr<ATrainingArena> ReturnArena;
    TWeakObjectPtr<ACQBTrainingPlayerSpawnPoint> ReturnCQBEntryPoint;
    FTransform PreTrainingTransform = FTransform::Identity;
    FRotator PreTrainingControlRotation = FRotator::ZeroRotator;
    bool bHasPreTrainingTransform = false;

    FDelegateHandle WorldTearDownHandle;
    FDelegateHandle SeamlessTravelHandle;
};
