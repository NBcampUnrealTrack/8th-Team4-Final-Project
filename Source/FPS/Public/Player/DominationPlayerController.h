#pragma once

#include "CoreMinimal.h"
#include "GameMode/DominationTypes.h"
#include "Player/ShooterPlayerController.h"
#include "DominationPlayerController.generated.h"

UCLASS()
class FPS_API ADominationPlayerController : public AShooterPlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Domination|Class")
	void SelectCombatClass(EDominationCombatClass RequestedClass);

private:
	UFUNCTION(Server, Reliable)
	void ServerSelectCombatClass(EDominationCombatClass RequestedClass);
};
