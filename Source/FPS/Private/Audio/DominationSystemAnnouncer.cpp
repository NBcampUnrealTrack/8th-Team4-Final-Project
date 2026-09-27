#include "Audio/DominationSystemAnnouncer.h"

#include "Components/AudioComponent.h"
#include "GameMode/State/DominationGameState.h"
#include "GameMode/Settings/DominationSandstormSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/ShooterAudioSettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"

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
		StormSoundClass = GetDefault<UShooterAudioSettings>()->SoundEffectsSoundClass.LoadSynchronous();
	}
	BindSandstormState();
}

void ADominationSystemAnnouncer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADominationGameState* GameState = BoundGameState.Get())
	{
		GameState->OnSandstormStateChanged.RemoveDynamic(this, &ThisClass::HandleSandstormStateChanged);
		GameState->OnDominationFlowStateChanged.RemoveDynamic(this, &ThisClass::HandleMatchFlowStateChanged);
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
	}
	BoundGameState = GameState;
	LastWarningPhaseEndServerTime = 0.0;

	if (GameState)
	{
		GameState->OnSandstormStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSandstormStateChanged);
		GameState->OnDominationFlowStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchFlowStateChanged);
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
}

void ADominationSystemAnnouncer::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSandstormAudio();
}

void ADominationSystemAnnouncer::UpdateSandstormAudio()
{
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
