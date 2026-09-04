#include "UI/InGame/Spectator/ShooterSpectatorWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerState.h"

void UShooterSpectatorWidget::SetSpectatorTarget(APlayerState* TargetPlayerState)
{
	if (TargetNameText)
	{
		TargetNameText->SetText(BuildTargetText(TargetPlayerState));
	}

	if (HintText)
	{
		HintText->SetText(FText::FromString(TEXT("Fire: Next teammate")));
	}
}

void UShooterSpectatorWidget::ClearSpectatorTarget()
{
	if (TargetNameText)
	{
		TargetNameText->SetText(FText::FromString(TEXT("No teammate alive")));
	}

	if (HintText)
	{
		HintText->SetText(FText::FromString(TEXT("Waiting for next spawn")));
	}
}

FText UShooterSpectatorWidget::BuildTargetText(APlayerState* TargetPlayerState) const
{
	if (!IsValid(TargetPlayerState))
	{
		return FText::FromString(TEXT("No teammate alive"));
	}

	const FString PlayerName = TargetPlayerState->GetPlayerName();
	return FText::FromString(PlayerName.IsEmpty() ? GetNameSafe(TargetPlayerState) : PlayerName);
}
