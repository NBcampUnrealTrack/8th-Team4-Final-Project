// ShooterMatchResultRowWidget.cpp
#include "UI/OutGame/MatchResult/ShooterMatchResultRowWidget.h"

#include "CommonTextBlock.h"

void UShooterMatchResultRowWidget::SetRowData(
	const FShooterMatchResultRow& InRow,
	bool bStatsAvailable
	,EShooterResultRowSide Side)
{
	if (Text_PlayerName)
	{
		Text_PlayerName->SetText(InRow.PlayerName);
	}

	if (Panel_Stats)
	{
		Panel_Stats->SetVisibility(
			bStatsAvailable
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Hidden
		);
	}
	
	if (bStatsAvailable)
	{
		if (Text_Kills)
		{
			Text_Kills->SetText(FText::AsNumber(InRow.Kills));
		}

		if (Text_Deaths)
		{
			Text_Deaths->SetText(FText::AsNumber(InRow.Deaths));
		}

		if (Text_Assists)
		{
			Text_Assists->SetText(FText::AsNumber(InRow.Assists));
		}
	}

	BP_ApplyTeamStyle(Side);
}
