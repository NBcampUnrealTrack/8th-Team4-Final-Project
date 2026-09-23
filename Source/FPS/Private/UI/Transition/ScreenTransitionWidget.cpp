// ScreenTransitionWidget.cpp
#include "UI/Transition/ScreenTransitionWidget.h"

#include "Animation/WidgetAnimation.h"

namespace
{
	// 재진입 상태 위함
	void ExecuteAndClear(FSimpleDelegate& Delegate)
	{
		FSimpleDelegate Local = MoveTemp(Delegate);
		Delegate.Unbind();
		Local.ExecuteIfBound();
	}
}

void UScreenTransitionWidget::PlayCover(FSimpleDelegate OnFinished, const bool bInstant)
{
	PendingCoverFinished = MoveTemp(OnFinished);

	// 기본 상태가 덮인 상태라 애니를 안 틀면 그 자체로 덮여 있다
	if (bInstant || !Anim_Cover)
	{
		ExecuteAndClear(PendingCoverFinished);
		return;
	}

	UnbindAllFromAnimationFinished(Anim_Cover);
	CoverFinishedEvent.BindDynamic(this, &ThisClass::HandleCoverAnimationFinished);
	BindToAnimationFinished(Anim_Cover, CoverFinishedEvent);

	PlayAnimation(Anim_Cover);
}

void UScreenTransitionWidget::PlayReveal(FSimpleDelegate OnFinished)
{
	PendingRevealFinished = MoveTemp(OnFinished);

	if (!Anim_Reveal)
	{
		ExecuteAndClear(PendingRevealFinished);
		return;
	}

	UnbindAllFromAnimationFinished(Anim_Reveal);
	RevealFinishedEvent.BindDynamic(this, &ThisClass::HandleRevealAnimationFinished);
	BindToAnimationFinished(Anim_Reveal, RevealFinishedEvent);

	PlayAnimation(Anim_Reveal);
}

void UScreenTransitionWidget::HandleCoverAnimationFinished()
{
	ExecuteAndClear(PendingCoverFinished);
}

void UScreenTransitionWidget::HandleRevealAnimationFinished()
{
	ExecuteAndClear(PendingRevealFinished);
}


