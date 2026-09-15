#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "Training/TrainingTypes.h"
#include "TrainingCombatAI.generated.h"

class ATrainingArena;
class UAbilitySystemComponent;
class UCombatHitZoneDefinition;
class UMaterialInterface;
class URogueAugmentAttributeSet;
class URogueHealthComponent;
class URogueVitalAttributeSet;

UCLASS()
class FPS_API ATrainingCombatAI : public ACharacter, public IAbilitySystemInterface, public IRogueDamageableTarget
{
    GENERATED_BODY()

public:
    ATrainingCombatAI();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void HandleDeath_Implementation(const FRogueDeathContext& Context) override;
    virtual void PlayDeathReaction_Implementation(const FRogueDeathReaction& Reaction) override;
    virtual UCombatHitZoneDefinition* GetCombatHitZoneDefinition_Implementation() const override;

    bool InitializeForTraining(ATrainingArena& InArena, APlayerController& InParticipant,
        const FTrainingDistanceBand& InBand);
    bool HasConfiguredBehavior() const;
    ATrainingArena* GetTrainingArena() const;
    APlayerController* GetParticipant() const;
    const FTrainingDistanceBand& GetDistanceBand() const { return DistanceBand; }

    UFUNCTION(BlueprintPure, Category = "Training")
    bool IsAlive() const;

    // Assigned on the AI Blueprint. Its material uses Time, DissolveStartTime and DissolveDuration.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Death")
    TObjectPtr<UMaterialInterface> DissolveMaterial;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StartDissolve();

    UPROPERTY(VisibleAnywhere, Category = "Training|GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    UPROPERTY()
    TObjectPtr<URogueVitalAttributeSet> VitalSet;

    UPROPERTY()
    TObjectPtr<URogueAugmentAttributeSet> AugmentSet;

    UPROPERTY(VisibleAnywhere, Category = "Training|GAS")
    TObjectPtr<URogueHealthComponent> HealthComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Damage")
    TObjectPtr<UCombatHitZoneDefinition> HitZoneDefinition;

    TWeakObjectPtr<ATrainingArena> TrainingArena;
    TWeakObjectPtr<APlayerController> Participant;
    FTrainingDistanceBand DistanceBand;
    bool bDeathPresentationStarted = false;
};
