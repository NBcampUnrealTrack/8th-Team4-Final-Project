// ShooterMatchOutcomeScreen.cpp
#include "UI/InGame/ShooterMatchOutcomeScreen.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"

UShooterMatchOutcomeScreen::UShooterMatchOutcomeScreen()
{
	InputConfig = EShooterWidgetInputMode::Game;
}

void UShooterMatchOutcomeScreen::ShowOutcome(const bool bVictory, const int32 LocalScore, const int32 EnemyScore)
{
	// 내 팀이 왼쪽
	if (Text_Score)
	{
		Text_Score->SetText(FText::Format(
			NSLOCTEXT("MatchOutcome", "Score", "{0} : {1}"),
			FText::AsNumber(LocalScore),
			FText::AsNumber(EnemyScore)));
	}

	// 승패 이미지는 각 애니가 드러낸다
	if (UWidgetAnimation* Animation = bVictory ? Anim_Victory.Get() : Anim_Defeat.Get())
	{
		PlayAnimation(Animation);
	}
}
