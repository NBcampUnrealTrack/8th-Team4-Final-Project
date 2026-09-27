#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DominationSandstormWarningWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class FPS_API UDominationSandstormWarningWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetWarning(float SecondsRemaining, float WarningDuration);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CountdownText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CountdownProgress;

private:
	int32 DisplayedSeconds = INDEX_NONE;
};
