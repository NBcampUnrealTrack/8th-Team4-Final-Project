// ShooterChatRouting.cpp
#include "Chat/ShooterChatRouting.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerController.h"

void ShooterChatRouting::ForEachChatRecipient(
	const AGameStateBase* GameState,
	const APlayerState* Sender,
	const EChatChannel Channel,
	TFunctionRef<void(AShooterPlayerController*)> Visitor)
{
	if (!IsValid(GameState) || !GameState->HasAuthority() || !IsValid(Sender))
	{
		return;
	}

	const ABaseTeamPlayerState* SenderTeamState = Cast<ABaseTeamPlayerState>(Sender);
	const EGameTeam SenderTeam = IsValid(SenderTeamState) ? SenderTeamState->GetTeam() : EGameTeam::None;

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!IsValid(PlayerState))
		{
			continue;
		}

		if (Channel == EChatChannel::Team)
		{
			const ABaseTeamPlayerState* RecipientTeamState = Cast<ABaseTeamPlayerState>(PlayerState);
			if (!IsValid(RecipientTeamState) || RecipientTeamState->GetTeam() != SenderTeam)
			{
				continue;
			}
		}

		if (AShooterPlayerController* RecipientController = Cast<AShooterPlayerController>(PlayerState->GetPlayerController()))
		{
			Visitor(RecipientController);
		}
	}
}

AShooterPlayerController* ShooterChatRouting::FindChatRecipientByName(
	const AGameStateBase* GameState, const FString& PlayerName)
{
	if (!IsValid(GameState) || !GameState->HasAuthority())
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!IsValid(PlayerState) || !PlayerState->GetPlayerName().Equals(PlayerName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		return Cast<AShooterPlayerController>(PlayerState->GetPlayerController());
	}

	return nullptr;
}

TArray<FString> ShooterChatRouting::GetPlayerNamesMatchingPrefix(
	const AGameStateBase* GameState, const FString& Prefix, const FString& ExcludeName)
{
	TArray<FString> Results;
	if (!IsValid(GameState) || Prefix.IsEmpty())
	{
		return Results;
	}

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (!IsValid(PlayerState))
		{
			continue;
		}

		const FString Name = PlayerState->GetPlayerName();
		if (!Name.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!ExcludeName.IsEmpty() && Name.Equals(ExcludeName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		Results.Add(Name);
	}

	return Results;
}
