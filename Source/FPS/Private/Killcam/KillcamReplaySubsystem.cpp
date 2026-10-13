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
#include "GameMode/State/BaseTeamMatchGameState.h"
#include "Killcam/KillcamCameraActor.h"
#include "NetworkReplayStreaming.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "UI/Killcam/KillcamWidget.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogKillcam, Log, All);

namespace
{
	constexpr float KillcamSetupRetryInterval = 0.2f;
	constexpr float KillcamSetupRetryTimeout = 30.0f;
	constexpr float KillcamBufferSafetyMargin = 2.0f;

	// 레벨 컬렉션 가시성은 USceneComponent::IsVisible()에만 반영되고, 이미 만들어진 렌더 프록시는 그대로 남는다.
	// 그래서 라이브 월드 킬러의 3P 조준경이 킬캠 화면(킬러가 지금 서 있는 자리)에 겹쳐 보였다.
	// 움직이는 컴포넌트만 렌더 상태를 다시 만들어 현재 컬렉션 가시성을 반영한다. (정적 지형은 복제 레벨과 겹쳐 티가 안 난다)
	void RefreshCollectionRenderState(const FLevelCollection* Collection)
	{
		if (!Collection)
		{
			return;
		}
		for (const ULevel* Level : Collection->GetLevels())
		{
			if (!IsValid(Level))
			{
				continue;
			}
			for (AActor* Actor : Level->Actors)
			{
				if (!IsValid(Actor))
				{
					continue;
				}
				Actor->ForEachComponent<UPrimitiveComponent>(false, [](UPrimitiveComponent* Component)
				{
					if (Component->IsRegistered() && Component->Mobility != EComponentMobility::Static)
					{
						Component->MarkRenderStateDirty();
					}
				});
			}
		}
	}
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
	UnbindGameState();

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
	KillerWeaponGuids.Reset();
	DeathDemoTime = 0.0f;
	SetupRetryElapsed = 0.0f;
	bPlaying = false;
}

bool UKillcamReplaySubsystem::IsPlaying() const
{
	return bPlaying;
}

bool UKillcamReplaySubsystem::CanSkipPlayback() const
{
	const UWorld* World = SourceWorld.Get();
	return bPlaying && IsValid(PlaybackCamera) && IsValid(World)
		&& World->GetRealTimeSeconds() - PlaybackShownRealTime >= SkipInputGraceSeconds;
}

void UKillcamReplaySubsystem::SkipPlayback()
{
	if (CanSkipPlayback())
	{
		StopPlayback();
	}
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
	if (bPlaybackRequestPending || bPlaying || IsMatchEnded() || !IsValid(PlayerController)
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
	KillerWeaponGuids.Reset();
	for (const AActor* OwnedActor : KillerPawn->Children)
	{
		if (const AWeaponBase* Weapon = Cast<AWeaponBase>(OwnedActor))
		{
			const FNetworkGUID WeaponGuid = RecordingDriver->GetGUIDForActor(Weapon);
			if (WeaponGuid.IsValid())
			{
				KillerWeaponGuids.Add(WeaponGuid);
			}
			// TEMP: 킬러 무기가 녹화 스트림에 들어갔는지(GUID/채널) 확인용
			UNetConnection* DemoConnection = RecordingDriver->ClientConnections.IsEmpty() ? nullptr : RecordingDriver->ClientConnections[0].Get();
			UE_LOG(LogKillcam, Warning, TEXT("[KillcamDiag] Death weapon=%s hidden=%d guid=%s channel=%d remoteRole=%d level=%s"),
				*Weapon->GetName(), Weapon->IsHidden() ? 1 : 0, *WeaponGuid.ToString(),
				DemoConnection && DemoConnection->FindActorChannelRef(const_cast<AWeaponBase*>(Weapon)) ? 1 : 0,
				static_cast<int32>(Weapon->GetRemoteRole()), *GetNameSafe(Weapon->GetLevel()));
		}
	}
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

	UnbindGameState();

	BoundGameState = GameState;
	if (IsValid(BoundGameState))
	{
		BoundGameState->OnPlayerEliminated.AddUniqueDynamic(this, &ThisClass::HandlePlayerEliminated);
		if (ABaseTeamMatchGameState* MatchGameState = Cast<ABaseTeamMatchGameState>(BoundGameState))
		{
			MatchGameState->OnMatchStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchStateChanged);
		}
	}
}

void UKillcamReplaySubsystem::UnbindGameState()
{
	if (!IsValid(BoundGameState))
	{
		return;
	}

	BoundGameState->OnPlayerEliminated.RemoveDynamic(this, &ThisClass::HandlePlayerEliminated);
	if (ABaseTeamMatchGameState* MatchGameState = Cast<ABaseTeamMatchGameState>(BoundGameState))
	{
		MatchGameState->OnMatchStateChanged.RemoveDynamic(this, &ThisClass::HandleMatchStateChanged);
	}
}

bool UKillcamReplaySubsystem::IsMatchEnded() const
{
	const ABaseTeamMatchGameState* MatchGameState = Cast<ABaseTeamMatchGameState>(BoundGameState);
	return IsValid(MatchGameState) && MatchGameState->GetMatchWinner() != EGameTeam::None;
}

void UKillcamReplaySubsystem::HandleMatchStateChanged()
{
	if (IsMatchEnded() && (bPlaying || bPlaybackRequestPending))
	{
		CancelPlayback(false);
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
	// 0.5ms는 액터 50개 우선순위 정렬만으로 초과돼(로그 0.69ms) 복제 단계가 매 프레임 굶었다.
	// 그 결과 무기 같은 저우선순위 채널이 녹화 시작 후 수십 초간 안 열려 킬캠에서 총이 사라졌다. (녹화는 8Hz)
	RecordingDriver->SetMaxDesiredRecordTimeMS(2.0f);
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
	if (IsMatchEnded())
	{
		CancelPlayback(false);
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
	// CurrentWeapon이 가리키는 무기 채널이 큐에 남으면 시킹 후에도 수 초간 None으로 남는다.
	for (const FNetworkGUID& WeaponGuid : KillerWeaponGuids)
	{
		PlaybackDriver->AddNonQueuedGUIDForScrubbing(WeaponGuid);
	}
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
		PlaybackShownRealTime = World->GetRealTimeSeconds();
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
	RefreshCollectionRenderState(SourceCollection);

	// TEMP: 킬러 무기가 리플레이에 언제 생기는지 확인용
	for (const FNetworkGUID& WeaponGuid : KillerWeaponGuids)
	{
		UE_LOG(LogKillcam, Warning, TEXT("[KillcamDiag] Playback weapon guid=%s actor=%s"),
			*WeaponGuid.ToString(), *GetNameSafe(GetReplayActor(WeaponGuid)));
	}

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
		RefreshCollectionRenderState(SourceCollection);
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
