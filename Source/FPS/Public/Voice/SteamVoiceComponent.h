#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "SteamVoiceComponent.generated.h"

class APlayerState;
class AShooterPlayerController;

UCLASS(ClassGroup = (Custom))
class FPS_API USteamVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USteamVoiceComponent();

	void StartCapture();
	void StopCapture();
	bool IsCapturingVoice() const { return bCapturingVoice; }

	bool DecompressVoicePacket(
		APlayerState* SpeakerPlayerState,
		const TArray<uint8>& CompressedVoiceData,
		TArray<uint8>& OutPcmVoiceData,
		int32& OutSampleRate) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void PollVoiceCapture();
	AShooterPlayerController* GetOwnerShooterPlayerController() const;

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice")
	float CapturePollInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice")
	int32 MaxCompressedVoiceBytes = 8192;

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice")
	int32 InitialPcmBufferBytes = 20480;

	FTimerHandle VoiceCaptureTimerHandle;
	bool bCapturingVoice = false;
};
