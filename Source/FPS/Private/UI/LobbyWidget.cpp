// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LobbyWidget.h"

#include "Components/Button.h"
#include "Player/ShooterPlayerController.h"
#include "Session/SessionSubSystem.h"

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
	CacheSessionSubsystem();

	if (!SessionSubsystem)
	{
		SetStatusMessage(FText::FromString(TEXT("Session subsystem is not available.")));
		OnStartRoomGameResult(false);
		return;
	}

	SetStatusMessage(FText::FromString(TEXT("Starting game...")));
	SessionSubsystem->StartRoomGame(MapName);
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

void ULobbyWidget::NativeDestruct()
{
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
