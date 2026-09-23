// ShooterMatchOutcomeScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterMatchOutcomeScreen.generated.h"

class UTextBlock;
class UWidgetAnimation;

// 매치 승패 연출. 모드를 모르고 호출부가 판정한 승패와 점수만 받는다
UCLASS(Abstract)
class FPS_API UShooterMatchOutcomeScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UShooterMatchOutcomeScreen();

	void ShowOutcome(bool bVictory, int32 LocalScore, int32 EnemyScore);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Victory;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Defeat;
};
