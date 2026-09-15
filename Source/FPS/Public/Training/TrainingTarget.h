#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "TrainingTarget.generated.h"

class UAbilitySystemComponent;
class UBoxComponent;
class UCombatHitZoneDefinition;
class URogueAugmentAttributeSet;
class URogueHealthComponent;
class URogueVitalAttributeSet;
class UStaticMeshComponent;
struct FGameplayEventData;

UCLASS()
class FPS_API ATrainingTarget : public AActor, public IAbilitySystemInterface, public IRogueDamageableTarget
{
    GENERATED_BODY()

public:
    ATrainingTarget();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void HandleDeath_Implementation(const FRogueDeathContext& Context) override;
    virtual void PlayDeathReaction_Implementation(const FRogueDeathReaction& Reaction) override;
    virtual UCombatHitZoneDefinition* GetCombatHitZoneDefinition_Implementation() const override;
    float GetGroundOffsetCentimeters() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training")
    TObjectPtr<UBoxComponent> HitCollision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training")
    TObjectPtr<UStaticMeshComponent> DisplayMesh;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void HandleDamageTaken(FGameplayTag EventTag, const FGameplayEventData* Payload);
    void FinishHit();

    UPROPERTY(VisibleAnywhere, Category = "Training|GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    UPROPERTY()
    TObjectPtr<URogueVitalAttributeSet> VitalSet;

    UPROPERTY()
    TObjectPtr<URogueAugmentAttributeSet> AugmentSet;

    UPROPERTY(VisibleAnywhere, Category = "Training|GAS")
    TObjectPtr<URogueHealthComponent> HealthComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Training")
    TObjectPtr<UCombatHitZoneDefinition> HitZoneDefinition;

    FDelegateHandle DamageTakenHandle;
    bool bHitProcessed = false;
};
