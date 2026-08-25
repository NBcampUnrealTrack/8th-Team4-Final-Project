#include "Voice/SteamVoiceComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Sound/SoundGroups.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

USteamVoiceComponent::USteamVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USteamVoiceComponent::StartCapture()
{
	if (!SteamUser())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][StartCapture] SteamUser=null Owner=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	if (bCapturingVoice)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][StartCapture] SteamUser=valid Owner=%s"), *GetNameSafe(GetOwner()));
	SteamUser()->StartVoiceRecording();
	bCapturingVoice = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			VoiceCaptureTimerHandle,
			this,
			&ThisClass::PollVoiceCapture,
			FMath::Max(0.01f, CapturePollInterval),
			true);
	}
}

void USteamVoiceComponent::StopCapture()
{
	if (!bCapturingVoice)
	{
		return;
	}

	if (SteamUser())
	{
		SteamUser()->StopVoiceRecording();
	}

	bCapturingVoice = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VoiceCaptureTimerHandle);
	}
}

void USteamVoiceComponent::PlayCompressedVoicePacket(APlayerState* SpeakerPlayerState, const TArray<uint8>& CompressedVoiceData)
{
	TArray<uint8> PcmVoiceData;
	if (!DecompressVoicePacket(SpeakerPlayerState, CompressedVoiceData, PcmVoiceData))
	{
		return;
	}

	FSteamVoicePlaybackSlot* PlaybackSlot = FindOrCreatePlaybackSlot(SpeakerPlayerState);
	if (!PlaybackSlot || !PlaybackSlot->SoundWave || !PlaybackSlot->AudioComponent)
	{
		return;
	}

	PlaybackSlot->SoundWave->QueueAudio(PcmVoiceData.GetData(), PcmVoiceData.Num());

	if (!PlaybackSlot->AudioComponent->IsPlaying())
	{
		PlaybackSlot->AudioComponent->Play();
	}
}

bool USteamVoiceComponent::DecompressVoicePacket(
	APlayerState* SpeakerPlayerState,
	const TArray<uint8>& CompressedVoiceData,
	TArray<uint8>& OutPcmVoiceData)
{
	OutPcmVoiceData.Reset();

	if (!IsValid(SpeakerPlayerState) || CompressedVoiceData.IsEmpty() || !SteamUser())
	{
		return false;
	}

	const int32 SampleRate = GetVoiceSampleRate();
	if (SampleRate <= 0)
	{
		return false;
	}

	OutPcmVoiceData.SetNumUninitialized(FMath::Max(InitialPcmBufferBytes, 0));
	if (OutPcmVoiceData.IsEmpty())
	{
		return false;
	}

	uint32 BytesWritten = 0;
	EVoiceResult DecompressResult = SteamUser()->DecompressVoice(
		CompressedVoiceData.GetData(),
		static_cast<uint32>(CompressedVoiceData.Num()),
		OutPcmVoiceData.GetData(),
		static_cast<uint32>(OutPcmVoiceData.Num()),
		&BytesWritten,
		static_cast<uint32>(SampleRate));

	if (DecompressResult == k_EVoiceResultBufferTooSmall && BytesWritten > 0)
	{
		OutPcmVoiceData.SetNumUninitialized(static_cast<int32>(BytesWritten));
		BytesWritten = 0;

		DecompressResult = SteamUser()->DecompressVoice(
			CompressedVoiceData.GetData(),
			static_cast<uint32>(CompressedVoiceData.Num()),
			OutPcmVoiceData.GetData(),
			static_cast<uint32>(OutPcmVoiceData.Num()),
			&BytesWritten,
			static_cast<uint32>(SampleRate));
	}

	if (DecompressResult != k_EVoiceResultOK || BytesWritten == 0)
	{
		OutPcmVoiceData.Reset();
		UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Decompress] Failed Speaker=%s CompressedBytes=%d Result=%d SampleRate=%d"),
			*GetNameSafe(SpeakerPlayerState),
			CompressedVoiceData.Num(),
			static_cast<int32>(DecompressResult),
			SampleRate);
		return false;
	}

	OutPcmVoiceData.SetNum(static_cast<int32>(BytesWritten));
	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Decompress] Success Speaker=%s CompressedBytes=%d PcmBytes=%d SampleRate=%d"),
		*GetNameSafe(SpeakerPlayerState),
		CompressedVoiceData.Num(),
		OutPcmVoiceData.Num(),
		SampleRate);
	return true;
}

void USteamVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCapture();

	for (FSteamVoicePlaybackSlot& PlaybackSlot : PlaybackSlots)
	{
		if (PlaybackSlot.AudioComponent)
		{
			PlaybackSlot.AudioComponent->Stop();
			PlaybackSlot.AudioComponent->DestroyComponent();
		}

		if (PlaybackSlot.SoundWave)
		{
			PlaybackSlot.SoundWave->ResetAudio();
		}
	}

	PlaybackSlots.Reset();
	CachedVoiceSampleRate = 0;
	Super::EndPlay(EndPlayReason);
}

void USteamVoiceComponent::PollVoiceCapture()
{
	if (!bCapturingVoice || !SteamUser())
	{
		StopCapture();
		return;
	}

	uint32 AvailableCompressedBytes = 0;
	const EVoiceResult AvailableResult = SteamUser()->GetAvailableVoice(&AvailableCompressedBytes);
	if (AvailableResult == k_EVoiceResultNotRecording)
	{
		StopCapture();
		return;
	}

	if (AvailableResult != k_EVoiceResultOK || AvailableCompressedBytes == 0)
	{
		return;
	}

	const uint32 BufferSize = FMath::Min<uint32>(
		AvailableCompressedBytes,
		static_cast<uint32>(FMath::Max(0, MaxCompressedVoiceBytes)));
	if (BufferSize == 0)
	{
		return;
	}

	TArray<uint8> CompressedVoiceData;
	CompressedVoiceData.SetNumUninitialized(static_cast<int32>(BufferSize));

	uint32 BytesWritten = 0;
	const EVoiceResult VoiceResult = SteamUser()->GetVoice(
		true,
		CompressedVoiceData.GetData(),
		BufferSize,
		&BytesWritten);

	if (VoiceResult != k_EVoiceResultOK || BytesWritten == 0)
	{
		return;
	}

	CompressedVoiceData.SetNum(static_cast<int32>(BytesWritten));
	UE_LOG(LogTemp, Warning, TEXT("[VoiceTrace][Capture] Owner=%s CompressedBytes=%d"),
		*GetNameSafe(GetOwner()),
		CompressedVoiceData.Num());

	if (AShooterPlayerController* PlayerController = GetOwnerShooterPlayerController())
	{
		PlayerController->SubmitCompressedVoicePacket(CompressedVoiceData);
	}
}

FSteamVoicePlaybackSlot* USteamVoiceComponent::FindOrCreatePlaybackSlot(APlayerState* SpeakerPlayerState)
{
	const int32 SampleRate = GetVoiceSampleRate();
	if (!IsValid(SpeakerPlayerState) || SampleRate <= 0)
	{
		return nullptr;
	}

	for (FSteamVoicePlaybackSlot& PlaybackSlot : PlaybackSlots)
	{
		if (PlaybackSlot.SpeakerPlayerState == SpeakerPlayerState)
		{
			if (PlaybackSlot.SampleRate == SampleRate
				&& PlaybackSlot.SoundWave
				&& PlaybackSlot.AudioComponent)
			{
				return &PlaybackSlot;
			}

			if (PlaybackSlot.AudioComponent)
			{
				PlaybackSlot.AudioComponent->Stop();
				PlaybackSlot.AudioComponent->DestroyComponent();
			}

			if (PlaybackSlot.SoundWave)
			{
				PlaybackSlot.SoundWave->ResetAudio();
			}

			PlaybackSlot = FSteamVoicePlaybackSlot();
			PlaybackSlot.SpeakerPlayerState = SpeakerPlayerState;
			break;
		}
	}

	FSteamVoicePlaybackSlot* PlaybackSlot = nullptr;
	for (FSteamVoicePlaybackSlot& ExistingSlot : PlaybackSlots)
	{
		if (ExistingSlot.SpeakerPlayerState == SpeakerPlayerState)
		{
			PlaybackSlot = &ExistingSlot;
			break;
		}
	}

	if (!PlaybackSlot)
	{
		PlaybackSlot = &PlaybackSlots.AddDefaulted_GetRef();
		PlaybackSlot->SpeakerPlayerState = SpeakerPlayerState;
	}

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(OwnerActor) || !IsValid(World))
	{
		return nullptr;
	}

	USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>(this);
	if (!IsValid(SoundWave))
	{
		return nullptr;
	}

	SoundWave->SetSampleRate(SampleRate);
	SoundWave->NumChannels = 1;
	SoundWave->SampleByteSize = sizeof(int16);
	SoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
	SoundWave->SoundGroup = SOUNDGROUP_Voice;

	UAudioComponent* AudioComponent = NewObject<UAudioComponent>(OwnerActor);
	if (!IsValid(AudioComponent))
	{
		return nullptr;
	}

	AudioComponent->bAutoActivate = false;
	AudioComponent->SetSound(SoundWave);
	AudioComponent->RegisterComponentWithWorld(World);

	PlaybackSlot->SoundWave = SoundWave;
	PlaybackSlot->AudioComponent = AudioComponent;
	PlaybackSlot->SampleRate = SampleRate;
	return PlaybackSlot;
}

AShooterPlayerController* USteamVoiceComponent::GetOwnerShooterPlayerController() const
{
	return Cast<AShooterPlayerController>(GetOwner());
}

int32 USteamVoiceComponent::GetVoiceSampleRate()
{
	if (CachedVoiceSampleRate > 0)
	{
		return CachedVoiceSampleRate;
	}

	if (!SteamUser())
	{
		return 0;
	}

	CachedVoiceSampleRate = static_cast<int32>(SteamUser()->GetVoiceOptimalSampleRate());
	return CachedVoiceSampleRate;
}
