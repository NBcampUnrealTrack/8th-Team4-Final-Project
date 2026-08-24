// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Lobby/LobbyWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/LobbyGameMode.h"
#include "GameMode/LobbyGameState.h"
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

void ULobbyWidget::ToggleLobbyReady()
{
	const AShooterPlayerState* LocalPlayerState = GetLocalShooterPlayerState();
	SetLobbyReady(!(IsValid(LocalPlayerState) && LocalPlayerState->IsLobbyReady()));
}

void ULobbyWidget::SelectTeam(const EGameTeam RequestedTeam)
{
	AShooterPlayerController* PlayerController = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (!PlayerController)
	{
		SetStatusMessage(FText::FromString(TEXT("Player controller is not available.")));
		return;
	}

	// 정원 초과 여부는 서버가 판단, 거부되면 플레이어 리스트가 기존 팀 그대로 유지
	PlayerController->SetLobbyTeam(RequestedTeam);

	// 화면에 뜨는 문구 주석 처리
	// SetStatusMessage(RequestedTeam == EGameTeam::TeamA
	// 	? FText::FromString(TEXT("Requested Team A."))
	// 	: FText::FromString(TEXT("Requested Team B.")));
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

	if (TeamAButton)
	{
		TeamAButton->OnClicked.AddDynamic(this, &ThisClass::TeamAButtonClicked);
	}

	if (TeamBButton)
	{
		TeamBButton->OnClicked.AddDynamic(this, &ThisClass::TeamBButtonClicked);
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
	ToggleLobbyReady();
}

void ULobbyWidget::StartButtonClicked()
{
	StartRoomGame();
}

void ULobbyWidget::TeamAButtonClicked()
{
	SelectTeam(EGameTeam::TeamA);
}

void ULobbyWidget::TeamBButtonClicked()
{
	SelectTeam(EGameTeam::TeamB);
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
		// 팀 정보도 함께 표시. 미선택(None)은 시작 시 자동 배정
		const EGameTeam PlayerTeam = ShooterPlayerState ? ShooterPlayerState->GetTeam() : EGameTeam::None;
		const FString PlayerName = PlayerState->GetPlayerName().IsEmpty()
			? GetNameSafe(PlayerState)
			: PlayerState->GetPlayerName();

		if (LobbyPlayerEntryWidgetClass)
		{
			ULobbyPlayerEntryWidget* EntryWidget = CreateWidget<ULobbyPlayerEntryWidget>(this, LobbyPlayerEntryWidgetClass);
			if (EntryWidget)
			{
				EntryWidget->SetupLobbyPlayerEntry(PlayerName, bIsHost, bIsReady, PlayerTeam);
				PlayerListBox->AddChild(EntryWidget);
			}
			continue;
		}

		if (WidgetTree)
		{
			UTextBlock* TextRow = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			if (TextRow)
			{
				const TCHAR* TeamLabel = PlayerTeam == EGameTeam::TeamA
					? TEXT("Red")
					: (PlayerTeam == EGameTeam::TeamB ? TEXT("Blue") : TEXT("Random"));
				TextRow->SetText(FText::FromString(FString::Printf(
					TEXT("%s%s [%s] - %s"),
					bIsHost ? TEXT("[Host] ") : TEXT(""),
					*PlayerName,
					TeamLabel,
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
	const AShooterPlayerState* LocalPlayerState = GetLocalShooterPlayerState();
	const bool bIsLocalReady = IsValid(LocalPlayerState) && LocalPlayerState->IsLobbyReady();

	if (ReadyButton)
	{
		ReadyButton->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// 토글 상태를 라벨에 반영
	if (ReadyButtonText)
	{
		ReadyButtonText->SetText(bIsLocalReady
			? FText::FromString(TEXT("Cancel Ready"))
			: FText::FromString(TEXT("Ready")));
	}

	// 이미 속한 팀 버튼은 비활성화해 현재 선택을 알 수 있게 함
	const EGameTeam LocalTeam = IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;
	if (TeamAButton)
	{
		TeamAButton->SetIsEnabled(LocalTeam != EGameTeam::TeamA);
		TeamAButton->SetBackgroundColor(LocalTeam == EGameTeam::TeamA
			? FLinearColor(1.0f, 0.15f, 0.15f )
			: FLinearColor(0.5f, 0.15f, 0.15f));
	}

	if (TeamBButton)
	{
		TeamBButton->SetIsEnabled(LocalTeam != EGameTeam::TeamB);
		TeamBButton->SetBackgroundColor(LocalTeam == EGameTeam::TeamB
			? FLinearColor(0.15f, 0.15f, 1.0f)
			: FLinearColor(0.15f, 0.15f, 0.5f));
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

AShooterPlayerState* ULobbyWidget::GetLocalShooterPlayerState() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	return IsValid(PlayerController) ? PlayerController->GetPlayerState<AShooterPlayerState>() : nullptr;
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
		// 팀이 바뀌어도 리스트가 갱신되도록 시그니처에 팀을 포함
		const EGameTeam PlayerTeam = ShooterPlayerState ? ShooterPlayerState->GetTeam() : EGameTeam::None;
		Signature += FString::Printf(
			TEXT("%s:%d:%d:%d|"),
			*PlayerState->GetPlayerName(),
			PlayerIndex,
			bIsReady ? 1 : 0,
			static_cast<int32>(PlayerTeam));
	}

	return Signature;
}

void ULobbyWidget::HandleLobbyPlayerListChanged()
{
	RefreshLobbyPlayerList();
}
