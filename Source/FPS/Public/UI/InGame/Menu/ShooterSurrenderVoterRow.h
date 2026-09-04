#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMode/Types/SurrenderVoteTypes.h"
#include "ShooterSurrenderVoterRow.generated.h"

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
};
