#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/Types/SurrenderVoteTypes.h"
#include "ShooterSurrenderVoterRow.generated.h"

class UBorder;
class UCheckBox;
class UTextBlock;

/** 투표자 한 명의 이름과 현재 선택을 읽기 전용으로 표시한다. */
UCLASS(Abstract)
class FPS_API UShooterSurrenderVoterRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetVoterState(const FSurrenderVoterState& VoterState);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> Check_VoteStatus;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_VoteStatus;

	// 상태별 배지 배경
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_StatusBadge;

	UPROPERTY(EditDefaultsOnly, Category = "Surrender|Badge")
	FSlateBrush ApproveBadgeBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Surrender|Badge")
	FSlateBrush PendingBadgeBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Surrender|Badge")
	FSlateBrush RejectBadgeBrush;

	// 기본값은 화면이 직접 행을 만들던 때의 색
	UPROPERTY(EditDefaultsOnly, Category = "Surrender|Badge")
	FLinearColor ApproveTextColor = FLinearColor(0.12f, 1.00f, 0.74f, 1.00f);

	UPROPERTY(EditDefaultsOnly, Category = "Surrender|Badge")
	FLinearColor PendingTextColor = FLinearColor(1.00f, 0.78f, 0.28f, 1.00f);

	UPROPERTY(EditDefaultsOnly, Category = "Surrender|Badge")
	FLinearColor RejectTextColor = FLinearColor(1.00f, 0.38f, 0.38f, 1.00f);
};
