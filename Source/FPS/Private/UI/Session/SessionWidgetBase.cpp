// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Session/SessionWidgetBase.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Session/SessionSubSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSessionWidgetBase, Log, All);

void USessionWidgetBase::MenuSetup(int32 InMaxPlayers, const FString& InMapName, const FString& InMatchType)
{
	UE_LOG(LogSessionWidgetBase, Log, TEXT("MenuSetup: Widget=%s MapName=%s MatchType=%s"), *GetNameSafe(this), *InMapName, *InMatchType);

	MaxPlayers = InMaxPlayers;
	MapName = InMapName.IsEmpty() ? TEXT("BombMap") : InMapName;
	MatchType = InMatchType.IsEmpty() ? TEXT("FreeForAll") : InMatchType;

	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	CacheSessionSubsystem();
}

bool USessionWidgetBase::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	CacheSessionSubsystem();
	return true;
}

void USessionWidgetBase::NativeDestruct()
{
	MenuTearDown();
	Super::NativeDestruct();
}

void USessionWidgetBase::CacheSessionSubsystem()
{
	if (!SessionSubsystem)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			SessionSubsystem = GameInstance->GetSubsystem<USessionSubSystem>();
			UE_LOG(LogSessionWidgetBase, Log, TEXT("CacheSessionSubsystem: %s"), *GetNameSafe(SessionSubsystem));
		}
	}
}

void USessionWidgetBase::SetStatusMessage(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}

	OnMenuStatusMessage(Message);
}

void USessionWidgetBase::MenuTearDown()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			const FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}
