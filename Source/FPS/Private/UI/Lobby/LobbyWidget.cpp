// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Lobby/LobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFlow/Lobby/LobbyGameMode.h"
#include "GameFlow/Lobby/LobbyGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Session/SessionSubSystem.h"
#include "TimerManager.h"
#include "UI/Lobby/LobbyPlayerEntryWidget.h"

namespace LobbyWidgetRefresh
{
	constexpr int32 MaxInitialRefreshRetries = 5;
	constexpr float InitialRefreshRetryInterval = 0.1f;
}

void ULobbyWidget::SetLobbyReady(bool bNewLobbyReady)
{
	AShooterPlayerController* PlayerController = GetGameInstance()
		? Cast<AShooterPlayerController>(GetGameInstance()->GetFirstLocalPlayerController())
		: nullptr;
	if (!PlayerController)
	{
		SetStatusMessage(FText::FromString(TEXT("Player controller is not available.")));
		return;
	}

	PlayerController->SetLobbyReady(bNewLobbyReady);
	SetStatusMessage(bNewLobbyReady
		? FText::FromString(TEXT("Ready."))
		: FText::FromString(TEXT("Ready canceled.")));
}

void ULobbyWidget::StartRoomGame()
{
	ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	if (!LobbyGameMode)
	{
		SetStatusMessage(FText::FromString(TEXT("Lobby game mode is not available.")));
		OnStartRoomGameResult(false);
		return;
	}

	SetStatusMessage(FText::FromString(TEXT("Starting game...")));
	if (!LobbyGameMode->RequestStartRoomGame(GetOwningPlayer(), MapName))
	{
		SetStatusMessage(FText::FromString(TEXT("Cannot start until every client is ready.")));
		OnStartRoomGameResult(false);
		UpdateLobbyButtons();
	}
}

bool ULobbyWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (ReadyButton)
	{
		ReadyButton->OnClicked.AddDynamic(this, &ThisClass::ReadyButtonClicked);
	}

	if (StartButton)
	{
		StartButton->OnClicked.AddDynamic(this, &ThisClass::StartButtonClicked);
	}

	BindSessionDelegates();
	return true;
}

void ULobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitialLobbyRefreshRetryCount = 0;
	BindLobbyGameState();
	RefreshLobbyPlayerList();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			InitialLobbyRefreshRetryTimerHandle,
			this,
			&ThisClass::RetryInitialLobbyRefresh,
			LobbyWidgetRefresh::InitialRefreshRetryInterval,
			true);
	}
}

void ULobbyWidget::NativeDestruct()
{
	StopInitialLobbyRefreshRetry();
	UnbindLobbyGameState();
	UnbindSessionDelegates();
	Super::NativeDestruct();
}

void ULobbyWidget::ReadyButtonClicked()
{
	SetLobbyReady(true);

	if (ReadyButton)
	{
		ReadyButton->SetIsEnabled(false);
	}
}

void ULobbyWidget::StartButtonClicked()
{
	StartRoomGame();
}

void ULobbyWidget::HandleStartRoomGameComplete(bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		SetStatusMessage(FText::FromString(TEXT("Cannot start until every client is ready.")));
	}

	OnStartRoomGameResult(bWasSuccessful);
	UpdateLobbyButtons();
}

void ULobbyWidget::BindSessionDelegates()
{
	CacheSessionSubsystem();

	if (bSessionDelegatesBound || !SessionSubsystem)
	{
		return;
	}

	SessionSubsystem->OnStartRoomGameComplete.AddDynamic(this, &ThisClass::HandleStartRoomGameComplete);
	bSessionDelegatesBound = true;
}

void ULobbyWidget::UnbindSessionDelegates()
{
	if (!bSessionDelegatesBound || !SessionSubsystem)
	{
		return;
	}

	SessionSubsystem->OnStartRoomGameComplete.RemoveDynamic(this, &ThisClass::HandleStartRoomGameComplete);
	bSessionDelegatesBound = false;
}

void ULobbyWidget::BindLobbyGameState()
{
	ALobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	if (!IsValid(LobbyGameState) || BoundLobbyGameState == LobbyGameState)
	{
		return;
	}

	UnbindLobbyGameState();
	BoundLobbyGameState = LobbyGameState;
	BoundLobbyGameState->OnLobbyPlayerListChanged.AddUniqueDynamic(this, &ThisClass::HandleLobbyPlayerListChanged);
}

void ULobbyWidget::UnbindLobbyGameState()
{
	if (IsValid(BoundLobbyGameState))
	{
		BoundLobbyGameState->OnLobbyPlayerListChanged.RemoveDynamic(this, &ThisClass::HandleLobbyPlayerListChanged);
	}

	BoundLobbyGameState = nullptr;
}

void ULobbyWidget::RefreshLobbyPlayerList()
{
	UpdateLobbyButtons();

	if (!PlayerListBox)
	{
		return;
	}

	const FString NewSignature = BuildLobbyListSignature();
	if (CachedLobbyListSignature == NewSignature)
	{
		return;
	}

	CachedLobbyListSignature = NewSignature;
	PlayerListBox->ClearChildren();

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	for (int32 PlayerIndex = 0; PlayerIndex < GameState->PlayerArray.Num(); ++PlayerIndex)
	{
		const APlayerState* PlayerState = GameState->PlayerArray[PlayerIndex];
		if (!IsValid(PlayerState))
		{
			continue;
		}

		const bool bIsHost = PlayerIndex == 0;
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		const bool bIsReady = bIsHost || (ShooterPlayerState && ShooterPlayerState->IsLobbyReady());
		const FString PlayerName = PlayerState->GetPlayerName().IsEmpty()
			? GetNameSafe(PlayerState)
			: PlayerState->GetPlayerName();

		if (LobbyPlayerEntryWidgetClass)
		{
			ULobbyPlayerEntryWidget* EntryWidget = CreateWidget<ULobbyPlayerEntryWidget>(this, LobbyPlayerEntryWidgetClass);
			if (EntryWidget)
			{
				EntryWidget->SetupLobbyPlayerEntry(PlayerName, bIsHost, bIsReady);
				PlayerListBox->AddChild(EntryWidget);
			}
			continue;
		}

		if (WidgetTree)
		{
			UTextBlock* TextRow = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			if (TextRow)
			{
				TextRow->SetText(FText::FromString(FString::Printf(
					TEXT("%s%s - %s"),
					bIsHost ? TEXT("[Host] ") : TEXT(""),
					*PlayerName,
					bIsReady ? TEXT("Ready") : TEXT("Waiting"))));
				PlayerListBox->AddChild(TextRow);
			}
		}
	}
}

void ULobbyWidget::RetryInitialLobbyRefresh()
{
	++InitialLobbyRefreshRetryCount;

	BindLobbyGameState();
	RefreshLobbyPlayerList();

	if (InitialLobbyRefreshRetryCount >= LobbyWidgetRefresh::MaxInitialRefreshRetries)
	{
		StopInitialLobbyRefreshRetry();
	}
}

void ULobbyWidget::StopInitialLobbyRefreshRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitialLobbyRefreshRetryTimerHandle);
	}
}

void ULobbyWidget::UpdateLobbyButtons()
{
	const bool bIsHost = IsLocalPlayerHost();

	if (ReadyButton)
	{
		ReadyButton->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (StartButton)
	{
		StartButton->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		const ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
		StartButton->SetIsEnabled(LobbyGameMode && LobbyGameMode->CanStartRoomGame());
	}
}

bool ULobbyWidget::IsLocalPlayerHost() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetNetMode() != NM_Client;
}

FString ULobbyWidget::BuildLobbyListSignature() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!IsValid(GameState))
	{
		return TEXT("");
	}

	FString Signature;
	for (int32 PlayerIndex = 0; PlayerIndex < GameState->PlayerArray.Num(); ++PlayerIndex)
	{
		const APlayerState* PlayerState = GameState->PlayerArray[PlayerIndex];
		if (!IsValid(PlayerState))
		{
			continue;
		}

		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		const bool bIsReady = PlayerIndex == 0 || (ShooterPlayerState && ShooterPlayerState->IsLobbyReady());
		Signature += FString::Printf(
			TEXT("%s:%d:%d|"),
			*PlayerState->GetPlayerName(),
			PlayerIndex,
			bIsReady ? 1 : 0);
	}

	return Signature;
}

void ULobbyWidget::HandleLobbyPlayerListChanged()
{
	RefreshLobbyPlayerList();
}
