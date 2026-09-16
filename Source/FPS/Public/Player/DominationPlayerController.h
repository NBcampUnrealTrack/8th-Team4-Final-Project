#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
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

	UFUNCTION(BlueprintCallable, Category = "Domination|Class")
	void SetPreviewCombatClass(EDominationCombatClass RequestedClass);

protected:
	virtual void SetupInputComponent() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerSelectCombatClass(EDominationCombatClass RequestedClass);
	UFUNCTION(Server, Reliable)
	void ServerSetPreviewCombatClass(EDominationCombatClass RequestedClass);

	void Input_ToggleAugmentSelect();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDominationUIInitComponent> UIInitComponent;
};
