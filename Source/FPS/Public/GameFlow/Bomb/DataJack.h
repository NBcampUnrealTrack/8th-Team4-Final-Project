#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Bomb/DataJackTypes.h"
#include "GameFramework/Actor.h"
#include "DataJack.generated.h"

class ABaseTeamPlayerState;
class ADataJackSiteVolume;
class APlayerController;
class APawn;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDataJackChangedSignature);

UCLASS(Blueprintable)
class FPS_API ADataJack : public AActor
{
	GENERATED_BODY()

public:
	ADataJack();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	EDataJackState GetDataJackState() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	ABaseTeamPlayerState* GetCarrierPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	ABaseTeamPlayerState* GetInteractingPlayerState() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	FName GetPlantedSiteId() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool HasBeenDropped() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	float GetInteractionTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	float GetInteractionProgress() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	float GetDetonationTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	float GetDetonationProgress() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	FVector GetTrackedWorldLocation() const;

	bool IsCarriedBy(const ABaseTeamPlayerState* PlayerState) const;
	bool IsInteractingPlayer(const ABaseTeamPlayerState* PlayerState) const;
	bool IsRoundActive() const;

	void ConfigureForMode(float NewPlantDuration, float NewCountdownDuration, float NewDefuseDuration, float NewDropHoldDuration, float NewValidationInterval);
	void InitializeForRound(const FTransform& SpawnTransform);
	void ActivateForRound();
	void DeactivateForRound();
	bool TryPickup(APawn* NewCarrierPawn, ABaseTeamPlayerState* NewCarrierPlayerState);
	bool BeginPlanting(APlayerController* PlayerController, ABaseTeamPlayerState* PlayerState, ADataJackSiteVolume* SiteVolume);
	bool BeginDefusing(APlayerController* PlayerController, ABaseTeamPlayerState* PlayerState);
	void CancelInteraction(APlayerController* PlayerController = nullptr);
	bool BeginDropHold(APlayerController* PlayerController, ABaseTeamPlayerState* PlayerState);
	void CancelDropHold(APlayerController* PlayerController = nullptr);
	void HandlePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState);

protected:
	UFUNCTION()
	void OnRep_DataJackState(EDataJackState PreviousState);

	UFUNCTION()
	void OnRep_DataJackData();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Game Flow|Data Jack|AV")
	void BP_OnPickedUp();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Game Flow|Data Jack|AV")
	void BP_OnDropped();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Game Flow|Data Jack|AV")
	void BP_OnPlantingStarted();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Game Flow|Data Jack|AV")
	void BP_OnPlanted();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Game Flow|Data Jack|AV")
	void BP_OnDefuseStarted();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Game Flow|Data Jack|AV")
	void BP_OnDetonated();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Game Flow|Data Jack|AV")
	void BP_OnDefused();

private:
	UFUNCTION()
	void HandlePickupAreaBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandlePickupAreaEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void SetDataJackState(EDataJackState NewState);
	void SetCarrierPlayerState(ABaseTeamPlayerState* NewCarrierPlayerState);
	void SetInteractingPlayerState(ABaseTeamPlayerState* NewInteractingPlayerState);
	void HandleStateTransition(EDataJackState PreviousState);
	void UpdateMeshVisibility();
	void UpdatePickupCollision();
	void EvaluatePickupOverlaps();
	FVector GetInteractionWorldLocation() const;
	bool CanPawnPlant(const APawn* PlayerPawn, const ADataJackSiteVolume* SiteVolume) const;
	bool CanPawnDefuse(const APawn* PlayerPawn) const;
	void ValidateActiveInteraction();
	void CompleteActiveInteraction();
	void CompleteDropHold();
	void Detonate();
	void ClearInteractionTimers();
	void ClearDropTimer();
	void ClearDetonationTimer();
	void ClearPickupBlockIfNotOverlapping();
	void NotifyInteractionStopped(APlayerController* PlayerController);
	void DropFromCarrier(bool bBlockPreviousCarrier);
	FTransform BuildDropTransform(const APawn* SourceCarrierPawn) const;
	FTransform BuildGroundedTransform(const FHitResult& SurfaceHit, const APawn* OrientationSource) const;
	float GetSynchronizedServerTime() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow|Data Jack")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow|Data Jack")
	TObjectPtr<UStaticMeshComponent> DataJackMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow|Data Jack")
	TObjectPtr<USceneComponent> InteractionPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow|Data Jack")
	TObjectPtr<USphereComponent> PickupArea;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Pickup", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float PickupRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Defuse", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float DefuseRadius = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Defuse", meta = (AllowPrivateAccess = "true"))
	bool bRequireDefuseLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Defuse", meta = (AllowPrivateAccess = "true", EditCondition = "bRequireDefuseLineOfSight", EditConditionHides))
	TEnumAsByte<ECollisionChannel> DefuseTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DropForwardDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DropObstacleClearance = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DropFloorTraceHeight = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Drop", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DropFloorTraceDepth = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Drop", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> DropPlacementTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Interaction", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InteractionMovementTolerance = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Placement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SurfaceClearance = 1.0f;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackState, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	EDataJackState DataJackState = EDataJackState::Idle;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackData, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	TObjectPtr<ABaseTeamPlayerState> CarrierPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackData, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	TObjectPtr<ABaseTeamPlayerState> InteractingPlayerState;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackData, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	FName PlantedSiteId = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackData, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	bool bHasBeenDropped = false;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackData, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	float InteractionStartServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackData, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	float InteractionEndServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackData, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	float DetonationStartServerTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_DataJackData, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	float DetonationEndServerTime = 0.0f;

	float PlantDuration = 4.0f;
	float CountdownDuration = 45.0f;
	float DefuseDuration = 7.0f;
	float DropHoldDuration = 1.0f;
	float InteractionValidationInterval = 0.1f;
	bool bRoundActive = false;
	FVector InteractionStartPawnLocation = FVector::ZeroVector;
	TWeakObjectPtr<APlayerController> InteractionController;
	TWeakObjectPtr<APawn> CarrierPawn;
	TWeakObjectPtr<APlayerController> DropController;
	TWeakObjectPtr<ABaseTeamPlayerState> DropPlayerState;
	TWeakObjectPtr<APawn> TemporarilyBlockedPickupPawn;
	TWeakObjectPtr<ADataJackSiteVolume> ActivePlantSite;
	FTimerHandle InteractionCompletionTimerHandle;
	FTimerHandle InteractionValidationTimerHandle;
	FTimerHandle DropHoldTimerHandle;
	FTimerHandle DetonationTimerHandle;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Data Jack")
	FOnDataJackChangedSignature OnDataJackChanged;
};
