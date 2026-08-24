#include "UI/InGame/Menu/ShooterSurrenderVoterRow.h"

#include "Components/CheckBox.h"
#include "Components/TextBlock.h"

void UShooterSurrenderVoterRow::SetVoterState(const FSurrenderVoterState& VoterState)
{
	Text_PlayerName->SetText(FText::FromString(VoterState.PlayerName));
	Check_VoteStatus->SetIsChecked(VoterState.Choice == ESurrenderVoteChoice::Approve);
	Check_VoteStatus->SetIsEnabled(false);

	switch (VoterState.Choice)
	{
	case ESurrenderVoteChoice::Approve:
		Text_VoteStatus->SetText(NSLOCTEXT("ShooterSurrender", "Approved", "찬성"));
		break;
	case ESurrenderVoteChoice::Reject:
		Text_VoteStatus->SetText(NSLOCTEXT("ShooterSurrender", "Rejected", "반대"));
		break;
	default:
		Text_VoteStatus->SetText(NSLOCTEXT("ShooterSurrender", "Pending", "대기"));
		break;
	}
}
