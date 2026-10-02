#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DominationSandstormWarningWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidgetAnimation;

UCLASS(Abstract, Blueprintable)
class FPS_API UDominationSandstormWarningWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetWarning(float SecondsRemaining, float WarningDuration);
	// Anim_Exit 이 없으면 즉시 제거한다.
	void PlayExitAndRemove();

protected:
	virtual void NativeConstruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CountdownText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CountdownProgress;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Enter;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Exit;

private:
	int32 DisplayedSeconds = INDEX_NONE;
};
