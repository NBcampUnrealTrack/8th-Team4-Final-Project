#include "UI/VoiceSystem/VoicePanelWidget.h"

#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "UI/VoiceSystem/VoicePlayerEntryWidget.h"

void UVoicePanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UVoicePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const AShooterPlayerController* ShooterController = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (ShooterController && !ShooterController->SupportsVoiceChat())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		EntryWidgets.Reset();
		if (PlayerListPanel)
		{
			PlayerListPanel->ClearChildren();
		}
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem())
	{
		VoiceChatSubsystem->OnVoiceMuteChanged.AddUniqueDynamic(this, &ThisClass::HandleMuteChanged);
	}

	RefreshVoicePanel();
}

void UVoicePanelWidget::NativeDestruct()
{
	if (UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem())
	{
		VoiceChatSubsystem->OnVoiceMuteChanged.RemoveDynamic(this, &ThisClass::HandleMuteChanged);
	}

	EntryWidgets.Reset();
	Super::NativeDestruct();
}

void UVoicePanelWidget::RefreshVoicePanel()
{
	const AShooterPlayerController* ShooterController = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (ShooterController && !ShooterController->SupportsVoiceChat())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		EntryWidgets.Reset();
		if (PlayerListPanel)
		{
			PlayerListPanel->ClearChildren();
		}
		return;
	}

	EntryWidgets.Reset();

	if (PlayerListPanel)
	{
		PlayerListPanel->ClearChildren();
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	UWorld* World = GetWorld();
	if (!IsValid(OwningPlayer) || !IsValid(World) || !EntryWidgetClass || !PlayerListPanel)
	{
		NotifyAudienceUpdated();

		const UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem();
		OnVoicePanelRefreshed(
			0,
			IsValid(VoiceChatSubsystem) ? VoiceChatSubsystem->GetVoiceAudience() : EVoiceChatAudience::Everyone);
		return;
	}

	AGameStateBase* GameState = World->GetGameState();
	APlayerState* LocalPlayerState = GetLocalPlayerState();
	if (!IsValid(GameState))
	{
		NotifyAudienceUpdated();

		const UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem();
		OnVoicePanelRefreshed(
			0,
			IsValid(VoiceChatSubsystem) ? VoiceChatSubsystem->GetVoiceAudience() : EVoiceChatAudience::Everyone);
		return;
	}

	const ABaseTeamPlayerState* LocalTeamPlayerState = Cast<ABaseTeamPlayerState>(LocalPlayerState);
	const EGameTeam LocalTeam = IsValid(LocalTeamPlayerState)
		? LocalTeamPlayerState->GetTeam()
		: EGameTeam::None;

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const ABaseTeamPlayerState* CandidateTeamPlayerState = Cast<ABaseTeamPlayerState>(PlayerState);
		if (!IsValid(CandidateTeamPlayerState)
			|| LocalTeam == EGameTeam::None
			|| LocalTeam != CandidateTeamPlayerState->GetTeam())
		{
			continue;
		}

		UVoicePlayerEntryWidget* EntryWidget = CreateWidget<UVoicePlayerEntryWidget>(OwningPlayer, EntryWidgetClass);
		if (!IsValid(EntryWidget))
		{
			continue;
		}

		EntryWidget->SetupVoicePlayerEntry(PlayerState, LocalPlayerState);
		PlayerListPanel->AddChild(EntryWidget);
		EntryWidgets.Add(EntryWidget);
	}

	NotifyAudienceUpdated();

	const UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem();
	OnVoicePanelRefreshed(
		EntryWidgets.Num(),
		IsValid(VoiceChatSubsystem) ? VoiceChatSubsystem->GetVoiceAudience() : EVoiceChatAudience::Everyone);
}

void UVoicePanelWidget::SetVoiceAudience(const EVoiceChatAudience NewAudience)
{
	if (UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem())
	{
		VoiceChatSubsystem->SetVoiceAudience(NewAudience);
	}

	RefreshVoicePanel();
}

void UVoicePanelWidget::HandleMuteChanged(APlayerState*, bool)
{
	for (UVoicePlayerEntryWidget* EntryWidget : EntryWidgets)
	{
		if (IsValid(EntryWidget))
		{
			EntryWidget->RefreshVoicePlayerEntry();
		}
	}
}

void UVoicePanelWidget::NotifyAudienceUpdated()
{
	const UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem();
	const EVoiceChatAudience Audience = IsValid(VoiceChatSubsystem)
		? VoiceChatSubsystem->GetVoiceAudience()
		: EVoiceChatAudience::Everyone;

	OnVoiceAudienceUpdated(Audience);
}

APlayerState* UVoicePanelWidget::GetLocalPlayerState() const
{
	if (const APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return OwningPlayer->PlayerState;
	}

	return nullptr;
}

UVoiceChatSubsystem* UVoicePanelWidget::GetVoiceChatSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UVoiceChatSubsystem>();
	}

	return nullptr;
}
