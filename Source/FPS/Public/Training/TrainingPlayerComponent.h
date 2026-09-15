#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Training/TrainingTypes.h"
#include "TrainingPlayerComponent.generated.h"

class APlayerController;
class ATrainingInteractionActor;
class UTrainingDialogWidget;
class UShooterUIManagerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTrainingMessage, const FText&, Message);

// Owner-only UI/RPC bridge. Does not spawn targets, run timers, or own game-mode rules.
UCLASS(ClassGroup = (Training), meta = (BlueprintSpawnableComponent))
class FPS_API UTrainingPlayerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTrainingPlayerComponent();
    void OpenDialogFor(ATrainingInteractionActor& Interactable);
    void SubmitConfirmation(int32 RequestId);
    void SubmitMode(int32 RequestId, ETrainingMode Mode, ETrainingDistancePreset Preset, float CustomMeters);
    void DismissDialog(int32 RequestId);
    void NotifyDialogRemoved(UTrainingDialogWidget& Dialog);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingDialogWidget> ConfirmationWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingDialogWidget> ModeSelectionWidgetClass;

    // Optional separate end-session design. Existing projects fall back to ConfirmationWidgetClass.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|UI")
    TSubclassOf<UTrainingDialogWidget> EndSessionWidgetClass;

    // Optional existing HUD notification/toast integration; no hard-coded widget construction.
    UPROPERTY(BlueprintAssignable, Category = "Training|UI")
    FTrainingMessage OnTrainingMessage;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    APlayerController* GetParticipantController() const;
    UShooterUIManagerSubsystem* GetUIManager() const;
    bool ValidatePendingRequest(int32 RequestId, FText& OutFailure) const;
    void ClearPendingRequest();
    void CloseActiveDialog();
    void HandleWorldLeaving(UWorld* DepartingWorld);
    void HandleSeamlessTravel(UWorld* DepartingWorld, const FString& Destination);

    UFUNCTION(Client, Reliable)
    void ClientOpenDialog(ETrainingDialogKind Kind, int32 RequestId);

    UFUNCTION(Client, Reliable)
    void ClientRequestResult(int32 RequestId, bool bSuccess, const FText& Failure);

    UFUNCTION(Client, Reliable)
    void ClientNotify(const FText& Message);

    UFUNCTION(Server, Reliable)
    void ServerConfirm(int32 RequestId);

    UFUNCTION(Server, Reliable)
    void ServerStartMode(int32 RequestId, ETrainingMode Mode, ETrainingDistancePreset Preset, float CustomMeters);

    UFUNCTION(Server, Reliable)
    void ServerCancel(int32 RequestId);

    TWeakObjectPtr<ATrainingInteractionActor> PendingInteractable;
    TWeakObjectPtr<UTrainingDialogWidget> ActiveDialog;
    ETrainingDialogKind PendingKind = ETrainingDialogKind::EnterArena;
    int32 PendingRequestId = 0;
    int32 RequestSequence = 0;
    int32 PendingSessionId = 0;
    FDelegateHandle WorldTearDownHandle;
    FDelegateHandle SeamlessTravelHandle;
};
