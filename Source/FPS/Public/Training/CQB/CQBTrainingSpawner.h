#pragma once

#include "CoreMinimal.h"
#include "Training/UI/CQBTrainingDialogWidget.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RogueInteractable.h"
#include "CQBTrainingSpawner.generated.h"

class ACQBTrainingSpawnPoint;
class ACQBTrainingEndActor;
class ACQBTrainingPlayerSpawnPoint;
class APawn;
class APlayerController;
class USoundBase;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UTrainingSessionHUDWidget;

UENUM(BlueprintType)
enum class ECQBTrainingSessionState : uint8
{
    Idle,
    Loading,
    Running,
    Completed,
};

UCLASS()
class FPS_API ACQBTrainingSpawner : public AActor, public IRogueInteractable
{
    GENERATED_BODY()

public:
    ACQBTrainingSpawner();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool CanInteract(const APlayerController& RequestingPlayer) const override;
    virtual void Interact(APlayerController& RequestingPlayer) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|Spawn")
    TSoftClassPtr<APawn> TrooperClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|Spawn")
    FName SpawnGroup = TEXT("Default");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|Spawn", meta = (ClampMin = "1", ClampMax = "32"))
    int32 MaxConcurrentTroopers = 5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|Rules")
    bool bSuppressStandardEliminationFeedback = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|Optimization", meta = (ClampMin = "0.05", ClampMax = "10.0", Units = "s"))
    float CorpseLifetimeSeconds = 1.25f;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CQB|Flow")
    TObjectPtr<ACQBTrainingPlayerSpawnPoint> PlayerStartPoint;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CQB|Flow")
    TObjectPtr<ACQBTrainingEndActor> EndActor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|UI")
    TSubclassOf<UCQBTrainingDialogWidget> StartDialogWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|UI")
    TSubclassOf<UTrainingSessionHUDWidget> HUDWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|Audio")
    TObjectPtr<USoundBase> InteractionSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CQB|Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
    float InteractionSoundVolume = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|Interaction")
    TObjectPtr<UBoxComponent> InteractionCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|Interaction")
    TObjectPtr<UStaticMeshComponent> DisplayMesh;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "CQB|State")
    ECQBTrainingSessionState SessionState = ECQBTrainingSessionState::Idle;

    UFUNCTION(BlueprintCallable, Category = "CQB|Spawn")
    void DespawnSpawnedTroopers();

    bool StartTrainingFor(APlayerController& Participant, int32 RequestId);
    void CancelPendingStart(APlayerController& Participant, int32 RequestId);
    void NotifyTrooperDeath(APawn& Trooper);
    bool IsTrainingComplete() const { return SessionState == ECQBTrainingSessionState::Completed; }
    bool IsTrainingRunning() const { return SessionState == ECQBTrainingSessionState::Running; }
    bool UsesStandardEliminationFeedback() const { return !bSuppressStandardEliminationFeedback; }
    ACQBTrainingPlayerSpawnPoint* GetPlayerStartPoint() const { return PlayerStartPoint; }
    void FinishTrainingAndReset();

protected:
    UFUNCTION()
    void HandleInteractionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void HandleInteractionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    void BeginTraining();
    void HandleTrooperClassLoaded();
    void StartLoadedTraining(UClass& LoadedTrooperClass);
    bool CollectSpawnPoints();
    bool SpawnUntilFull(UClass& LoadedTrooperClass);
    APawn* SpawnFromPoint(ACQBTrainingSpawnPoint& SpawnPoint, UClass& LoadedTrooperClass);
    int32 CountAliveTroopers() const;
    void RegisterTrooperDeath(int32 TrooperIndex);
    bool RefillAfterDeath();
    void CompleteTraining();

    void ResolvePendingStart(bool bSuccess, const FText& Failure);
    void AbortPendingStart(const FText& Failure);
    void DestroySpawnedTroopersOnly();
    void ResetRuntimeData();
    void ResetSessionState(bool bNotifyParticipant);

    void NotifyParticipantStarted();
    void NotifyParticipantProgress();
    void NotifyParticipantStopped();

    int32 PendingStartRequestId = 0;
    int32 NextSpawnIndex = 0;
    int32 KilledTroopers = 0;
    TSet<int32> CountedDeathIndices;

    TWeakObjectPtr<APlayerController> LocalOverlappingController;
    TWeakObjectPtr<APlayerController> ActiveParticipant;
    TSharedPtr<FStreamableHandle> PendingLoadHandle;
    TArray<TWeakObjectPtr<ACQBTrainingSpawnPoint>> OrderedSpawnPoints;
    TArray<TWeakObjectPtr<APawn>> SpawnedTroopers;
};
