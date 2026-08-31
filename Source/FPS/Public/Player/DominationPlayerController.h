#pragma once

#include "CoreMinimal.h"
#include "GameMode/DominationTypes.h"
#include "Player/ShooterPlayerController.h"
#include "DominationPlayerController.generated.h"

class UDominationUIInitComponent;

UCLASS()
class FPS_API ADominationPlayerController : public AShooterPlayerController
{
	GENERATED_BODY()

public:
	ADominationPlayerController();
	virtual bool CanProcessGameplayInput() const override;

	UFUNCTION(BlueprintCallable, Category = "Domination|Class")
	void SelectCombatClass(EDominationCombatClass RequestedClass);

private:
	UFUNCTION(Server, Reliable)
	void ServerSelectCombatClass(EDominationCombatClass RequestedClass);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDominationUIInitComponent> UIInitComponent;
};
