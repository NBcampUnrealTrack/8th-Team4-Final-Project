// ScreenTransitionWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ScreenTransitionWidget.generated.h"

class UWidgetAnimation;

// 트랜지션 애니메이션 위젯
UCLASS(Abstract)
class FPS_API UScreenTransitionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// bInstant 거나 Anim_Cover 가 없으면 기본 상태(덮임) 
	void PlayCover(FSimpleDelegate OnFinished, bool bInstant);
	void PlayReveal(FSimpleDelegate OnFinished);

private:
	UFUNCTION()
	void HandleCoverAnimationFinished();
	UFUNCTION()
	void HandleRevealAnimationFinished();

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Cover;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Reveal;

	FWidgetAnimationDynamicEvent CoverFinishedEvent;
	FWidgetAnimationDynamicEvent RevealFinishedEvent;

	FSimpleDelegate PendingCoverFinished;
	FSimpleDelegate PendingRevealFinished;
};
