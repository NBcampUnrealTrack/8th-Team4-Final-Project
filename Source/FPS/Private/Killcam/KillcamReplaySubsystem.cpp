#include "Killcam/KillcamReplaySubsystem.h"

#include "Engine/ActorChannel.h"
#include "Engine/DemoNetConnection.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "NetworkReplayStreaming.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"

void UKillcamReplaySubsystem::Deinitialize()
{
	ReleaseWorld(SourceWorld.Get());
	Super::Deinitialize();
}

void UKillcamReplaySubsystem::SetUpForWorld(UWorld* World)
{
	if (!IsSupportedWorld(World))
	{
		return;
	}

	if (SourceWorld.Get() != World)
	{
		ReleaseWorld(SourceWorld.Get());
		SourceWorld = World;
	}

	BindGameState();
	StartRecording();
}

void UKillcamReplaySubsystem::ReleaseWorld(UWorld* World)
{
	if (!World || SourceWorld.Get() != World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(PostDeathTimerHandle);
	World->GetTimerManager().ClearTimer(PlaybackTimerHandle);

	if (IsValid(BoundGameState))
	{
		BoundGameState->OnPlayerEliminated.RemoveDynamic(this, &ThisClass::HandlePlayerEliminated);
	}

	if (bPlaying)
	{
		RestoreLiveWorld();
		CleanPlaybackWorld();
	}
	else if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		GameInstance->StopRecordingReplay();
	}

	BoundGameState = nullptr;
	SourceWorld.Reset();
	SavedLiveViewTarget.Reset();
	KillerActorGuid = FNetworkGUID();
	VictimActorGuid = FNetworkGUID();
	DeathDemoTime = 0.0f;
	bPlaying = false;
}

bool UKillcamReplaySubsystem::IsPlaying() const
{
	return bPlaying;
}

void UKillcamReplaySubsystem::HandlePlayerEliminated(const FShooterEliminationEvent& EliminationEvent)
{
	AShooterPlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController)
		|| PlayerController->GetPlayerState<AShooterPlayerState>() != EliminationEvent.VictimPlayerState
		|| !IsValid(EliminationEvent.KillerPlayerState)
		|| EliminationEvent.KillerPlayerState == EliminationEvent.VictimPlayerState)
	{
		return;
	}

	UWorld* World = SourceWorld.Get();
	UDemoNetDriver* RecordingDriver = nullptr;
	if (IsValid(World))
	{
		RecordingDriver = World->GetDemoNetDriver();
	}
	APawn* KillerPawn = EliminationEvent.KillerPlayerState->GetPawn();
	APawn* VictimPawn = EliminationEvent.VictimPlayerState->GetPawn();
	if (!IsValid(RecordingDriver) || !RecordingDriver->IsRecording() || !IsValid(KillerPawn) || !IsValid(VictimPawn))
	{
		return;
	}

	KillerActorGuid = RecordingDriver->GetGUIDForActor(KillerPawn);
	VictimActorGuid = RecordingDriver->GetGUIDForActor(VictimPawn);
	DeathDemoTime = RecordingDriver->GetDemoCurrentTime();
	if (!KillerActorGuid.IsValid() || !VictimActorGuid.IsValid())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PostDeathTimerHandle,
		this,
		&ThisClass::StartPlayback,
		PostDeathRecordDuration,
		false);
}

void UKillcamReplaySubsystem::BindGameState()
{
	UWorld* World = SourceWorld.Get();
	ABaseTeamGameState* GameState = nullptr;
	if (IsValid(World))
	{
		GameState = World->GetGameState<ABaseTeamGameState>();
	}
	if (BoundGameState == GameState)
	{
		return;
	}

	if (IsValid(BoundGameState))
	{
		BoundGameState->OnPlayerEliminated.RemoveDynamic(this, &ThisClass::HandlePlayerEliminated);
	}

	BoundGameState = GameState;
	if (IsValid(BoundGameState))
	{
		BoundGameState->OnPlayerEliminated.AddUniqueDynamic(this, &ThisClass::HandlePlayerEliminated);
	}
}

void UKillcamReplaySubsystem::StartRecording()
{
	UWorld* World = SourceWorld.Get();
	UGameInstance* GameInstance = nullptr;
	if (IsValid(World))
	{
		GameInstance = World->GetGameInstance();
	}
	if (!IsValid(GameInstance) || bPlaying)
	{
		return;
	}

	UDemoNetDriver* ExistingDriver = World->GetDemoNetDriver();
	if (IsValid(ExistingDriver) && ExistingDriver->IsRecording())
	{
		return;
	}

	TArray<FString> Options;
	Options.Add(TEXT("ReplayStreamerOverride=InMemoryNetworkReplayStreaming"));
	Options.Add(TEXT("SkipSpawnSpectatorController"));
	GameInstance->StartRecordingReplay(TEXT("_Killcam"), TEXT("_Killcam"), Options);

	UDemoNetDriver* RecordingDriver = World->GetDemoNetDriver();
	if (!IsValid(RecordingDriver))
	{
		return;
	}

	FLevelCollection& SourceCollection = World->FindOrAddCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	SourceCollection.SetDemoNetDriver(RecordingDriver);
	RecordingDriver->SetMaxDesiredRecordTimeMS(0.5f);
	RecordingDriver->SetActorPrioritizationEnabled(true);
	RecordingDriver->SetViewerOverride(GetPlayerController());
	RecordingDriver->SetCheckpointSaveMaxMSPerFrame(2.0f);

	TSharedPtr<INetworkReplayStreamer> ReplayStreamer = RecordingDriver->GetReplayStreamer();
	if (ReplayStreamer.IsValid())
	{
		ReplayStreamer->SetTimeBufferHintSeconds(6.0f);
	}
}

void UKillcamReplaySubsystem::StartPlayback()
{
	UWorld* World = SourceWorld.Get();
	UGameInstance* GameInstance = nullptr;
	FLevelCollection* DuplicateCollection = nullptr;
	UDemoNetDriver* RecordingDriver = nullptr;
	if (IsValid(World))
	{
		GameInstance = World->GetGameInstance();
		DuplicateCollection = World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
		RecordingDriver = World->GetDemoNetDriver();
	}
	if (!IsValid(GameInstance) || !DuplicateCollection || !IsValid(RecordingDriver) || !RecordingDriver->IsRecording())
	{
		return;
	}

	AShooterPlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController))
	{
		return;
	}

	SavedLiveViewTarget = PlayerController->GetViewTarget();
	const float PlaybackStartTime = FMath::Max(0.0f, DeathDemoTime - (PlaybackDuration - PostDeathRecordDuration));
	GameInstance->StopRecordingReplay();

	DuplicateCollection->SetIsVisible(true);
	for (ULevel* Level : DuplicateCollection->GetLevels())
	{
		if (IsValid(Level) && !Level->bIsVisible)
		{
			World->AddToWorld(Level);
		}
	}

	TArray<FString> Options;
	Options.Add(TEXT("ReplayStreamerOverride=InMemoryNetworkReplayStreaming"));
	Options.Add(TEXT("AsyncLoadWorldOverride=0"));
	Options.Add(TEXT("LevelPrefixOverride=1"));
	if (!GameInstance->PlayReplay(TEXT("_Killcam"), nullptr, Options))
	{
		DuplicateCollection->SetIsVisible(false);
		StartRecording();
		return;
	}

	UDemoNetDriver* PlaybackDriver = World->GetDemoNetDriver();
	if (!IsValid(PlaybackDriver))
	{
		StartRecording();
		return;
	}

	bPlaying = true;
	DuplicateCollection->SetDemoNetDriver(PlaybackDriver);
	PlaybackDriver->AddNonQueuedGUIDForScrubbing(KillerActorGuid);
	PlaybackDriver->AddNonQueuedGUIDForScrubbing(VictimActorGuid);
	PlaybackDriver->GotoTimeInSeconds(
		PlaybackStartTime,
		FOnGotoTimeDelegate::CreateUObject(this, &ThisClass::HandlePlaybackReady));
}

void UKillcamReplaySubsystem::HandlePlaybackReady(const bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		StopPlayback();
		return;
	}

	if (!ShowPlayback())
	{
		StopPlayback();
		return;
	}

	if (UWorld* World = SourceWorld.Get())
	{
		World->GetTimerManager().SetTimer(
			PlaybackTimerHandle,
			this,
			&ThisClass::StopPlayback,
			PlaybackDuration,
			false);
	}
}

bool UKillcamReplaySubsystem::ShowPlayback()
{
	UWorld* World = SourceWorld.Get();
	if (!IsValid(World))
	{
		return false;
	}

	FLevelCollection* SourceCollection = World->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	FLevelCollection* DuplicateCollection = World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	AShooterPlayerController* PlayerController = GetPlayerController();
	AActor* KillerActor = GetReplayActor(KillerActorGuid);
	if (!SourceCollection || !DuplicateCollection || !IsValid(PlayerController) || !IsValid(KillerActor))
	{
		return false;
	}

	SourceCollection->SetIsVisible(false);
	DuplicateCollection->SetIsVisible(true);
	PlayerController->SetViewTarget(KillerActor);
	return true;
}

void UKillcamReplaySubsystem::StopPlayback()
{
	UWorld* World = SourceWorld.Get();
	if (!IsValid(World))
	{
		bPlaying = false;
		return;
	}

	World->GetTimerManager().ClearTimer(PlaybackTimerHandle);
	RestoreLiveWorld();
	CleanPlaybackWorld();
	bPlaying = false;
	StartRecording();
}

void UKillcamReplaySubsystem::CleanPlaybackWorld()
{
	UWorld* World = SourceWorld.Get();
	FLevelCollection* DuplicateCollection = nullptr;
	if (IsValid(World))
	{
		DuplicateCollection = World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	}
	if (!IsValid(World) || !DuplicateCollection)
	{
		return;
	}

	{
		FScopedLevelCollectionContextSwitch CollectionContext(DuplicateCollection, World);
		UDemoNetDriver* PlaybackDriver = World->GetDemoNetDriver();
		if (IsValid(PlaybackDriver) && IsValid(PlaybackDriver->ServerConnection))
		{
			for (UChannel* Channel : PlaybackDriver->ServerConnection->OpenChannels)
			{
				UActorChannel* ActorChannel = Cast<UActorChannel>(Channel);
				if (IsValid(ActorChannel) && IsValid(ActorChannel->Actor) && ActorChannel->Actor->IsNetStartupActor())
				{
					ActorChannel->Actor = nullptr;
				}
			}
		}

		for (ULevel* Level : DuplicateCollection->GetLevels())
		{
			if (!IsValid(Level))
			{
				continue;
			}

			for (int32 ActorIndex = Level->Actors.Num() - 1; ActorIndex >= 0; --ActorIndex)
			{
				AActor* Actor = Level->Actors[ActorIndex];
				if (IsValid(Actor) && !Actor->IsNetStartupActor())
				{
					World->DestroyActor(Actor, true);
				}
			}
		}

		World->DestroyDemoNetDriver();
		DuplicateCollection->SetDemoNetDriver(nullptr);
	}

	DuplicateCollection->SetIsVisible(false);
	for (ULevel* Level : DuplicateCollection->GetLevels())
	{
		if (IsValid(Level) && Level->bIsVisible)
		{
			World->RemoveFromWorld(Level);
		}
	}
}

void UKillcamReplaySubsystem::RestoreLiveWorld()
{
	UWorld* World = SourceWorld.Get();
	if (!IsValid(World))
	{
		return;
	}

	FLevelCollection* SourceCollection = World->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	FLevelCollection* DuplicateCollection = World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	if (SourceCollection)
	{
		SourceCollection->SetIsVisible(true);
	}
	if (DuplicateCollection)
	{
		DuplicateCollection->SetIsVisible(false);
	}

	AShooterPlayerController* PlayerController = GetPlayerController();
	AActor* LiveViewTarget = SavedLiveViewTarget.Get();
	if (IsValid(PlayerController) && IsValid(LiveViewTarget))
	{
		PlayerController->SetViewTarget(LiveViewTarget);
	}
	SavedLiveViewTarget.Reset();
}

bool UKillcamReplaySubsystem::IsSupportedWorld(const UWorld* World) const
{
	if (!IsValid(World) || !World->IsGameWorld() || World->IsPlayingReplay() || World->GetNetMode() != NM_Client)
	{
		return false;
	}

	const FWorldContext* WorldContext = nullptr;
	if (GEngine)
	{
		WorldContext = GEngine->GetWorldContextFromWorld(World);
	}
	if (!WorldContext || WorldContext->WorldType == EWorldType::PIE)
	{
		return false;
	}

	return World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels) != nullptr;
}

AShooterPlayerController* UKillcamReplaySubsystem::GetPlayerController() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
	{
		return nullptr;
	}

	return Cast<AShooterPlayerController>(LocalPlayer->GetPlayerController(SourceWorld.Get()));
}

UDemoNetDriver* UKillcamReplaySubsystem::GetPlaybackDemoNetDriver() const
{
	UWorld* World = SourceWorld.Get();
	if (!IsValid(World))
	{
		return nullptr;
	}

	FLevelCollection* DuplicateCollection = World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	if (!DuplicateCollection)
	{
		return nullptr;
	}

	return DuplicateCollection->GetDemoNetDriver();
}

AActor* UKillcamReplaySubsystem::GetReplayActor(const FNetworkGUID& ActorGuid) const
{
	UDemoNetDriver* PlaybackDriver = GetPlaybackDemoNetDriver();
	if (!IsValid(PlaybackDriver) || !ActorGuid.IsValid())
	{
		return nullptr;
	}

	return PlaybackDriver->GetActorForGUID(ActorGuid);
}
