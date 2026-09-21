#include "Settings/ShooterGameUserSettings.h"

#include "Engine/Engine.h"
#include "Internationalization/Internationalization.h"
#include "HAL/IConsoleManager.h"
#include "Settings/ShooterAudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace ShooterGameUserSettings
{
	constexpr float MinMouseSensitivity = 0.01f;
	constexpr float MaxMouseSensitivity = 10.0f;
	constexpr float MinAimSensitivityMultiplier = 0.01f;
	constexpr float MaxAimSensitivityMultiplier = 2.0f;
	constexpr float MinAudioVolume = 0.0f;
	constexpr float MaxAudioVolume = 1.0f;
	constexpr int32 MaxResolutionWidth = 1920;
	constexpr int32 MaxResolutionHeight = 1080;
	constexpr float MinFrameRateLimit = 30.0f;
	constexpr float MaxFrameRateLimit = 240.0f;
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
	CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.4f);
	CrosshairThickness = -0.5f;
	CrosshairGap = -0.25f;
	CrosshairOutlineThickness = 0.f;
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
	SetScreenResolution(FIntPoint(ShooterGameUserSettings::MaxResolutionWidth, ShooterGameUserSettings::MaxResolutionHeight));
	SetFrameRateLimit(ShooterGameUserSettings::MaxFrameRateLimit);
	OnAudioSettingsChanged.Broadcast();
	OnCrosshairSettingsChanged.Broadcast();
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
	PerformanceTargetFPS = FMath::Clamp(PerformanceTargetFPS, 30.0f, ShooterGameUserSettings::MaxFrameRateLimit);

	const FIntPoint CurrentResolution = GetScreenResolution();
	if (CurrentResolution.X <= 0 || CurrentResolution.Y <= 0
		|| CurrentResolution.X > ShooterGameUserSettings::MaxResolutionWidth
		|| CurrentResolution.Y > ShooterGameUserSettings::MaxResolutionHeight)
	{
		SetScreenResolution(FIntPoint(
			ShooterGameUserSettings::MaxResolutionWidth,
			ShooterGameUserSettings::MaxResolutionHeight));
	}

	const float CurrentFrameRateLimit = GetFrameRateLimit();
	SetFrameRateLimit(
		!FMath::IsFinite(CurrentFrameRateLimit) || CurrentFrameRateLimit <= 0.0f
			? ShooterGameUserSettings::MaxFrameRateLimit
			: FMath::Clamp(
				CurrentFrameRateLimit,
				ShooterGameUserSettings::MinFrameRateLimit,
				ShooterGameUserSettings::MaxFrameRateLimit));

	CrosshairThickness = FMath::Clamp(CrosshairThickness, -1.0f, 0.0f);
	CrosshairGap = FMath::Clamp(CrosshairGap, -1.0f, 0.0f);
	CrosshairOutlineThickness = FMath::Clamp(CrosshairOutlineThickness, -1.0f, 1.0f);
}


void UShooterGameUserSettings::ApplyNonResolutionSettings()
{
	const float CurrentFrameRateLimit = GetFrameRateLimit();
	const float EffectiveFrameRateLimit =
		!FMath::IsFinite(CurrentFrameRateLimit) || CurrentFrameRateLimit <= 0.0f
			? ShooterGameUserSettings::MaxFrameRateLimit
			: FMath::Clamp(
				CurrentFrameRateLimit,
				ShooterGameUserSettings::MinFrameRateLimit,
				ShooterGameUserSettings::MaxFrameRateLimit);

	if (!FMath::IsNearlyEqual(CurrentFrameRateLimit, EffectiveFrameRateLimit))
	{
		SetFrameRateLimit(EffectiveFrameRateLimit);
	}

	Super::ApplyNonResolutionSettings();

	// 다른 초기화 경로에서 t.MaxFPS가 바뀌어도 옵션의 프레임 제한을 최종값으로 맞춘다.
	if (IConsoleVariable* MaxFPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		MaxFPSCVar->Set(EffectiveFrameRateLimit, ECVF_SetByGameSetting);
	}
}

void UShooterGameUserSettings::SetDynamicCrosshairEnabled(const bool bEnabled)
{
	bDynamicCrosshair = bEnabled;
}

void UShooterGameUserSettings::SetCrosshairColor(const FLinearColor NewColor)
{
	if (CrosshairColor == NewColor)
	{
		return;
	}

	CrosshairColor = NewColor;
	OnCrosshairSettingsChanged.Broadcast();
}

void UShooterGameUserSettings::SetCrosshairThickness(const float NewThickness)
{
	const float Clamped = FMath::Clamp(NewThickness, -1.0f, 0.0f);
	if (FMath::IsNearlyEqual(CrosshairThickness, Clamped))
	{
		return;
	}

	CrosshairThickness = Clamped;
	OnCrosshairSettingsChanged.Broadcast();
}

void UShooterGameUserSettings::SetCrosshairGap(const float NewGap)
{
	const float Clamped = FMath::Clamp(NewGap, -1.0f, 0.0f);
	if (FMath::IsNearlyEqual(CrosshairGap, Clamped))
	{
		return;
	}

	CrosshairGap = Clamped;
	OnCrosshairSettingsChanged.Broadcast();
}

void UShooterGameUserSettings::SetCrosshairOutlineThickness(const float NewThickness)
{
	const float Clamped = FMath::Clamp(NewThickness, -1.0f, 1.0f);
	if (FMath::IsNearlyEqual(CrosshairOutlineThickness, Clamped))
	{
		return;
	}

	CrosshairOutlineThickness = Clamped;
	OnCrosshairSettingsChanged.Broadcast();
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

USoundMix* UShooterGameUserSettings::GetUserSettingsSoundMix()
{
	if (!CachedUserSettingsSoundMix)
	{
		const TSoftObjectPtr<USoundMix>& Reference = GetDefault<UShooterAudioSettings>()->UserSettingsSoundMix;
		USoundMix* Loaded = Reference.LoadSynchronous();
		if (!ensureMsgf(Loaded,
			TEXT("Assign UserSettingsSoundMix in Project Settings > Shooter Audio Settings.")))
		{
			return nullptr;
		}

		CachedUserSettingsSoundMix = Loaded;
	}

	return CachedUserSettingsSoundMix;
}

USoundClass* UShooterGameUserSettings::GetAudioChannelSoundClass(const EShooterAudioChannel Channel)
{
	if (const TObjectPtr<USoundClass>* Cached = CachedAudioChannelSoundClasses.Find(Channel))
	{
		return *Cached;
	}

	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();
	const TSoftObjectPtr<USoundClass>* Reference = nullptr;
	switch (Channel)
	{
	case EShooterAudioChannel::Master:
		Reference = &AudioSettings->MasterSoundClass;
		break;
	case EShooterAudioChannel::Music:
		Reference = &AudioSettings->MusicSoundClass;
		break;
	case EShooterAudioChannel::SoundEffects:
		Reference = &AudioSettings->SoundEffectsSoundClass;
		break;
	case EShooterAudioChannel::UserInterface:
		Reference = &AudioSettings->UserInterfaceSoundClass;
		break;
	case EShooterAudioChannel::Voice:
		Reference = &AudioSettings->VoiceSoundClass;
		break;
	default:
		return nullptr;
	}

	USoundClass* Loaded = Reference->LoadSynchronous();
	if (!ensureMsgf(Loaded,
		TEXT("Assign every channel Sound Class in Project Settings > Shooter Audio Settings. Missing: %s"),
		*Reference->ToString()))
	{
		return nullptr;
	}

	CachedAudioChannelSoundClasses.Add(Channel, Loaded);
	return Loaded;
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
