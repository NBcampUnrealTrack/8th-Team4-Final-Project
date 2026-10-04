#pragma once

#include "CoreMinimal.h"
#include "GAS/CueNotify/RogueMontageCueNotify.h"
#include "GameMode/Types/DominationTypes.h"
#include "RogueDashCueNotify.generated.h"

USTRUCT(BlueprintType)
struct FPS_API FDashClassMontages
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
	TObjectPtr<UAnimMontage> Primary;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
	TObjectPtr<UAnimMontage> Pistol;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
	TObjectPtr<UAnimMontage> Knife;
};

// Cosmetic selection only: distance, duration, cooldown and prediction remain in the ability.
UCLASS()
class FPS_API URogueDashCueNotify : public URogueMontageCueNotify
{
	GENERATED_BODY()
public:
	// Hard asset references keep all nine class/grip variants available in cooked builds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Dash")
	TMap<EDominationCombatClass, FDashClassMontages> ClassMontages;

protected:
	virtual UAnimMontage* ResolveMontage3P(AActor* Target) const override;
};
