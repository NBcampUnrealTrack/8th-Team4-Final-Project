#include "Killcam/KillcamReplaySubsystem.h"

#include "Engine/ActorChannel.h"
#include "Engine/DemoNetConnection.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "Killcam/KillcamCameraActor.h"
#include "NetworkReplayStreaming.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "UI/Killcam/KillcamWidget.h"

bool UKillcamReplaySubsystem::IsLiveWorldActor(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const UWorld* World = Actor->GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const FLevelCollection* SourceCollection = World->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	if (!SourceCollection)
	{
		return true;
	}

	ULevel* ActorLevel = Actor->GetLevel();
	return IsValid(ActorLevel) && SourceCollection->GetLevels().Contains(TObjectPtr<ULevel>(ActorLevel));
}

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
	if (IsValid(PlaybackWidget))
	{
		PlaybackWidget->RemoveFromParent();
		PlaybackWidget = nullptr;
	}
	PlaybackElimination = FShooterEliminationEvent();

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

	DestroyPlaybackCamera();
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
	if (bPlaying || !IsValid(PlayerController)
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
	PlaybackElimination = EliminationEvent;
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

	PlayerController->SetKillcamViewTargetSuppressed(true);

	AKillcamCameraActor* PlaybackCameraActor = SpawnPlaybackCamera(KillerActor);
	if (!IsValid(PlaybackCameraActor))
	{
		return false;
	}

	PlayerController->ApplyKillcamViewTarget(PlaybackCameraActor);
	if (UClass* Class = WidgetClass.LoadSynchronous())
	{
		PlaybackWidget = CreateWidget<UKillcamWidget>(PlayerController, Class);
		if (IsValid(PlaybackWidget))
		{
			PlaybackWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			PlaybackWidget->SetReplay(PlaybackElimination, Cast<APawn>(KillerActor), Cast<APawn>(GetReplayActor(VictimActorGuid)));
			PlaybackWidget->AddToPlayerScreen(100);
		}
	}
	return true;
}

void UKillcamReplaySubsystem::StopPlayback()
{
	UWorld* World = SourceWorld.Get();
	UDemoNetDriver* PlaybackDriver = GetPlaybackDemoNetDriver();
	if (IsValid(World) && IsValid(PlaybackDriver) && PlaybackDriver->IsInTick())
	{
		World->GetTimerManager().ClearTimer(PlaybackTimerHandle);
		PlaybackTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::StopPlayback);
		return;
	}

	if (IsValid(PlaybackWidget))
	{
		PlaybackWidget->RemoveFromParent();
		PlaybackWidget = nullptr;
	}
	if (!IsValid(World))
	{
		DestroyPlaybackCamera();
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

		for (ULevel* Level : DuplicateCollection->GetLevels())
		{
			if (!IsValid(Level))
			{
				continue;
			}

			while (Level->bIsVisible)
			{
				World->RemoveFromWorld(Level);
			}
		}

		DuplicateCollection->SetDemoNetDriver(nullptr);
		DuplicateCollection->SetIsVisible(false);
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
	if (IsValid(PlayerController))
	{
		AActor* RestoreTarget = ResolveRestoreViewTarget(PlayerController);

		PlayerController->SetKillcamViewTargetSuppressed(false);
		if (IsValid(RestoreTarget))
		{
			PlayerController->SetViewTarget(RestoreTarget);
		}

		if (!PlayerController->HasAuthority())
		{
			PlayerController->ServerVerifyViewTarget();
		}

	}

	DestroyPlaybackCamera();
	SavedLiveViewTarget.Reset();
}

AKillcamCameraActor* UKillcamReplaySubsystem::SpawnPlaybackCamera(AActor* KillerActor)
{
	UWorld* World = SourceWorld.Get();
	if (!IsValid(World) || !IsValid(KillerActor))
	{
		return nullptr;
	}

	DestroyPlaybackCamera();

	UClass* ClassToSpawn = CameraClass.IsNull() ? nullptr : CameraClass.LoadSynchronous();
	if (!ClassToSpawn)
	{
		ClassToSpawn = AKillcamCameraActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FLevelCollection* SourceCollection = World->FindCollectionByType(ELevelCollectionType::DynamicSourceLevels);
	AKillcamCameraActor* SpawnedCamera = nullptr;
	if (SourceCollection)
	{
		FScopedLevelCollectionContextSwitch CollectionContext(SourceCollection, World);
		SpawnedCamera = World->SpawnActor<AKillcamCameraActor>(ClassToSpawn, FTransform::Identity, SpawnParams);
	}
	else
	{
		SpawnedCamera = World->SpawnActor<AKillcamCameraActor>(ClassToSpawn, FTransform::Identity, SpawnParams);
	}

	if (IsValid(SpawnedCamera))
	{
		SpawnedCamera->StartTracking(KillerActor);
	}

	PlaybackCamera = SpawnedCamera;
	return SpawnedCamera;
}

void UKillcamReplaySubsystem::DestroyPlaybackCamera()
{
	if (IsValid(PlaybackCamera))
	{
		PlaybackCamera->StopTracking();
		PlaybackCamera->Destroy();
	}
	PlaybackCamera = nullptr;
}

AActor* UKillcamReplaySubsystem::ResolveRestoreViewTarget(AShooterPlayerController* PlayerController) const
{
	if (!IsValid(PlayerController))
	{
		return nullptr;
	}

	AActor* PendingTarget = PlayerController->ConsumePendingViewTarget();
	if (IsLiveWorldActor(PendingTarget) && PendingTarget != PlayerController)
	{
		return PendingTarget;
	}

	APawn* CurrentPawn = PlayerController->GetPawn();
	const bool bIsSpectatorPawn = IsValid(CurrentPawn) && CurrentPawn->IsA<ASpectatorPawn>();
	if (IsValid(CurrentPawn) && !bIsSpectatorPawn)
	{
		return CurrentPawn;
	}

	AActor* SavedTarget = SavedLiveViewTarget.Get();
	if (IsLiveWorldActor(SavedTarget) && SavedTarget != PlaybackCamera.Get() && SavedTarget != PlayerController)
	{
		return SavedTarget;
	}

	if (APawn* SpectatorPawn = PlayerController->GetSpectatorPawn())
	{
		return SpectatorPawn;
	}

	return CurrentPawn;
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

	const bool bHasDuplicateCollection = World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels) != nullptr;
	return bHasDuplicateCollection;
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
