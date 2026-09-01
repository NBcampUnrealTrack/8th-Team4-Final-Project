// ShooterKillFeedEntry.cpp
#include "UI/InGame/HUD/ShooterKillFeedEntry.h"

#include "CommonTextBlock.h"
#include "Player/BaseTeamPlayerState.h"
#include "UI/ShooterUISettings.h"

void UShooterKillFeedEntry::SetData(const FShooterKillFeedEntryData& InData)
{
	if (Text_KillerName)
	{
		Text_KillerName->SetText(InData.KillerName);
	}
	
	if (Text_VictimName)
	{
		Text_VictimName->SetText(InData.VictimName);
	}
	
	const ABaseTeamPlayerState* LocalPlayerState =
		GetOwningPlayerState<ABaseTeamPlayerState>();
	
	const EGameTeam LocalTeam =
		LocalPlayerState ? LocalPlayerState->GetTeam() : EGameTeam::None;
	
	if (LocalTeam != EGameTeam::None)
	{
		const bool bKillerIsAlly = InData.KillerTeam == LocalTeam;
		const bool bVictimIsAlly = InData.VictimTeam == LocalTeam;
		
		RefreshTeamVisual(bKillerIsAlly, bVictimIsAlly);
	}
	
	if (Icon_Headshot)
	{
		Icon_Headshot->SetVisibility(InData.bHeadshot ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
