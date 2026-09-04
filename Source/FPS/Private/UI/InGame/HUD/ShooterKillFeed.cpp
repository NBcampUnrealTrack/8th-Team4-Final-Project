// ShooterKillFeed.cpp
#include "UI/InGame/HUD/ShooterKillFeed.h"

#include "Components/VerticalBox.h"
#include "Player/ShooterPlayerState.h"
#include "UI/InGame/HUD/ShooterKillFeedEntry.h"

void UShooterKillFeed::NativeConstruct()
{
	Super::NativeConstruct();

	ABaseTeamGameState* TeamGameState = GetWorld() ? GetWorld()->GetGameState<ABaseTeamGameState>() : nullptr;
	if (!ensure(TeamGameState))
	{
		return;
	}
	
	BoundGameState = TeamGameState;
	TeamGameState->OnPlayerEliminated.AddUniqueDynamic(this, &ThisClass::HandlePlayerEliminated);
}

void UShooterKillFeed::NativeDestruct()
{
	if (ABaseTeamGameState* TeamGameState = BoundGameState.Get())
	{
		TeamGameState->OnPlayerEliminated.RemoveDynamic(this, &ThisClass::HandlePlayerEliminated);
	}
	BoundGameState.Reset();
	
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

void UShooterKillFeed::HandlePlayerEliminated(const FShooterEliminationEvent& EliminationEvent)
{
	const AShooterPlayerState* Victim = EliminationEvent.VictimPlayerState;
	if (!IsValid(Victim))
	{
		return;
	}
	
	const AShooterPlayerState* Killer = EliminationEvent.KillerPlayerState;
	
	FShooterKillFeedEntryData Data;
	
	// PlayerState의 수명이 5초보다 짧을 수 있으므로 포인터를 들지않음
	Data.VictimName = FText::FromString(Victim->GetPlayerName());
	Data.VictimTeam = Victim->GetTeam();
	Data.WeaponTag = EliminationEvent.WeaponTag;
	Data.bHeadshot = EliminationEvent.bHeadshot;
	
	if (!IsValid(Killer))
	{
		Data.EventType = EShooterKillFeedEventType::Environment;
	}
	else if (Killer == Victim)
	{
		Data.EventType = EShooterKillFeedEventType::SelfElimination;
		Data.KillerName = Data.VictimName;
		Data.KillerTeam = Data.VictimTeam;
	}
	else
	{
		Data.EventType = EShooterKillFeedEventType::Elimination;
		Data.KillerName = FText::FromString(Killer->GetPlayerName());
		Data.KillerTeam = Killer->GetTeam();
	}

	AddEntry(Data);
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
