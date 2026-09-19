#pragma once

#include "CoreMinimal.h"
#include "Training/UI/CQBTrainingDialogWidget.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Training/TrainingTypes.h"
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
    void NotifyCQBStarted(ACQBTrainingSpawner& Spawner, int32 TotalTroopers, TSubclassOf<UTrainingSessionHUDWidget> HUDWidgetClass);
    void NotifyCQBProgress(int32 KilledTroopers, int32 TotalTroopers, bool bComplete);
    void NotifyCQBStopped(ACQBTrainingSpawner& Spawner);
    void SubmitConfirmation(int32 RequestId);
    void SubmitMode(int32 RequestId, ETrainingMode Mode, ETrainingDistancePreset Preset, float CustomMeters);
    void ReturnToPreTrainingPosition(int32 RequestId);
    void SubmitAugments(int32 RequestId, const TArray<FPrimaryAssetId>& Selection);
    void SubmitCombatClass(int32 RequestId, EDominationCombatClass RequestedClass);
    bool GetAugmentAvailabilityTags(FGameplayTagContainer& OutTags) const;
    void DismissDialog(int32 RequestId);
    void NotifyDialogRemoved(UTrainingDialogWidget& Dialog);
    void NotifySessionPhase(int32 SessionId, ETrainingFeedbackPhase Phase, double PhaseStartServerSeconds,
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
    void SavePreTrainingPosition(APawn& ParticipantPawn, APlayerController& ParticipantPlayer);
    void CloseActiveCQBDialog();
    void ClearPendingCQBRequest();
    bool ValidatePendingCQBRequest(int32 RequestId, FText& OutFailure) const;

    UFUNCTION(Client, Reliable)
    void ClientOpenDialog(ETrainingDialogKind Kind, int32 RequestId, USoundBase* InteractionSound, float SoundVolume,
        TSubclassOf<UTrainingDialogWidget> WidgetClassOverride, const FTrainingLoadoutSnapshot& LoadoutSnapshot);

    UFUNCTION(Client, Reliable)
    void ClientRequestResult(int32 RequestId, bool bSuccess, const FText& Failure);

    UFUNCTION(Client, Reliable)
    void ClientSessionPhase(int32 SessionId, ETrainingFeedbackPhase Phase, double PhaseStartServerSeconds,
        ETrainingMode Mode, float DurationSeconds);

    UFUNCTION(Client, Reliable)
    void ClientSessionHit(int32 SessionId, int32 Kills, float Damage, bool bKilled, FVector WorldPosition);

    UFUNCTION(Client, Reliable)
    void ClientTrainingDamage(float Damage, bool bKilled, FVector WorldPosition);

    UFUNCTION(Client, Reliable)
    void ClientSessionFinished(const FTrainingSessionResult& Result);

    UFUNCTION(Client, Reliable)
    void ClientPlayBotSound(int32 SessionId, USoundBase* Sound, USoundAttenuation* Attenuation,
        FVector WorldPosition, float Volume);

    UFUNCTION(Client, Reliable)
    void ClientNotify(const FText& Message);

    UFUNCTION(Client, Reliable)
    void ClientOpenCQBDialog(ECQBTrainingDialogKind Kind, int32 RequestId,
        TSubclassOf<UCQBTrainingDialogWidget> WidgetClass);

    UFUNCTION(Client, Reliable)
    void ClientCQBRequestResult(int32 RequestId, bool bSuccess, const FText& Failure);

    UFUNCTION(Client, Reliable)
    void ClientSuspendCQBDialogForClassSelection(int32 RequestId);

    UFUNCTION(Client, Reliable)
    void ClientCQBStarted(ACQBTrainingSpawner* Spawner, int32 TotalTroopers,
        TSubclassOf<UTrainingSessionHUDWidget> HUDWidgetClass);

    UFUNCTION(Client, Reliable)
    void ClientCQBProgress(int32 KilledTroopers, int32 TotalTroopers, bool bComplete);

    UFUNCTION(Client, Reliable)
    void ClientCQBStopped();

    UFUNCTION(Server, Reliable)
    void ServerEnterTrainingDestination(int32 RequestId, ETrainingDestination Destination);

    UFUNCTION(Server, Reliable)
    void ServerStartCQB(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerCancelActiveCQB(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerOpenCQBClassSelection(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerReturnFromCQBStart(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerFinishCQBToStart(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerFinishCQBToOrigin(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerCancelCQB(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerConfirm(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerStartMode(int32 RequestId, ETrainingMode Mode, ETrainingDistancePreset Preset, float CustomMeters);

    UFUNCTION(Server, Reliable)
    void ServerReturnToPreTrainingPosition(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerApplyAugments(int32 RequestId, const TArray<FPrimaryAssetId>& Selection);

    UFUNCTION(Client, Reliable)
    void ClientAugmentResult(int32 RequestId, bool bSuccess,
        const FTrainingLoadoutSnapshot& LoadoutSnapshot, const FText& Failure);

    UFUNCTION(Server, Reliable)
    void ServerChangeCombatClass(int32 RequestId, EDominationCombatClass RequestedClass);

    UFUNCTION(Client, Reliable)
    void ClientCombatClassResult(int32 RequestId, bool bSuccess,
        const FTrainingLoadoutSnapshot& LoadoutSnapshot, const FText& Failure);

    UFUNCTION(Server, Reliable)
    void ServerCancel(int32 RequestId);

    UPROPERTY(Transient)
    TObjectPtr<UTrainingPlayerPresentation> Presentation;

    TWeakObjectPtr<UTrainingResultsWidget> ResultsWidget;
    int32 LastStoppedSessionId = 0;
    UPROPERTY(Transient)
    FTrainingLoadoutSnapshot ClientLoadout;
    bool bHasClientLoadout = false;

    TWeakObjectPtr<ATrainingInteractionActor> PendingInteractable;
    TWeakObjectPtr<UTrainingDialogWidget> ActiveDialog;
    TWeakObjectPtr<ACQBTrainingSpawner> PendingCQBSpawner;
    TWeakObjectPtr<ACQBTrainingSpawner> ActiveCQBSpawner;
    TWeakObjectPtr<ACQBTrainingEndActor> PendingCQBEndActor;
    TWeakObjectPtr<UCQBTrainingDialogWidget> ActiveCQBDialog;
    TWeakObjectPtr<UTrainingSessionHUDWidget> CQBHUDWidget;
    ETrainingDialogKind PendingKind = ETrainingDialogKind::EnterArena;
    int32 PendingRequestId = 0;
    int32 RequestSequence = 0;
    int32 PendingSessionId = 0;
    int32 PendingCQBRequestId = 0;
    int32 SuspendedCQBRequestId = 0;
    TSubclassOf<UCQBTrainingDialogWidget> SuspendedCQBDialogClass;

    TWeakObjectPtr<ATrainingArena> ReturnArena;
    TWeakObjectPtr<ACQBTrainingPlayerSpawnPoint> ReturnCQBEntryPoint;
    FTransform PreTrainingTransform = FTransform::Identity;
    FRotator PreTrainingControlRotation = FRotator::ZeroRotator;
    bool bHasPreTrainingTransform = false;

    FDelegateHandle WorldTearDownHandle;
    FDelegateHandle SeamlessTravelHandle;
};
