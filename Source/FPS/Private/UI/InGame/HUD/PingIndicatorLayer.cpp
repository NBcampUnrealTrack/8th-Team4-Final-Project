// PingIndicatorLayer.cpp
#include "UI/InGame/HUD/PingIndicatorLayer.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Player/ShooterPlayerController.h"
#include "UI/InGame/HUD/PingIndicatorEntry.h"

void UPingIndicatorLayer::NativeConstruct()
{
	Super::NativeConstruct();

	if (AShooterPlayerController* PC = GetOwningPlayer<AShooterPlayerController>())
	{
		BoundPlayerController = PC;
		PC->OnPingReceived.AddUniqueDynamic(this, &ThisClass::HandlePingReceived);
	}
}

void UPingIndicatorLayer::NativeDestruct()
{
	if (BoundPlayerController.IsValid())
	{
		BoundPlayerController->OnPingReceived.RemoveAll(this);
	}
	BoundPlayerController.Reset();

	Super::NativeDestruct();
}

void UPingIndicatorLayer::HandlePingReceived(const FPingEvent& Ping)
{
	if (UPingIndicatorEntry* Entry = GetFreeEntry())
	{
		Entry->Activate(Ping);
	}
}

UPingIndicatorEntry* UPingIndicatorLayer::GetFreeEntry()
{
	for (UPingIndicatorEntry* Entry : Entries)
	{
		if (Entry && !Entry->IsActive())
		{
			return Entry;
		}
	}

	if (!ensure(EntryWidgetClass) || Box_Indicators == nullptr)
	{
		return nullptr;
	}

	UPingIndicatorEntry* NewEntry = CreateWidget<UPingIndicatorEntry>(this, EntryWidgetClass);
	if (NewEntry == nullptr)
	{
		return nullptr;
	}

	UCanvasPanelSlot* NewSlot = Box_Indicators->AddChildToCanvas(NewEntry);
	if (NewSlot == nullptr)
	{
		return nullptr;
	}

	// 1. 앵커 기준은 좌상단 한 점
	NewSlot->SetAnchors(FAnchors(0.f, 0.f));
	// 2. 투영된 지점이 아이콘 정중앙에 오게
	NewSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	NewSlot->SetAutoSize(true);

	NewEntry->InitializeIndicator(Box_Indicators, NewSlot);

	Entries.Add(NewEntry);

	return NewEntry;
}
