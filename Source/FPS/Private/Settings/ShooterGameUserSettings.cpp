#include "Settings/ShooterGameUserSettings.h"

#include "Engine/Engine.h"
#include "Internationalization/Internationalization.h"

namespace ShooterGameUserSettings
{
	constexpr float MinMouseSensitivity = 0.01f;
	constexpr float MaxMouseSensitivity = 10.0f;
	constexpr float MinAimSensitivityMultiplier = 0.01f;
	constexpr float MaxAimSensitivityMultiplier = 2.0f;
	constexpr float MinAudioVolume = 0.0f;
	constexpr float MaxAudioVolume = 1.0f;
}

UShooterGameUserSettings* UShooterGameUserSettings::Get()
{
	if (!GEngine)
	{
		return nullptr;
	}

	UShooterGameUserSettings* Settings =
		Cast<UShooterGameUserSettings>(GEngine->GetGameUserSettings());
	ensureMsgf(Settings,
		TEXT("Register GameUserSettingsClassName=/Script/FPS.ShooterGameUserSettings in DefaultEngine.ini."));

	return Settings;
}

void UShooterGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	bDynamicCrosshair = true;
	MouseHorizontalSensitivity = 1.0f;
	MouseVerticalSensitivity = 1.0f;
	AimSensitivityMultiplier = 1.0f;
	bInvertVerticalLook = false;

	MasterVolume = 1.0f;
	MusicVolume = 1.0f;
	SoundEffectsVolume = 1.0f;
	UserInterfaceVolume = 1.0f;
	VoiceVolume = 1.0f;
	bMasterAudioMuted = false;
	bVoiceChatEnabled = true;
	Language = EShooterOptionsLanguage::Korean;
	ExperienceProfile = EShooterExperienceProfile::Balanced120;
	PerformanceTargetFPS = 120.0f;
	OnAudioSettingsChanged.Broadcast();
}

void UShooterGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();

	MouseHorizontalSensitivity = FMath::Clamp(
		MouseHorizontalSensitivity,
		ShooterGameUserSettings::MinMouseSensitivity,
		ShooterGameUserSettings::MaxMouseSensitivity);
	MouseVerticalSensitivity = FMath::Clamp(
		MouseVerticalSensitivity,
		ShooterGameUserSettings::MinMouseSensitivity,
		ShooterGameUserSettings::MaxMouseSensitivity);
	AimSensitivityMultiplier = FMath::Clamp(
		AimSensitivityMultiplier,
		ShooterGameUserSettings::MinAimSensitivityMultiplier,
		ShooterGameUserSettings::MaxAimSensitivityMultiplier);

	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
	SoundEffectsVolume = FMath::Clamp(SoundEffectsVolume, 0.0f, 1.0f);
	UserInterfaceVolume = FMath::Clamp(UserInterfaceVolume, 0.0f, 1.0f);
	VoiceVolume = FMath::Clamp(VoiceVolume, 0.0f, 1.0f);
	PerformanceTargetFPS = FMath::Clamp(PerformanceTargetFPS, 30.0f, 1000.0f);
}

void UShooterGameUserSettings::SetDynamicCrosshairEnabled(const bool bEnabled)
{
	bDynamicCrosshair = bEnabled;
}

void UShooterGameUserSettings::SetMouseHorizontalSensitivity(const float Sensitivity)
{
	MouseHorizontalSensitivity = FMath::Clamp(
		Sensitivity,
		ShooterGameUserSettings::MinMouseSensitivity,
		ShooterGameUserSettings::MaxMouseSensitivity);
}

void UShooterGameUserSettings::SetMouseVerticalSensitivity(const float Sensitivity)
{
	MouseVerticalSensitivity = FMath::Clamp(
		Sensitivity,
		ShooterGameUserSettings::MinMouseSensitivity,
		ShooterGameUserSettings::MaxMouseSensitivity);
}

void UShooterGameUserSettings::SetAimSensitivityMultiplier(const float Multiplier)
{
	AimSensitivityMultiplier = FMath::Clamp(
		Multiplier,
		ShooterGameUserSettings::MinAimSensitivityMultiplier,
		ShooterGameUserSettings::MaxAimSensitivityMultiplier);
}

void UShooterGameUserSettings::SetInvertVerticalLookEnabled(const bool bEnabled)
{
	bInvertVerticalLook = bEnabled;
}

FVector2D UShooterGameUserSettings::ApplyLookSensitivity(
	const FVector2D RawLookInput,
	const bool bAiming) const
{
	FVector2D Result(
		RawLookInput.X * MouseHorizontalSensitivity,
		RawLookInput.Y * MouseVerticalSensitivity * (bInvertVerticalLook ? -1.0f : 1.0f));

	if (bAiming)
	{
		Result *= AimSensitivityMultiplier;
	}

	return Result;
}

float UShooterGameUserSettings::GetAudioVolume(const EShooterAudioChannel Channel) const
{
	switch (Channel)
	{
	case EShooterAudioChannel::Master:
		return MasterVolume;
	case EShooterAudioChannel::Music:
		return MusicVolume;
	case EShooterAudioChannel::SoundEffects:
		return SoundEffectsVolume;
	case EShooterAudioChannel::UserInterface:
		return UserInterfaceVolume;
	case EShooterAudioChannel::Voice:
		return VoiceVolume;
	default:
		return 1.0f;
	}
}

void UShooterGameUserSettings::SetAudioVolume(
	const EShooterAudioChannel Channel,
	const float Volume)
{
	float* TargetVolume = nullptr;
	switch (Channel)
	{
	case EShooterAudioChannel::Master:
		TargetVolume = &MasterVolume;
		break;
	case EShooterAudioChannel::Music:
		TargetVolume = &MusicVolume;
		break;
	case EShooterAudioChannel::SoundEffects:
		TargetVolume = &SoundEffectsVolume;
		break;
	case EShooterAudioChannel::UserInterface:
		TargetVolume = &UserInterfaceVolume;
		break;
	case EShooterAudioChannel::Voice:
		TargetVolume = &VoiceVolume;
		break;
	default:
		return;
	}

	const float ClampedVolume = FMath::Clamp(
		Volume,
		ShooterGameUserSettings::MinAudioVolume,
		ShooterGameUserSettings::MaxAudioVolume);
	if (FMath::IsNearlyEqual(*TargetVolume, ClampedVolume))
	{
		return;
	}

	*TargetVolume = ClampedVolume;
	OnAudioSettingsChanged.Broadcast();
}

void UShooterGameUserSettings::SetMasterAudioMuted(const bool bMuted)
{
	if (bMasterAudioMuted == bMuted)
	{
		return;
	}

	bMasterAudioMuted = bMuted;
	OnAudioSettingsChanged.Broadcast();
}

void UShooterGameUserSettings::SetVoiceChatEnabled(const bool bEnabled)
{
	if (bVoiceChatEnabled == bEnabled)
	{
		return;
	}

	bVoiceChatEnabled = bEnabled;
	OnAudioSettingsChanged.Broadcast();
}

void UShooterGameUserSettings::SetLanguage(const EShooterOptionsLanguage NewLanguage)
{
	if (Language == NewLanguage)
	{
		return;
	}

	Language = NewLanguage;
	ApplyLanguageCulture();
}

void UShooterGameUserSettings::ApplyLanguageCulture() const
{
	const FString CultureName = Language == EShooterOptionsLanguage::English
		? TEXT("en")
		: TEXT("ko-KR");
	FInternationalization::Get().SetCurrentCulture(CultureName);
}
