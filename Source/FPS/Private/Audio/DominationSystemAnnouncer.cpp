#include "Audio/DominationSystemAnnouncer.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/State/DominationGameState.h"
#include "GameMode/Settings/DominationSandstormSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Player/BaseTeamPlayerState.h"
#include "Settings/ShooterAudioSettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DominationSystemAnnouncer"

ADominationSystemAnnouncer::ADominationSystemAnnouncer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;

	FSystemAnnouncerCue ActivatedCue;
	ActivatedCue.Priority = 10;
	Announcements.Add(DominationAnnouncementIds::ObjectiveActivated, ActivatedCue);

	FSystemAnnouncerCue StartedCue;
	StartedCue.Priority = 20;
	Announcements.Add(DominationAnnouncementIds::CaptureStarted, StartedCue);

	FSystemAnnouncerCue CompletedCue;
	CompletedCue.Priority = 100;
	CompletedCue.bInterruptLowerPriority = true;
	Announcements.Add(DominationAnnouncementIds::CaptureCompleted, CompletedCue);

	FSystemAnnouncerCue SandstormCue;
	SandstormCue.Priority = 70;
	SandstormCue.bInterruptLowerPriority = true;
	SandstormCue.MaxQueueDelaySeconds = 1.0f;
	Announcements.Add(DominationAnnouncementIds::SandstormWarning, SandstormCue);

	FSystemAnnouncerCue MatchStartedCue;
	MatchStartedCue.Priority = 50;
	Announcements.Add(DominationAnnouncementIds::MatchStarted, MatchStartedCue);

	FSystemAnnouncerCue OutcomeCue;
	OutcomeCue.Priority = 200;
	OutcomeCue.bInterruptLowerPriority = true;
	Announcements.Add(DominationAnnouncementIds::Victory, OutcomeCue);
	Announcements.Add(DominationAnnouncementIds::Defeat, OutcomeCue);
}

void ADominationSystemAnnouncer::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!StormSoundClass)
	{
		const TSoftObjectPtr<USoundClass> Reference = GetDefault<UShooterAudioSettings>()->SoundEffectsSoundClass;
		StormSoundClass = Reference.Get();
		if (!StormSoundClass && !Reference.IsNull())
		{
			bStormSoundClassLoading = true;
			StormSoundClassLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
				Reference.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, Reference]()
				{
					if (bStormAudioShuttingDown) return;
					StormSoundClass = Reference.Get();
					bStormSoundClassLoading = false;
					StormSoundClassLoadHandle.Reset();
					UpdateSandstormAudio();
				}));
			if (!StormSoundClassLoadHandle.IsValid()) bStormSoundClassLoading = false;
		}
	}
	BindSandstormState();
}

void ADominationSystemAnnouncer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bStormAudioShuttingDown = true;
	if (StormSoundClassLoadHandle.IsValid())
	{
		StormSoundClassLoadHandle->CancelHandle();
		StormSoundClassLoadHandle.Reset();
	}
	bStormSoundClassLoading = false;
	if (ADominationGameState* GameState = BoundGameState.Get())
	{
		GameState->OnSandstormStateChanged.RemoveDynamic(this, &ThisClass::HandleSandstormStateChanged);
		GameState->OnDominationFlowStateChanged.RemoveDynamic(this, &ThisClass::HandleMatchFlowStateChanged);
		GameState->OnMatchStateChanged.RemoveDynamic(this, &ThisClass::HandleMatchStateChanged);
	}
	BoundGameState.Reset();
	SetActorTickEnabled(false);
	StopStormLoop();
	Super::EndPlay(EndPlayReason);
}

void ADominationSystemAnnouncer::OnRep_Owner()
{
	Super::OnRep_Owner();
	if (HasActorBegunPlay())
	{
		BindSandstormState();
	}
}

void ADominationSystemAnnouncer::BindSandstormState()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ADominationGameState* GameState = Cast<ADominationGameState>(GetOwner());
	if (BoundGameState.Get() == GameState)
	{
		return;
	}

	if (ADominationGameState* PreviousGameState = BoundGameState.Get())
	{
		PreviousGameState->OnSandstormStateChanged.RemoveDynamic(this, &ThisClass::HandleSandstormStateChanged);
		PreviousGameState->OnDominationFlowStateChanged.RemoveDynamic(this, &ThisClass::HandleMatchFlowStateChanged);
		PreviousGameState->OnMatchStateChanged.RemoveDynamic(this, &ThisClass::HandleMatchStateChanged);
	}
	BoundGameState = GameState;
	LastWarningPhaseEndServerTime = 0.0;

	if (GameState)
	{
		GameState->OnSandstormStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSandstormStateChanged);
		GameState->OnDominationFlowStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchFlowStateChanged);
		GameState->OnMatchStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchStateChanged);
		// 진행 중 입장도 현재 단계와 남은 시간부터 재생한다.
		HandleSandstormStateChanged(GameState->GetSandstormState());
	}
	else
	{
		SetActorTickEnabled(false);
		StopStormLoop();
	}
}

void ADominationSystemAnnouncer::HandleSandstormStateChanged(const FDominationSandstormState& State)
{
	const ADominationGameState* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return;
	}

	if (State.Phase != EDominationSandstormPhase::Warning
		|| GameState->GetDominationPhase() == EDominationMatchPhase::MatchEnded)
	{
		CancelAnnouncement(DominationAnnouncementIds::SandstormWarning);
	}
	else if (State.PhaseEndServerTime != LastWarningPhaseEndServerTime)
	{
		LastWarningPhaseEndServerTime = State.PhaseEndServerTime;
		const float Remaining = State.GetTimeRemaining(GameState->GetServerWorldTimeSeconds());
		if (Remaining > 0.0f)
		{
			// 서버도 같은 상태 구독 경로를 사용하므로 별도 멀티캐스트와 중복되지 않는다.
			QueueAnnouncement(DominationAnnouncementIds::SandstormWarning, Remaining);
		}
	}

	UpdateSandstormAudio();
}

void ADominationSystemAnnouncer::HandleMatchFlowStateChanged()
{
	if (const ADominationGameState* GameState = BoundGameState.Get())
	{
		HandleSandstormStateChanged(GameState->GetSandstormState());
	}
	TryAnnounceMatchOutcome();
}

void ADominationSystemAnnouncer::HandleMatchStateChanged()
{
	TryAnnounceMatchOutcome();
}

void ADominationSystemAnnouncer::TryAnnounceMatchOutcome()
{
	const ADominationGameState* GameState = BoundGameState.Get();
	if (bMatchOutcomeAnnounced || !GameState
		|| GameState->GetDominationPhase() != EDominationMatchPhase::MatchEnded
		|| GameState->GetMatchWinner() == EGameTeam::None)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* LocalController = GameInstance
		? GameInstance->GetFirstLocalPlayerController(GetWorld())
		: nullptr;
	const ABaseTeamPlayerState* LocalPlayerState = LocalController
		? LocalController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	if (!LocalPlayerState || LocalPlayerState->GetTeam() == EGameTeam::None)
	{
		return;
	}

	bMatchOutcomeAnnounced = true;
	QueueAnnouncement(LocalPlayerState->GetTeam() == GameState->GetMatchWinner()
		? DominationAnnouncementIds::Victory
		: DominationAnnouncementIds::Defeat);
}

void ADominationSystemAnnouncer::Tick(const float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DominationSystemAnnouncer_Tick);
	Super::Tick(DeltaSeconds);
	UpdateSandstormAudio();
}

void ADominationSystemAnnouncer::UpdateSandstormAudio()
{
	if (bStormAudioShuttingDown || bStormSoundClassLoading)
	{
		SetActorTickEnabled(false);
		return;
	}
	const ADominationGameState* GameState = BoundGameState.Get();
	if (!GameState || GameState->GetDominationPhase() == EDominationMatchPhase::MatchEnded)
	{
		SetActorTickEnabled(false);
		StopStormLoop();
		return;
	}

	const FDominationSandstormState& State = GameState->GetSandstormState();
	const double ServerTime = GameState->GetServerWorldTimeSeconds();
	float Intensity = State.GetIntensity(ServerTime);
	bool bNeedsFadeTick = State.GetTimeRemaining(ServerTime) > 0.0f
		&& (State.Phase == EDominationSandstormPhase::FadeIn
			|| State.Phase == EDominationSandstormPhase::FadeOut);
#if !UE_BUILD_SHIPPING
	float LocalPreviewIntensity = PreviewIntensity;
	if (LocalPreviewIntensity < 0.0f)
	{
		if (const UDominationSandstormSettings* Settings = GameState->GetSandstormSettings())
		{
			LocalPreviewIntensity = Settings->PreviewIntensity;
		}
	}
	if (LocalPreviewIntensity >= 0.0f)
	{
		Intensity = FMath::Clamp(LocalPreviewIntensity, 0.0f, 1.0f);
		bNeedsFadeTick = false;
	}
#endif
	SetActorTickEnabled(bNeedsFadeTick && StormLoopSound != nullptr);

	if (Intensity <= UE_SMALL_NUMBER || !StormLoopSound)
	{
		StopStormLoop();
		return;
	}

	const float Volume = Intensity * PeakStormVolume;
	if (!StormAudioComponent)
	{
		StormAudioComponent = UGameplayStatics::CreateSound2D(
			this, StormLoopSound, Volume, 1.0f, 0.0f, nullptr, false, false);
		if (StormAudioComponent)
		{
			if (StormSoundClass)
			{
				StormAudioComponent->SoundClassOverride = StormSoundClass;
			}
			StormAudioComponent->Play();
		}
	}
	else
	{
		StormAudioComponent->SetVolumeMultiplier(Volume);
	}
}

void ADominationSystemAnnouncer::StopStormLoop()
{
	if (StormAudioComponent)
	{
		StormAudioComponent->Stop();
		StormAudioComponent->DestroyComponent();
		StormAudioComponent = nullptr;
	}
}

#if !UE_BUILD_SHIPPING
void ADominationSystemAnnouncer::SetSandstormPreviewIntensity(const float Intensity)
{
	PreviewIntensity = FMath::Clamp(Intensity, -1.0f, 1.0f);
	UpdateSandstormAudio();
}
#endif

#if WITH_EDITOR
EDataValidationResult ADominationSystemAnnouncer::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const FName AnnouncementId : {
		DominationAnnouncementIds::MatchStarted,
		DominationAnnouncementIds::CaptureStarted,
		DominationAnnouncementIds::CaptureCompleted,
		DominationAnnouncementIds::Victory,
		DominationAnnouncementIds::Defeat })
	{
		const FSystemAnnouncerCue* Cue = Announcements.Find(AnnouncementId);
		if (Cue == nullptr || !Cue->Sound)
		{
			Context.AddWarning(FText::Format(
				LOCTEXT("MissingSound", "'{0}' 에 사운드가 없습니다. 방송해도 소리 없이 건너뜁니다"),
				FText::FromName(AnnouncementId)));
			continue;
		}

		if (Cue->Sound->IsLooping())
		{
			Context.AddError(FText::Format(
				LOCTEXT("LoopingSound", "'{0}' 의 사운드가 반복 재생입니다. 방송기가 거부해 재생되지 않습니다"),
				FText::FromName(AnnouncementId)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
