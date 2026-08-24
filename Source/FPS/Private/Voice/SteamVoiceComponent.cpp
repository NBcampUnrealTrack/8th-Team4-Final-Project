#include "Voice/SteamVoiceComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerController.h"
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
	if (bCapturingVoice || !SteamUser())
	{
		return;
	}

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

bool USteamVoiceComponent::DecompressVoicePacket(
	APlayerState* SpeakerPlayerState,
	const TArray<uint8>& CompressedVoiceData,
	TArray<uint8>& OutPcmVoiceData,
	int32& OutSampleRate) const
{
	OutPcmVoiceData.Reset();
	OutSampleRate = 0;

	if (!IsValid(SpeakerPlayerState) || CompressedVoiceData.IsEmpty() || !SteamUser())
	{
		return false;
	}

	OutSampleRate = static_cast<int32>(SteamUser()->GetVoiceOptimalSampleRate());
	if (OutSampleRate <= 0)
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
		static_cast<uint32>(OutSampleRate));

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
			static_cast<uint32>(OutSampleRate));
	}

	if (DecompressResult != k_EVoiceResultOK || BytesWritten == 0)
	{
		OutPcmVoiceData.Reset();
		return false;
	}

	OutPcmVoiceData.SetNum(static_cast<int32>(BytesWritten));
	return true;
}

void USteamVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCapture();
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

	if (AShooterPlayerController* PlayerController = GetOwnerShooterPlayerController())
	{
		PlayerController->SubmitCompressedVoicePacket(CompressedVoiceData);
	}
}

AShooterPlayerController* USteamVoiceComponent::GetOwnerShooterPlayerController() const
{
	return Cast<AShooterPlayerController>(GetOwner());
}
