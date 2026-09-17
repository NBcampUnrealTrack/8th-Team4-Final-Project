#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingLoadoutSnapshot.h"
#include "TrainingPlayerComponent.generated.h"

class APlayerController;
class UAugmentInventoryComponent;
class ATrainingInteractionActor;
class ATrainingArena;
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
class FPS_API UTrainingPlayerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTrainingPlayerComponent();
    void OpenDialogFor(ATrainingInteractionActor& Interactable);
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
    APlayerController* GetParticipantController() const;
    UAugmentInventoryComponent* GetAugmentInventory() const;
    FTrainingLoadoutSnapshot BuildLoadoutSnapshot() const;
    bool ValidateLoadoutRequest(int32 RequestId, FText& OutFailure) const;
    UShooterUIManagerSubsystem* GetUIManager() const;
    bool ValidatePendingRequest(int32 RequestId, FText& OutFailure) const;
    void ClearPendingRequest();
    void CloseActiveDialog();
    void ClearSessionFeedback();
    void ClearTrainingDamageFeedback();
    UTrainingDamageFeedbackWidget* EnsureDamageFeedbackWidget();
    void PresentDamageFeedback(float Damage, bool bKilled, const FVector& WorldPosition);
    void CloseResults(bool bPlayCloseSound = false);
    void HandleWorldLeaving(UWorld* DepartingWorld);
    void HandleSeamlessTravel(UWorld* DepartingWorld, const FString& Destination);

    UFUNCTION(Client, Reliable)
    void ClientOpenDialog(ETrainingDialogKind Kind, int32 RequestId, USoundBase* InteractionSound, float SoundVolume,
        const FTrainingLoadoutSnapshot& LoadoutSnapshot);

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
    TObjectPtr<UTrainingStartFeedbackWidget> StartFeedbackWidget;

    UPROPERTY(Transient)
    TObjectPtr<UTrainingSessionHUDWidget> SessionHUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UTrainingDamageFeedbackWidget> DamageFeedbackWidget;

    UPROPERTY(Transient)
    TObjectPtr<USoundAttenuation> DefaultBotAttenuation;

    TWeakObjectPtr<UTrainingResultsWidget> ResultsWidget;
    int32 LastStoppedSessionId = 0;
    int32 FeedbackSessionId = 0;
    ETrainingMode FeedbackMode = ETrainingMode::TargetAcquisition;
    UPROPERTY(Transient)
    FTrainingLoadoutSnapshot ClientLoadout;
    bool bHasClientLoadout = false;

    TWeakObjectPtr<ATrainingInteractionActor> PendingInteractable;
    TWeakObjectPtr<UTrainingDialogWidget> ActiveDialog;
    ETrainingDialogKind PendingKind = ETrainingDialogKind::EnterArena;
    int32 PendingRequestId = 0;
    int32 RequestSequence = 0;
    int32 PendingSessionId = 0;

    TWeakObjectPtr<ATrainingArena> ReturnArena;
    FTransform PreTrainingTransform = FTransform::Identity;
    FRotator PreTrainingControlRotation = FRotator::ZeroRotator;
    bool bHasPreTrainingTransform = false;

    FDelegateHandle WorldTearDownHandle;
    FDelegateHandle SeamlessTravelHandle;
};
