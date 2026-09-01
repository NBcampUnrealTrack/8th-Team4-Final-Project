// ShooterKillFeed.cpp
#include "UI/InGame/HUD/ShooterKillFeed.h"

#include "Components/VerticalBox.h"
#include "UI/InGame/HUD/ShooterKillFeedEntry.h"

void UShooterKillFeed::NativeDestruct()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimerHandle);
	}
	ExpiryTimerHandle.Invalidate();
	
	ActiveEntries.Reset();
	
	Super::NativeDestruct();
}

void UShooterKillFeed::AddEntry(const FShooterKillFeedEntryData& Entry)
{
	if (!ensure(EntryWidgetClass) || !ensure(Box_Entries))
	{
		return;
	}

	UShooterKillFeedEntry* EntryWidget = CreateWidget<UShooterKillFeedEntry>(this, EntryWidgetClass);
	if (EntryWidget == nullptr)
	{
		return;
	}

	EntryWidget->SetData(Entry);
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

void UShooterKillFeed::RemoveOldestEntry()
{
	if (ActiveEntries.Num() > 0)
	{
		if (UShooterKillFeedEntry* Oldest = ActiveEntries[0])
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

void UShooterKillFeed::DebugAddFakeEntry()
{
	FShooterKillFeedEntryData Fake;
	Fake.KillerName = FText::FromString(TEXT("Killer"));
	Fake.VictimName = FText::FromString(TEXT("Victim"));
	Fake.KillerTeam = EGameTeam::TeamA;
	Fake.VictimTeam = EGameTeam::TeamB;
	Fake.bHeadshot = (ActiveEntries.Num() % 2) == 0;

	AddEntry(Fake);
}
