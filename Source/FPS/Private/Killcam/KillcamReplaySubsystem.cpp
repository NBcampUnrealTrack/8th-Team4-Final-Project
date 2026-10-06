#include "Killcam/KillcamReplaySubsystem.h"

#include "Engine/ActorChannel.h"
#include "Character/ShooterCharacter.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogKillcam, Log, All);

namespace
{
	constexpr float KillcamSetupRetryInterval = 0.2f;
	constexpr float KillcamSetupRetryTimeout = 30.0f;
	constexpr float KillcamBufferSafetyMargin = 2.0f;
}

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

	RetrySetup();
}

void UKillcamReplaySubsystem::RetrySetup()
{
	UWorld* World = SourceWorld.Get();
	if (!IsValid(World))
	{
		return;
	}

	BindGameState();
	StartRecording();

	FTimerManager& TimerManager = World->GetTimerManager();
	if (IsSetUpComplete())
	{
		TimerManager.ClearTimer(SetupRetryTimerHandle);
		SetupRetryElapsed = 0.0f;
		return;
	}

	if (!IsValid(BoundGameState) && IsValid(World->GetGameState()))
	{
		TimerManager.ClearTimer(SetupRetryTimerHandle);
		SetupRetryElapsed = 0.0f;
		return;
	}

	if (SetupRetryElapsed >= KillcamSetupRetryTimeout)
	{
		TimerManager.ClearTimer(SetupRetryTimerHandle);
		UE_LOG(LogKillcam, Warning,
			TEXT("Killcam setup gave up after %.0fs. GameState=%s Recording=%d"),
			KillcamSetupRetryTimeout,
			*GetNameSafe(BoundGameState),
			IsValid(World->GetDemoNetDriver()) && World->GetDemoNetDriver()->IsRecording());
		return;
	}

	if (!TimerManager.IsTimerActive(SetupRetryTimerHandle))
	{
		SetupRetryElapsed = 0.0f;
		TimerManager.SetTimer(
			SetupRetryTimerHandle,
			this,
			&ThisClass::RetrySetup,
			KillcamSetupRetryInterval,
			true);
		return;
	}

	SetupRetryElapsed += KillcamSetupRetryInterval;
}

bool UKillcamReplaySubsystem::IsSetUpComplete() const
{
	if (!IsValid(BoundGameState))
	{
		return false;
	}

	const UWorld* World = SourceWorld.Get();
	const UDemoNetDriver* RecordingDriver = IsValid(World) ? World->GetDemoNetDriver() : nullptr;
	return IsValid(RecordingDriver) && RecordingDriver->IsRecording();
}

float UKillcamReplaySubsystem::GetRequiredBufferSeconds() const
{
	const float MinimumSeconds = PlaybackDuration + PostDeathRecordDuration + KillcamBufferSafetyMargin;
	return FMath::Max(ReplayBufferSeconds, MinimumSeconds);
}

void UKillcamReplaySubsystem::ReleaseWorld(UWorld* World)
{
	if (!World || SourceWorld.Get() != World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(PostDeathTimerHandle);
	World->GetTimerManager().ClearTimer(PlaybackTimerHandle);
	World->GetTimerManager().ClearTimer(SetupRetryTimerHandle);
	CancelPlayback(false);

	if (IsValid(BoundGameState))
	{
		BoundGameState->OnPlayerEliminated.RemoveDynamic(this, &ThisClass::HandlePlayerEliminated);
	}

	UDemoNetDriver* RecordingDriver = World->GetDemoNetDriver();
	if (IsValid(RecordingDriver) && RecordingDriver->IsRecording())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			GameInstance->StopRecordingReplay();
		}
	}

	DestroyPlaybackCamera();
	BoundGameState = nullptr;
	SourceWorld.Reset();
	SavedLiveViewTarget.Reset();
	KillerActorGuid = FNetworkGUID();
	VictimActorGuid = FNetworkGUID();
	DeathDemoTime = 0.0f;
	SetupRetryElapsed = 0.0f;
	bPlaying = false;
}

bool UKillcamReplaySubsystem::IsPlaying() const
{
	return bPlaying;
}

void UKillcamReplaySubsystem::CancelForRespawn()
{
	CancelPlayback(true);
}

void UKillcamReplaySubsystem::HandleOwnedPawnChanged(APawn* NewPawn)
{
	AShooterPlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController) || PlayerController->GetPawn() != NewPawn
		|| !IsLiveWorldActor(NewPawn) || !NewPawn->IsA<AShooterCharacter>())
	{
		return;
	}

	// The new owned avatar is a life boundary even if the previous death flag is still replicated.
	if (bPlaybackRequestPending && NewPawn != VictimPawnAtElimination.Get())
	{
		CancelPlayback(true);
	}
}

bool UKillcamReplaySubsystem::IsCurrentRequest(const uint64 Generation,
	const TWeakObjectPtr<UWorld> RequestWorld, const TWeakObjectPtr<APawn> VictimPawn) const
{
	return bPlaybackRequestPending && Generation == PlaybackGeneration
		&& RequestWorld.IsValid() && RequestWorld == SourceWorld && VictimPawn == VictimPawnAtElimination;
}

bool UKillcamReplaySubsystem::CanPlayForCurrentLife() const
{
	const AShooterPlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController)
		|| PlayerController->GetPlayerState<AShooterPlayerState>() != PlaybackElimination.VictimPlayerState)
	{
		return false;
	}
	if (const AShooterCharacter* VictimPawn = Cast<AShooterCharacter>(VictimPawnAtElimination.Get()))
	{
		if (VictimPawn->GetPawnLifeId() != VictimPawnLifeIdAtElimination)
		{
			return false;
		}
	}

	const APawn* OwnedPawn = PlayerController->GetPawn();
	return !IsLiveWorldActor(OwnedPawn) || !OwnedPawn->IsA<AShooterCharacter>()
		|| OwnedPawn == VictimPawnAtElimination.Get();
}

void UKillcamReplaySubsystem::HandlePlayerEliminated(const FShooterEliminationEvent& EliminationEvent)
{
	AShooterPlayerController* PlayerController = GetPlayerController();
	if (bPlaybackRequestPending || bPlaying || !IsValid(PlayerController)
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
	AShooterCharacter* VictimPawn = Cast<AShooterCharacter>(EliminationEvent.VictimPlayerState->GetPawn());
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
	if (!EliminationEvent.VictimPawnLifeId.IsValid()
		|| VictimPawn->GetPawnLifeId() != EliminationEvent.VictimPawnLifeId)
	{
		return;
	}
	APawn* OwnedPawn = PlayerController->GetPawn();
	if (!IsLiveWorldActor(VictimPawn)
		|| (IsLiveWorldActor(OwnedPawn) && OwnedPawn->IsA<AShooterCharacter>() && OwnedPawn != VictimPawn))
	{
		// A delayed elimination belongs to its original avatar, never the newly possessed one.
		return;
	}
	VictimPawnAtElimination = VictimPawn;
	VictimPawnLifeIdAtElimination = EliminationEvent.VictimPawnLifeId;
	bPlaybackRequestPending = true;
	const uint64 Generation = ++PlaybackGeneration;

	World->GetTimerManager().SetTimer(
		PostDeathTimerHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::StartPlayback,
			Generation, SourceWorld, VictimPawnAtElimination),
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
	if (!IsValid(GameInstance) || bPlaying || IsValid(GetPlaybackDemoNetDriver())
		|| (PlaybackCleanupWorld.Get() == World
			&& World->GetTimerManager().IsTimerActive(PlaybackCleanupTimerHandle)))
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
		ReplayStreamer->SetTimeBufferHintSeconds(GetRequiredBufferSeconds());
	}
}

void UKillcamReplaySubsystem::StartPlayback(const uint64 Generation,
	const TWeakObjectPtr<UWorld> RequestWorld, const TWeakObjectPtr<APawn> VictimPawn)
{
	if (!IsCurrentRequest(Generation, RequestWorld, VictimPawn))
	{
		return;
	}
	if (!CanPlayForCurrentLife())
	{
		CancelPlayback(true);
		return;
	}
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
		CancelPlayback(true);
		return;
	}

	AShooterPlayerController* PlayerController = GetPlayerController();
	if (!IsValid(PlayerController))
	{
		CancelPlayback(true);
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
		CancelPlayback(true);
		return;
	}

	UDemoNetDriver* PlaybackDriver = World->GetDemoNetDriver();
	if (!IsValid(PlaybackDriver))
	{
		DuplicateCollection->SetIsVisible(false);
		CancelPlayback(true);
		return;
	}

	bPlaying = true;
	DuplicateCollection->SetDemoNetDriver(PlaybackDriver);
	PlaybackDriver->AddNonQueuedGUIDForScrubbing(KillerActorGuid);
	PlaybackDriver->AddNonQueuedGUIDForScrubbing(VictimActorGuid);
	const TWeakObjectPtr<UDemoNetDriver> WeakPlaybackDriver = PlaybackDriver;
	// Seeking also has a deadline; an unfinished seek cannot outlive this death indefinitely.
	World->GetTimerManager().SetTimer(PlaybackTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, Generation, RequestWorld, VictimPawn, WeakPlaybackDriver]()
		{
			if (IsCurrentRequest(Generation, RequestWorld, VictimPawn)
				&& GetPlaybackDemoNetDriver() == WeakPlaybackDriver.Get())
			{
				StopPlayback();
			}
		}), GetRequiredBufferSeconds(), false);
	PlaybackDriver->GotoTimeInSeconds(
		PlaybackStartTime,
		FOnGotoTimeDelegate::CreateUObject(this, &ThisClass::HandlePlaybackReady,
			Generation, RequestWorld, VictimPawn, WeakPlaybackDriver));
}

void UKillcamReplaySubsystem::HandlePlaybackReady(const bool bWasSuccessful, const uint64 Generation,
	const TWeakObjectPtr<UWorld> RequestWorld, const TWeakObjectPtr<APawn> VictimPawn,
	const TWeakObjectPtr<UDemoNetDriver> PlaybackDriver)
{
	// Check before both success and failure: an old callback must never stop a newer replay.
	if (!IsCurrentRequest(Generation, RequestWorld, VictimPawn) || !PlaybackDriver.IsValid()
		|| GetPlaybackDemoNetDriver() != PlaybackDriver.Get())
	{
		return;
	}
	if (!bWasSuccessful || !CanPlayForCurrentLife())
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
			FTimerDelegate::CreateWeakLambda(this, [this, Generation, RequestWorld, VictimPawn, PlaybackDriver]()
			{
				if (IsCurrentRequest(Generation, RequestWorld, VictimPawn)
					&& GetPlaybackDemoNetDriver() == PlaybackDriver.Get())
				{
					StopPlayback();
				}
			}),
			PlaybackDuration,
			false);
	}
}

bool UKillcamReplaySubsystem::ShowPlayback()
{
	UWorld* World = SourceWorld.Get();
	if (!IsValid(World) || !bPlaybackRequestPending || !CanPlayForCurrentLife())
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
	CancelPlayback(true);
}

void UKillcamReplaySubsystem::CancelPlayback(const bool bRestartRecording)
{
	UWorld* World = SourceWorld.Get();
	UDemoNetDriver* PlaybackDriver = GetPlaybackDemoNetDriver();
	const uint64 Generation = ++PlaybackGeneration;
	bPlaybackRequestPending = false;
	const bool bHadPlayback = bPlaying || IsValid(PlaybackCamera);
	bPlaying = false;
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(PostDeathTimerHandle);
		World->GetTimerManager().ClearTimer(PlaybackTimerHandle);
	}
	if (IsValid(PlaybackWidget))
	{
		PlaybackWidget->RemoveFromParent();
		PlaybackWidget = nullptr;
	}
	if (bHadPlayback && IsValid(World))
	{
		RestoreLiveWorld();
	}
	else
	{
		DestroyPlaybackCamera();
	}
	PlaybackElimination = FShooterEliminationEvent();
	VictimPawnAtElimination.Reset();
	VictimPawnLifeIdAtElimination.Invalidate();
	SavedLiveViewTarget.Reset();

	if (IsValid(World) && IsValid(PlaybackDriver))
	{
		FinishPlaybackCleanup(World, PlaybackDriver, Generation, bRestartRecording);
	}
	else if (bRestartRecording && IsValid(World))
	{
		// Cancelling the post-death delay keeps an existing recording running.
		RetrySetup();
	}
}

void UKillcamReplaySubsystem::FinishPlaybackCleanup(const TWeakObjectPtr<UWorld> RequestWorld,
	const TWeakObjectPtr<UDemoNetDriver> PlaybackDriver, const uint64 Generation, const bool bRestartRecording)
{
	UWorld* World = RequestWorld.Get();
	UDemoNetDriver* Driver = PlaybackDriver.Get();
	FLevelCollection* DuplicateCollection = IsValid(World)
		? World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels) : nullptr;
	// The world and exact driver are captured. Never tear down a replacement replay driver.
	if (!DuplicateCollection || !IsValid(Driver) || DuplicateCollection->GetDemoNetDriver() != Driver)
	{
		return;
	}
	if (Driver->IsInTick())
	{
		if (UWorld* CleanupWorld = PlaybackCleanupWorld.Get())
		{
			CleanupWorld->GetTimerManager().ClearTimer(PlaybackCleanupTimerHandle);
		}
		PlaybackCleanupWorld = World;
		PlaybackCleanupTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::FinishPlaybackCleanup,
				RequestWorld, PlaybackDriver, Generation, bRestartRecording));
		return;
	}

	CleanPlaybackWorld(World, Driver);
	if (Generation == PlaybackGeneration && RequestWorld == SourceWorld)
	{
		PlaybackCleanupWorld.Reset();
		PlaybackCleanupTimerHandle.Invalidate();
		if (bRestartRecording)
		{
			RetrySetup();
		}
	}
}

void UKillcamReplaySubsystem::CleanPlaybackWorld(UWorld* World, UDemoNetDriver* ExpectedPlaybackDriver)
{
	FLevelCollection* DuplicateCollection = nullptr;
	if (IsValid(World))
	{
		DuplicateCollection = World->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
	}
	if (!IsValid(World) || !DuplicateCollection || !IsValid(ExpectedPlaybackDriver)
		|| DuplicateCollection->GetDemoNetDriver() != ExpectedPlaybackDriver)
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

	auto RestoreView = [this]()
	{
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
	};
	if (SourceCollection)
	{
		// A failed seek may call back while the duplicate collection is active.
		FScopedLevelCollectionContextSwitch CollectionContext(SourceCollection, World);
		RestoreView();
	}
	else
	{
		RestoreView();
	}
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
	APawn* CurrentPawn = PlayerController->GetPawn();
	const bool bHasFreshCombatPawn = IsLiveWorldActor(CurrentPawn) && CurrentPawn->IsA<AShooterCharacter>()
		&& CurrentPawn != VictimPawnAtElimination.Get();
	if (bHasFreshCombatPawn)
	{
		// Pending may still describe an old spectator request. Ownership of a new avatar wins.
		return CurrentPawn;
	}
	if (IsLiveWorldActor(PendingTarget) && PendingTarget != PlayerController
		&& PendingTarget != VictimPawnAtElimination.Get())
	{
		return PendingTarget;
	}

	AActor* SavedTarget = SavedLiveViewTarget.Get();
	if (IsLiveWorldActor(SavedTarget) && SavedTarget != PlaybackCamera.Get() && SavedTarget != PlayerController
		&& SavedTarget != VictimPawnAtElimination.Get())
	{
		return SavedTarget;
	}

	if (APawn* SpectatorPawn = PlayerController->GetSpectatorPawn())
	{
		return SpectatorPawn;
	}

	return IsLiveWorldActor(CurrentPawn) && CurrentPawn != VictimPawnAtElimination.Get()
		? static_cast<AActor*>(CurrentPawn) : PlayerController;
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
