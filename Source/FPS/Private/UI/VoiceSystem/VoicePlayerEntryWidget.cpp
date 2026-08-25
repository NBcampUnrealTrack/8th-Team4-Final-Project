#include "UI/VoiceSystem/VoicePlayerEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "Voice/VoiceChatSubsystem.h"

void UVoicePlayerEntryWidget::NativeDestruct()
{
	if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(VoicePlayerState))
	{
		ShooterPlayerState->OnVoiceTalkingChanged.RemoveDynamic(this, &ThisClass::HandleVoiceTalkingChanged);
	}

	Super::NativeDestruct();
}

void UVoicePlayerEntryWidget::SetupVoicePlayerEntry(APlayerState* InPlayerState, APlayerState* InLocalPlayerState)
{
	if (AShooterPlayerState* PreviousShooterPlayerState = Cast<AShooterPlayerState>(VoicePlayerState))
	{
		PreviousShooterPlayerState->OnVoiceTalkingChanged.RemoveDynamic(this, &ThisClass::HandleVoiceTalkingChanged);
	}

	VoicePlayerState = InPlayerState;
	LocalPlayerState = InLocalPlayerState;

	if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(VoicePlayerState))
	{
		ShooterPlayerState->OnVoiceTalkingChanged.AddUniqueDynamic(this, &ThisClass::HandleVoiceTalkingChanged);
	}

	RefreshVoicePlayerEntry();
}

void UVoicePlayerEntryWidget::RefreshVoicePlayerEntry()
{
	const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(VoicePlayerState);
	const bool bIsTalking = IsValid(ShooterPlayerState) && ShooterPlayerState->IsVoiceTalking();

	const UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem();
	const bool bIsMuted = IsValid(VoiceChatSubsystem) && VoiceChatSubsystem->IsPlayerMuted(VoicePlayerState);
	const bool bCanHearVoice = !IsValid(VoiceChatSubsystem)
		|| IsLocalPlayerEntry()
		|| VoiceChatSubsystem->CanHearVoice(LocalPlayerState, VoicePlayerState);

	const FString DisplayName = GetDisplayName();

	if (PlayerNameText)
	{
		PlayerNameText->SetText(FText::FromString(DisplayName));
	}

	if (CharacterImage)
	{
		CharacterImage->SetBrush(DefaultCharacterBrush);
	}

	if (MuteIcon)
	{
		MuteIcon->SetBrush(bIsMuted ? MutedIconBrush : UnmutedIconBrush);
	}

	SetColorAndOpacity(bIsTalking ? TalkingEntryColor : NormalEntryColor);

	OnVoicePlayerEntryUpdated(DisplayName, IsLocalPlayerEntry(), bIsTalking, bIsMuted, bCanHearVoice);
}

void UVoicePlayerEntryWidget::TogglePlayerMute()
{
	if (IsLocalPlayerEntry())
	{
		return;
	}

	if (UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem())
	{
		VoiceChatSubsystem->TogglePlayerMuted(VoicePlayerState);
		RefreshVoicePlayerEntry();
	}
}

void UVoicePlayerEntryWidget::HandleVoiceTalkingChanged(AShooterPlayerState* ChangedPlayerState, bool)
{
	if (ChangedPlayerState == VoicePlayerState)
	{
		RefreshVoicePlayerEntry();
	}
}

UVoiceChatSubsystem* UVoicePlayerEntryWidget::GetVoiceChatSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UVoiceChatSubsystem>();
	}

	return nullptr;
}

FString UVoicePlayerEntryWidget::GetDisplayName() const
{
	if (!IsValid(VoicePlayerState))
	{
		return TEXT("Unknown");
	}

	const FString PlayerName = VoicePlayerState->GetPlayerName();
	return PlayerName.IsEmpty() ? GetNameSafe(VoicePlayerState) : PlayerName;
}

bool UVoicePlayerEntryWidget::IsLocalPlayerEntry() const
{
	return IsValid(VoicePlayerState) && VoicePlayerState == LocalPlayerState;
}
