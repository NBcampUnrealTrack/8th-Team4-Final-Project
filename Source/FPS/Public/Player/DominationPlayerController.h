#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "Player/ShooterPlayerController.h"
#include "Interfaces/RogueCombatClassSelector.h"
#include "DominationPlayerController.generated.h"

class UDominationUIInitComponent;
class UDominationSandstormComponent;

UCLASS()
class FPS_API ADominationPlayerController : public AShooterPlayerController, public IRogueCombatClassSelector
{
	GENERATED_BODY()

public:
	ADominationPlayerController();
	virtual bool CanProcessGameplayInput() const override;
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	virtual void NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest) override;

	UFUNCTION(BlueprintCallable, Category = "Domination|Class")
	virtual void SelectCombatClass(EDominationCombatClass RequestedClass) override;

	UFUNCTION(BlueprintCallable, Category = "Domination|Class")
	virtual void SetPreviewCombatClass(EDominationCombatClass RequestedClass) override;

	virtual float GetCombatClassSelectionTimeRemaining() const override;

	UFUNCTION(Exec)
	void SandstormPreview(float Intensity = -1.0f);

protected:
	virtual void SetupInputComponent() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerSelectCombatClass(EDominationCombatClass RequestedClass);
	UFUNCTION(Server, Reliable)
	void ServerSetPreviewCombatClass(EDominationCombatClass RequestedClass);

	void Input_ToggleAugmentSelect();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Sandstorm", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDominationSandstormComponent> SandstormComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDominationUIInitComponent> UIInitComponent;

	// [INPUTDBG] CanProcessGameplayInput 조건 변화 로그용 임시 변수. 원인 확인 후 제거할 것.
	mutable uint32 LastInputDebugMask = MAX_uint32;
};
