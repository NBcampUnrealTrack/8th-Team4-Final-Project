#include "UI/VoiceSystem/VoicePanelWidget.h"

#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "TimerManager.h"
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

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReconcileTimerHandle, this, &ThisClass::ReconcileTeammates, ReconcileInterval, true);
	}
}

void UVoicePanelWidget::NativeDestruct()
{
	if (UVoiceChatSubsystem* VoiceChatSubsystem = GetVoiceChatSubsystem())
	{
		VoiceChatSubsystem->OnVoiceMuteChanged.RemoveDynamic(this, &ThisClass::HandleMuteChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconcileTimerHandle);
	}

	UnbindAllTalkingEvents();
	EntryWidgets.Reset();
	Super::NativeDestruct();
}

void UVoicePanelWidget::RefreshVoicePanel()
{
	const AShooterPlayerController* ShooterController = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (ShooterController && !ShooterController->SupportsVoiceChat())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		UnbindAllTalkingEvents();
		EntryWidgets.Reset();
		if (PlayerListPanel)
		{
			PlayerListPanel->ClearChildren();
		}
		return;
	}

	UnbindAllTalkingEvents();
	EntryWidgets.Reset();

	if (PlayerListPanel)
	{
		PlayerListPanel->ClearChildren();
	}

	ReconcileTeammates();

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

void UVoicePanelWidget::HandleVoiceTalkingChanged(AShooterPlayerState* ChangedPlayerState, bool bIsTalking)
{
	if (bIsTalking)
	{
		AddTalkingEntry(ChangedPlayerState);
	}
	else
	{
		RemoveEntry(FindEntry(ChangedPlayerState));
	}
}

void UVoicePanelWidget::ReconcileTeammates()
{
	UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	const ABaseTeamPlayerState* LocalTeamPlayerState = Cast<ABaseTeamPlayerState>(GetLocalPlayerState());
	const EGameTeam LocalTeam = IsValid(LocalTeamPlayerState)
		? LocalTeamPlayerState->GetTeam()
		: EGameTeam::None;

	TSet<AShooterPlayerState*> Teammates;
	if (IsValid(GameState) && LocalTeam != EGameTeam::None)
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
			if (IsValid(ShooterPlayerState) && ShooterPlayerState->GetTeam() == LocalTeam)
			{
				Teammates.Add(ShooterPlayerState);
			}
		}
	}

	for (int32 Index = BoundPlayerStates.Num() - 1; Index >= 0; --Index)
	{
		AShooterPlayerState* PlayerState = BoundPlayerStates[Index].Get();
		if (IsValid(PlayerState) && Teammates.Contains(PlayerState))
		{
			continue;
		}

		if (PlayerState)
		{
			PlayerState->OnVoiceTalkingChanged.RemoveDynamic(this, &ThisClass::HandleVoiceTalkingChanged);
		}
		BoundPlayerStates.RemoveAtSwap(Index, EAllowShrinking::No);
	}

	for (int32 Index = EntryWidgets.Num() - 1; Index >= 0; --Index)
	{
		UVoicePlayerEntryWidget* EntryWidget = EntryWidgets[Index];
		AShooterPlayerState* PlayerState = IsValid(EntryWidget)
			? Cast<AShooterPlayerState>(EntryWidget->GetVoicePlayerState())
			: nullptr;
		if (!IsValid(PlayerState) || !Teammates.Contains(PlayerState))
		{
			if (IsValid(EntryWidget))
			{
				EntryWidget->RemoveFromParent();
			}
			EntryWidgets.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		EntryWidget->RefreshProfileImage();
	}

	for (AShooterPlayerState* PlayerState : Teammates)
	{
		if (BoundPlayerStates.Contains(PlayerState))
		{
			continue;
		}

		PlayerState->OnVoiceTalkingChanged.AddUniqueDynamic(this, &ThisClass::HandleVoiceTalkingChanged);
		BoundPlayerStates.Add(PlayerState);
		if (PlayerState->IsVoiceTalking())
		{
			AddTalkingEntry(PlayerState);
		}
	}
}

void UVoicePanelWidget::AddTalkingEntry(AShooterPlayerState* PlayerState)
{
	if (!IsValid(PlayerState) || FindEntry(PlayerState) || !EntryWidgetClass || !PlayerListPanel)
	{
		return;
	}

	UVoicePlayerEntryWidget* EntryWidget = CreateWidget<UVoicePlayerEntryWidget>(GetOwningPlayer(), EntryWidgetClass);
	if (!IsValid(EntryWidget))
	{
		return;
	}

	EntryWidget->SetupVoicePlayerEntry(PlayerState, GetLocalPlayerState());
	// 새로 말한 사람은 맨 아래에 붙는다
	PlayerListPanel->AddChild(EntryWidget);
	EntryWidgets.Add(EntryWidget);
}

void UVoicePanelWidget::RemoveEntry(UVoicePlayerEntryWidget* EntryWidget)
{
	if (!IsValid(EntryWidget))
	{
		return;
	}

	EntryWidget->RemoveFromParent();
	EntryWidgets.RemoveSingleSwap(EntryWidget, EAllowShrinking::No);
}

UVoicePlayerEntryWidget* UVoicePanelWidget::FindEntry(const APlayerState* PlayerState) const
{
	for (UVoicePlayerEntryWidget* EntryWidget : EntryWidgets)
	{
		if (IsValid(EntryWidget) && EntryWidget->GetVoicePlayerState() == PlayerState)
		{
			return EntryWidget;
		}
	}

	return nullptr;
}

void UVoicePanelWidget::UnbindAllTalkingEvents()
{
	for (const TWeakObjectPtr<AShooterPlayerState>& BoundPlayerState : BoundPlayerStates)
	{
		if (AShooterPlayerState* PlayerState = BoundPlayerState.Get())
		{
			PlayerState->OnVoiceTalkingChanged.RemoveDynamic(this, &ThisClass::HandleVoiceTalkingChanged);
		}
	}

	BoundPlayerStates.Reset();
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
