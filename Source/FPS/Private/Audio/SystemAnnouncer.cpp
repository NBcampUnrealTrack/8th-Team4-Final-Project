#include "Audio/SystemAnnouncer.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/ShooterAudioSettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"

ASystemAnnouncer::ASystemAnnouncer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	bOnlyRelevantToOwner = false;
	SetReplicateMovement(false);
}

void ASystemAnnouncer::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!IsValid(VoiceSoundClass))
	{
		VoiceSoundClass = GetDefault<UShooterAudioSettings>()->VoiceSoundClass.LoadSynchronous();
	}

	PlayNextAnnouncement();
}

void ASystemAnnouncer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bShuttingDown = true;
	ClearLocalAnnouncements();
	LastSentTimes.Reset();
	Super::EndPlay(EndPlayReason);
}

bool ASystemAnnouncer::HasAnnouncement(const FName AnnouncementId) const
{
	const FSystemAnnouncerCue* Cue = Announcements.Find(AnnouncementId);
	return !AnnouncementId.IsNone() && Cue != nullptr
		&& IsValid(Cue->Sound) && !Cue->Sound->IsLooping();
}

bool ASystemAnnouncer::Announce(const FName AnnouncementId)
{
	if (!HasAuthority() || bShuttingDown || !HasAnnouncement(AnnouncementId))
	{
		return false;
	}

	const FSystemAnnouncerCue& Cue = Announcements.FindChecked(AnnouncementId);
	const double Now = GetWorld()->GetRealTimeSeconds();
	const double* LastSentTime = LastSentTimes.Find(AnnouncementId);
	if (LastSentTime != nullptr && Now - *LastSentTime < Cue.CooldownSeconds)
	{
		return false;
	}

	LastSentTimes.Add(AnnouncementId, Now);
	MulticastAnnounce(AnnouncementId);
	return true;
}

void ASystemAnnouncer::StopAnnouncements()
{
	if (HasAuthority() && !bShuttingDown)
	{
		LastSentTimes.Reset();
		MulticastStopAnnouncements();
	}
}

void ASystemAnnouncer::MulticastAnnounce_Implementation(const FName AnnouncementId)
{
	if (GetNetMode() != NM_DedicatedServer && !bShuttingDown)
	{
		QueueAnnouncement(AnnouncementId);
	}
}

void ASystemAnnouncer::MulticastStopAnnouncements_Implementation()
{
	ClearLocalAnnouncements();
}

void ASystemAnnouncer::QueueAnnouncement(const FName AnnouncementId)
{
	if (!HasAnnouncement(AnnouncementId) || CurrentAnnouncementId == AnnouncementId)
	{
		return;
	}

	const FSystemAnnouncerCue& Cue = Announcements.FindChecked(AnnouncementId);
	const double Now = GetWorld()->GetRealTimeSeconds();
	PendingAnnouncements.RemoveAll([this, Now](const FQueuedAnnouncement& Entry)
	{
		const FSystemAnnouncerCue* QueuedCue = Announcements.Find(Entry.Id);
		return QueuedCue == nullptr || !IsValid(QueuedCue->Sound)
			|| Now - Entry.QueuedAt > QueuedCue->MaxQueueDelaySeconds;
	});

	if (PendingAnnouncements.ContainsByPredicate([AnnouncementId](const FQueuedAnnouncement& Entry)
	{
		return Entry.Id == AnnouncementId;
	}))
	{
		return;
	}

	if (Cue.bInterruptLowerPriority)
	{
		PendingAnnouncements.RemoveAll([this, &Cue](const FQueuedAnnouncement& Entry)
		{
			return Announcements.FindChecked(Entry.Id).Priority < Cue.Priority;
		});

		if (!CurrentAnnouncementId.IsNone() && Cue.Priority > CurrentPriority)
		{
			StopCurrentVoice();
		}
	}

	if (PendingAnnouncements.Num() >= FMath::Clamp(MaxQueuedAnnouncements, 1, 32))
	{
		int32 LowestPriorityIndex = 0;
		for (int32 Index = 1; Index < PendingAnnouncements.Num(); ++Index)
		{
			if (Announcements.FindChecked(PendingAnnouncements[Index].Id).Priority
				< Announcements.FindChecked(PendingAnnouncements[LowestPriorityIndex].Id).Priority)
			{
				LowestPriorityIndex = Index;
			}
		}

		if (Cue.Priority <= Announcements.FindChecked(PendingAnnouncements[LowestPriorityIndex].Id).Priority)
		{
			return;
		}
		PendingAnnouncements.RemoveAt(LowestPriorityIndex);
	}

	FQueuedAnnouncement Entry;
	Entry.Id = AnnouncementId;
	Entry.QueuedAt = Now;
	PendingAnnouncements.Add(Entry);

	// 생성 초기 RPC는 보관하되 오디오 초기화가 끝난 뒤 재생한다.
	if (HasActorBegunPlay())
	{
		PlayNextAnnouncement();
	}
}

void ASystemAnnouncer::PlayNextAnnouncement()
{
	if (bShuttingDown || !CurrentAnnouncementId.IsNone() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	while (!PendingAnnouncements.IsEmpty())
	{
		const double Now = GetWorld()->GetRealTimeSeconds();
		PendingAnnouncements.RemoveAll([this, Now](const FQueuedAnnouncement& Entry)
		{
			const FSystemAnnouncerCue* Cue = Announcements.Find(Entry.Id);
			return Cue == nullptr || !IsValid(Cue->Sound) || Cue->Sound->IsLooping()
				|| Now - Entry.QueuedAt > Cue->MaxQueueDelaySeconds;
		});
		if (PendingAnnouncements.IsEmpty())
		{
			return;
		}

		int32 NextIndex = 0;
		for (int32 Index = 1; Index < PendingAnnouncements.Num(); ++Index)
		{
			if (Announcements.FindChecked(PendingAnnouncements[Index].Id).Priority
				> Announcements.FindChecked(PendingAnnouncements[NextIndex].Id).Priority)
			{
				NextIndex = Index;
			}
		}

		const FName NextId = PendingAnnouncements[NextIndex].Id;
		PendingAnnouncements.RemoveAt(NextIndex);
		if (TryPlayAnnouncement(NextId, Announcements.FindChecked(NextId)))
		{
			return;
		}
	}
}

bool ASystemAnnouncer::TryPlayAnnouncement(
	const FName AnnouncementId,
	const FSystemAnnouncerCue& Cue)
{
	UAudioComponent* NewAudioComponent = UGameplayStatics::CreateSound2D(
		this, Cue.Sound, FMath::Max(0.0f, Cue.VolumeMultiplier), 1.0f,
		0.0f, nullptr, false, true);
	if (!IsValid(NewAudioComponent))
	{
		return false;
	}

	if (IsValid(VoiceSoundClass))
	{
		NewAudioComponent->SoundClassOverride = VoiceSoundClass;
	}

	VoiceAudioComponent = NewAudioComponent;
	CurrentAnnouncementId = AnnouncementId;
	CurrentPriority = Cue.Priority;
	NewAudioComponent->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::HandleVoiceFinished);
	NewAudioComponent->Play();

	// 오디오 장치가 없거나 재생이 거부되어도 대기열이 멈추지 않는다.
	if (!IsValid(VoiceAudioComponent) || !VoiceAudioComponent->IsPlaying())
	{
		StopCurrentVoice();
		return false;
	}
	return true;
}

void ASystemAnnouncer::HandleVoiceFinished()
{
	if (IsValid(VoiceAudioComponent))
	{
		VoiceAudioComponent->OnAudioFinished.RemoveDynamic(this, &ThisClass::HandleVoiceFinished);
	}
	VoiceAudioComponent = nullptr;
	CurrentAnnouncementId = NAME_None;
	CurrentPriority = 0;
	PlayNextAnnouncement();
}

void ASystemAnnouncer::StopCurrentVoice()
{
	if (IsValid(VoiceAudioComponent))
	{
		UAudioComponent* StoppedComponent = VoiceAudioComponent;
		VoiceAudioComponent = nullptr;
		// Stop도 완료 콜백을 발생시키므로 먼저 구독을 해제한다.
		StoppedComponent->OnAudioFinished.RemoveDynamic(this, &ThisClass::HandleVoiceFinished);
		StoppedComponent->Stop();
		if (IsValid(StoppedComponent))
		{
			StoppedComponent->DestroyComponent();
		}
	}
	CurrentAnnouncementId = NAME_None;
	CurrentPriority = 0;
}

void ASystemAnnouncer::ClearLocalAnnouncements()
{
	PendingAnnouncements.Reset();
	StopCurrentVoice();
}
