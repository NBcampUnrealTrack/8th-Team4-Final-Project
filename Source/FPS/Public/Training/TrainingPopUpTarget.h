#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Interfaces/RogueCombatFeedbackPolicy.h"
#include "Interfaces/RogueHitscanReactiveTarget.h"
#include "Training/TrainingTypes.h"
#include "TrainingPopUpTarget.generated.h"

class UAbilitySystemComponent;
class UBoxComponent;
class URogueAugmentAttributeSet;
class URogueVitalAttributeSet;
class USceneComponent;
class UStaticMeshComponent;
class USoundBase;
struct FGameplayEventData;
struct FGameplayEffectContextHandle;

UENUM(BlueprintType)
enum class ETrainingPopUpTargetState : uint8
{
    Up,
    Falling,
    Down,
    Rising
};

UCLASS(Blueprintable)
class FPS_API ATrainingPopUpTarget : public AActor, public IAbilitySystemInterface, public IRogueHitscanReactiveTarget, public IRogueCombatFeedbackPolicy
{
    GENERATED_BODY()

public:
    ATrainingPopUpTarget();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void HandleHitscanImpact(AActor* InstigatorActor, const FVector& ImpactPoint,
        const FVector& ShotDirection) override;
    virtual bool CountsAsCombatKill() const override { return false; }
    virtual bool PlaysCombatFeedbackAudio() const override { return false; }

    UFUNCTION(BlueprintCallable, Category = "Training|PopUpTarget")
    void KnockDown();

    UFUNCTION(BlueprintCallable, Category = "Training|PopUpTarget")
    void ResetTarget();

    UFUNCTION(BlueprintPure, Category = "Training|PopUpTarget")
    bool IsTargetUp() const { return TargetState == ETrainingPopUpTargetState::Up; }

    UFUNCTION(BlueprintPure, Category = "Training|PopUpTarget|Health")
    float GetCurrentHealth() const;

    UFUNCTION(BlueprintPure, Category = "Training|PopUpTarget|Health")
    float GetMaxHealth() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<UStaticMeshComponent> BaseMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<USceneComponent> TargetPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<UStaticMeshComponent> TargetMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<UBoxComponent> HitBox;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Health",
        meta = (ClampMin = "1.0"))
    float TargetMaxHealth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion")
    FRotator DownRotation = FRotator(-90.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FallDirectionDeadZone = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion",
        meta = (ClampMin = "0.01", Units = "s"))
    float FallDurationSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion",
        meta = (ClampMin = "0.0", Units = "s"))
    float DownHoldSeconds = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion",
        meta = (ClampMin = "0.01", Units = "s"))
    float RiseDurationSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Audio")
    TObjectPtr<USoundBase> HitSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Audio",
        meta = (ClampMin = "0.0"))
    float HitSoundVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Audio",
        meta = (ClampMin = "0.01"))
    float HitSoundPitch = 1.0f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    UFUNCTION()
    void OnRep_TargetState();

    UFUNCTION()
    void OnRep_FallDirectionSign();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayHitSound(FVector_NetQuantize ImpactPoint);

    void HandleDamageTaken(FGameplayTag EventTag, const FGameplayEventData* Payload);
    void HandleOutOfHealth(UAbilitySystemComponent* VictimASC, const FGameplayEffectContextHandle& ContextHandle);
    void InitializeHealth();
    void ResetHealthForNextCycle();
    void SetTargetState(ETrainingPopUpTargetState NewState);
    void UpdateTargetRotation();
    void ConfigureTargetComponents();
    void ConfigureHitBoxCollision();
    void SetHitBoxActive(bool bActive);
    void BeginRise();
    void CacheFallDirectionFromShot(const FVector& ShotDirection);
    void CommitPendingFallDirection();
    int8 ResolveFallDirectionSign(const FVector& ShotDirection) const;
    FRotator GetDirectionalDownRotation() const;

    UPROPERTY(VisibleAnywhere, Category = "Training|PopUpTarget|GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    UPROPERTY()
    TObjectPtr<URogueVitalAttributeSet> VitalSet;

    UPROPERTY()
    TObjectPtr<URogueAugmentAttributeSet> AugmentSet;

    UPROPERTY(ReplicatedUsing = OnRep_TargetState, VisibleInstanceOnly,
        Category = "Training|PopUpTarget|State")
    ETrainingPopUpTargetState TargetState = ETrainingPopUpTargetState::Up;

    UPROPERTY(ReplicatedUsing = OnRep_FallDirectionSign)
    int8 FallDirectionSign = 1;

    int8 PendingFallDirectionSign = 1;
    FDelegateHandle DamageTakenHandle;
    FDelegateHandle OutOfHealthHandle;
    FRotator RaisedRotation = FRotator::ZeroRotator;
    float MotionAlpha = 0.0f;
    FTimerHandle ResetTimer;
};
