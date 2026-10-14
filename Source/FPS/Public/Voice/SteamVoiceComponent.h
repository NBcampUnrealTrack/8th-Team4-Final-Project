#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "SteamVoiceComponent.generated.h"

class APlayerState;
class AShooterPlayerController;
class UAudioComponent;
class USoundWaveProcedural;

USTRUCT()
struct FSteamVoicePlaybackSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerState> SpeakerPlayerState;

	UPROPERTY()
	TObjectPtr<USoundWaveProcedural> SoundWave;

	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent;

	int32 SampleRate = 0;
};

UCLASS(ClassGroup = (Custom))
class FPS_API USteamVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USteamVoiceComponent();

	/** Starts Steam voice capture. Returns false when capture cannot be started. */
	bool StartCapture();
	void StopCapture();
	bool IsCapturingVoice() const { return bCapturingVoice; }

	void PlayCompressedVoicePacket(
		APlayerState* SpeakerPlayerState,
		const TArray<uint8>& CompressedVoiceData,
		float SenderMicrophoneVolume = 1.0f);
	void StopPlayback();
	void SetOutputVolume(float Volume);

private:
	bool DecompressVoicePacket(
		APlayerState* SpeakerPlayerState,
		const TArray<uint8>& CompressedVoiceData,
		TArray<uint8>& OutPcmVoiceData);
	static void ApplyPcmGain(TArray<uint8>& PcmVoiceData, float Gain);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void PollVoiceCapture();
	FSteamVoicePlaybackSlot* FindOrCreatePlaybackSlot(APlayerState* SpeakerPlayerState);
	AShooterPlayerController* GetOwnerShooterPlayerController() const;
	int32 GetVoiceSampleRate();

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice")
	float CapturePollInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice")
	int32 MaxCompressedVoiceBytes = 8192;

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice")
	int32 InitialPcmBufferBytes = 20480;

	UPROPERTY()
	TArray<FSteamVoicePlaybackSlot> PlaybackSlots;

	FTimerHandle VoiceCaptureTimerHandle;
	bool bCapturingVoice = false;
	int32 CachedVoiceSampleRate = 0;
	float VoiceOutputVolume = 1.0f;
};
