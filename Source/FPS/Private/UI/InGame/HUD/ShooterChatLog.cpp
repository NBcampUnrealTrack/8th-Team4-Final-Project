// ShooterChatLog.cpp
#include "UI/InGame/HUD/ShooterChatLog.h"

#include "Components/VerticalBox.h"
#include "Player/ShooterPlayerController.h"
#include "UI/InGame/HUD/ShooterChatEntry.h"

void UShooterChatLog::NativeConstruct()
{
	Super::NativeConstruct();

	AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>();
	if (!ensure(PlayerController))
	{
		return;
	}

	BoundPlayerController = PlayerController;
	PlayerController->OnChatMessageReceived.AddUniqueDynamic(this, &ThisClass::HandleChatMessageReceived);
}

void UShooterChatLog::NativeDestruct()
{
	if (AShooterPlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnChatMessageReceived.RemoveDynamic(this, &ThisClass::HandleChatMessageReceived);
	}
	BoundPlayerController.Reset();

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimerHandle);
	}
	ExpiryTimerHandle.Invalidate();

	ActiveEntries.Reset();

	Super::NativeDestruct();
}

void UShooterChatLog::HandleChatMessageReceived(const FChatMessage& Message)
{
	if (!ensure(EntryWidgetClass) || !ensure(Box_Entries))
	{
		return;
	}

	UShooterChatEntry* EntryWidget = CreateWidget<UShooterChatEntry>(this, EntryWidgetClass);
	if (EntryWidget == nullptr)
	{
		return;
	}

	EntryWidget->SetData(Message);
	Box_Entries->AddChildToVerticalBox(EntryWidget);
	ActiveEntries.Add(EntryWidget);

	while (ActiveEntries.Num() > MaxVisibleEntries)
	{
		RemoveOldestEntry();
	}

	// 최초 예약 등록
	if (ExpiryTimerHandle.IsValid() == false)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ExpiryTimerHandle,
			this,
			&ThisClass::RemoveOldestEntry,
			EntryLifetimeSeconds,
			false);
	}
}

void UShooterChatLog::RemoveOldestEntry()
{
	if (ActiveEntries.Num() > 0)
	{
		if (UShooterChatEntry* Oldest = ActiveEntries[0])
		{
			Oldest->RemoveFromParent();
		}
		ActiveEntries.RemoveAt(0);
	}

	// 남은 줄이 있으면 만료 예약
	ExpiryTimerHandle.Invalidate();

	if (ActiveEntries.Num() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ExpiryTimerHandle,
			this,
			&ThisClass::RemoveOldestEntry,
			EntryLifetimeSeconds,
			false);
	}
}
