#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "ShooterKeyBindingProxy.generated.h"

class UShooterOptionsScreen;

UCLASS()
class FPS_API UShooterKeyBindingProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UShooterOptionsScreen* InOwner, FName InBindingId);

	UFUNCTION()
	void HandleKeySelected(FInputChord SelectedKey);

private:
	TWeakObjectPtr<UShooterOptionsScreen> Owner;
	FName BindingId;
};
