#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShooterSpectatorWidget.generated.h"

class APlayerState;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class FPS_API UShooterSpectatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSpectatorTarget(APlayerState* TargetPlayerState);
	void ClearSpectatorTarget();

private:
	FText BuildTargetText(APlayerState* TargetPlayerState) const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TargetNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;
};
