#include "Settings/ShooterGameUserSettings.h"

#include "Engine/Engine.h"
#include "Internationalization/Internationalization.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Settings/ShooterAudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

namespace ShooterGameUserSettingsConstants
{
	constexpr float MinMouseSensitivity = 0.0f;
	constexpr float MaxMouseSensitivity = 2.0f;
	constexpr float DefaultMouseSensitivity = 1.0f;
	constexpr float MinAudioVolume = 0.0f;
	constexpr float MaxAudioVolume = 2.0f;
	constexpr float DefaultAudioVolume = 1.0f;
	constexpr int32 MaxResolutionWidth = 1920;
	constexpr int32 MaxResolutionHeight = 1080;
	static constexpr float SupportedFrameRateLimits[] = { 60.0f, 80.0f, 100.0f };
	constexpr float DefaultFrameRateLimit = 60.0f;
	constexpr int32 DefaultGraphicsQualityLevel = 1;
	constexpr float MinScreenBrightness = 0.0f;
	constexpr float MaxScreenBrightness = 100.0f;
	constexpr float DefaultScreenBrightness = 50.0f;
	constexpr float MinDisplayGamma = 1.2f;
	constexpr float MaxDisplayGamma = 3.2f;

	float NormalizeScreenBrightness(const float Brightness)
	{
		return FMath::IsFinite(Brightness)
			? FMath::Clamp(Brightness, MinScreenBrightness, MaxScreenBrightness)
			: DefaultScreenBrightness;
	}
}

UShooterGameUserSettings::UShooterGameUserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 부모 생성자의 SetToDefaults는 이 클래스의 재정의 함수를 호출하지 않는다.
	SetOverallScalabilityLevel(GetDefaultGraphicsQualityLevel());
}

FIntPoint UShooterGameUserSettings::GetMaxSupportedResolution()
{
	return FIntPoint(
		ShooterGameUserSettingsConstants::MaxResolutionWidth,
		ShooterGameUserSettingsConstants::MaxResolutionHeight);
}

int32 UShooterGameUserSettings::GetFrameRateLimitChoiceCount()
{
	return UE_ARRAY_COUNT(ShooterGameUserSettingsConstants::SupportedFrameRateLimits);
}

float UShooterGameUserSettings::GetFrameRateLimitChoice(const int32 Index)
{
	return Index >= 0 && Index < GetFrameRateLimitChoiceCount()
		? ShooterGameUserSettingsConstants::SupportedFrameRateLimits[Index]
		: ShooterGameUserSettingsConstants::DefaultFrameRateLimit;
}

int32 UShooterGameUserSettings::FindFrameRateLimitChoiceIndex(const float FramesPerSecond)
{
	if (!FMath::IsFinite(FramesPerSecond))
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < GetFrameRateLimitChoiceCount(); ++Index)
	{
		if (FMath::IsNearlyEqual(FramesPerSecond, GetFrameRateLimitChoice(Index)))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

float UShooterGameUserSettings::NormalizeFrameRateLimit(const float FramesPerSecond)
{
	const int32 Index = FindFrameRateLimitChoiceIndex(FramesPerSecond);
	return Index == INDEX_NONE
		? ShooterGameUserSettingsConstants::DefaultFrameRateLimit
		: GetFrameRateLimitChoice(Index);
}

float UShooterGameUserSettings::GetDefaultFrameRateLimit()
{
	return ShooterGameUserSettingsConstants::DefaultFrameRateLimit;
}

float UShooterGameUserSettings::GetMinMouseSensitivity()
{
	return ShooterGameUserSettingsConstants::MinMouseSensitivity;
}

float UShooterGameUserSettings::GetMaxMouseSensitivity()
{
	return ShooterGameUserSettingsConstants::MaxMouseSensitivity;
}

float UShooterGameUserSettings::GetMinAudioVolume()
{
	return ShooterGameUserSettingsConstants::MinAudioVolume;
}

float UShooterGameUserSettings::GetMaxAudioVolume()
{
	return ShooterGameUserSettingsConstants::MaxAudioVolume;
}

int32 UShooterGameUserSettings::GetDefaultGraphicsQualityLevel()
{
	return ShooterGameUserSettingsConstants::DefaultGraphicsQualityLevel;
}

float UShooterGameUserSettings::GetMinScreenBrightness()
{
	return ShooterGameUserSettingsConstants::MinScreenBrightness;
}

float UShooterGameUserSettings::GetMaxScreenBrightness()
{
	return ShooterGameUserSettingsConstants::MaxScreenBrightness;
}

float UShooterGameUserSettings::GetDefaultScreenBrightness()
{
	return ShooterGameUserSettingsConstants::DefaultScreenBrightness;
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
	SetOverallScalabilityLevel(GetDefaultGraphicsQualityLevel());
	ScreenBrightness = GetDefaultScreenBrightness();

	bDynamicCrosshair = true;
	CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.4f);
	CrosshairThickness = -0.5f;
	CrosshairGap = -0.25f;
	CrosshairOutlineThickness = 0.f;
	MouseHorizontalSensitivity = ShooterGameUserSettingsConstants::DefaultMouseSensitivity;
	MouseVerticalSensitivity = ShooterGameUserSettingsConstants::DefaultMouseSensitivity;
	AimSensitivityMultiplier = ShooterGameUserSettingsConstants::DefaultMouseSensitivity;
	bInvertVerticalLook = false;

	MasterVolume = ShooterGameUserSettingsConstants::DefaultAudioVolume;
	MusicVolume = ShooterGameUserSettingsConstants::DefaultAudioVolume;
	SoundEffectsVolume = ShooterGameUserSettingsConstants::DefaultAudioVolume;
	UserInterfaceVolume = ShooterGameUserSettingsConstants::DefaultAudioVolume;
	VoiceVolume = ShooterGameUserSettingsConstants::DefaultAudioVolume;
	bMasterAudioMuted = false;
	bVoiceChatEnabled = true;
	VoiceChatMicrophoneVolume = ShooterGameUserSettingsConstants::DefaultAudioVolume;
	VoiceChatOutputVolume = ShooterGameUserSettingsConstants::DefaultAudioVolume;
	KeyBindingOverrides.Reset();
	Language = EShooterOptionsLanguage::Korean;
	SetScreenResolution(GetMaxSupportedResolution());
	SetFrameRateLimit(GetDefaultFrameRateLimit());
	ResetInputModes();
	ApplyScreenBrightness();
	OnAudioSettingsChanged.Broadcast();
	OnCrosshairSettingsChanged.Broadcast();
	OnInputBindingsChanged.Broadcast();
}

void UShooterGameUserSettings::LoadSettings(const bool bForceReload)
{
	const bool bPreviousToggleADS = bToggleADS;
	const bool bPreviousToggleCrouch = bToggleCrouch;
	Super::LoadSettings(bForceReload);

	// 저장된 품질은 유지하고, 첫 실행에만 Medium을 기본값으로 사용한다.
	if (!GConfig || !GConfig->DoesSectionExist(TEXT("ScalabilityGroups"), GGameUserSettingsIni))
	{
		SetOverallScalabilityLevel(GetDefaultGraphicsQualityLevel());
	}

	SetScreenBrightness(ScreenBrightness);
	if (bPreviousToggleADS != bToggleADS || bPreviousToggleCrouch != bToggleCrouch)
	{
		OnInputModesChanged.Broadcast();
	}
}

void UShooterGameUserSettings::SetADSToggleEnabled(const bool bEnabled)
{
	if (bToggleADS != bEnabled)
	{
		bToggleADS = bEnabled;
		OnInputModesChanged.Broadcast();
	}
}

void UShooterGameUserSettings::SetCrouchToggleEnabled(const bool bEnabled)
{
	if (bToggleCrouch != bEnabled)
	{
		bToggleCrouch = bEnabled;
		OnInputModesChanged.Broadcast();
	}
}

void UShooterGameUserSettings::ResetInputModes()
{
	if (!bToggleADS && !bToggleCrouch)
	{
		return;
	}

	bToggleADS = false;
	bToggleCrouch = false;
	OnInputModesChanged.Broadcast();
}

void UShooterGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();
	ScreenBrightness = ShooterGameUserSettingsConstants::NormalizeScreenBrightness(ScreenBrightness);

	MouseHorizontalSensitivity = FMath::Clamp(
		MouseHorizontalSensitivity,
		ShooterGameUserSettingsConstants::MinMouseSensitivity,
		ShooterGameUserSettingsConstants::MaxMouseSensitivity);
	MouseVerticalSensitivity = FMath::Clamp(
		MouseVerticalSensitivity,
		ShooterGameUserSettingsConstants::MinMouseSensitivity,
		ShooterGameUserSettingsConstants::MaxMouseSensitivity);
	AimSensitivityMultiplier = FMath::Clamp(
		AimSensitivityMultiplier,
		ShooterGameUserSettingsConstants::MinMouseSensitivity,
		ShooterGameUserSettingsConstants::MaxMouseSensitivity);

	MasterVolume = FMath::Clamp(MasterVolume, ShooterGameUserSettingsConstants::MinAudioVolume, ShooterGameUserSettingsConstants::MaxAudioVolume);
	MusicVolume = FMath::Clamp(MusicVolume, ShooterGameUserSettingsConstants::MinAudioVolume, ShooterGameUserSettingsConstants::MaxAudioVolume);
	SoundEffectsVolume = FMath::Clamp(SoundEffectsVolume, ShooterGameUserSettingsConstants::MinAudioVolume, ShooterGameUserSettingsConstants::MaxAudioVolume);
	UserInterfaceVolume = FMath::Clamp(UserInterfaceVolume, ShooterGameUserSettingsConstants::MinAudioVolume, ShooterGameUserSettingsConstants::MaxAudioVolume);
	VoiceVolume = FMath::Clamp(VoiceVolume, ShooterGameUserSettingsConstants::MinAudioVolume, ShooterGameUserSettingsConstants::MaxAudioVolume);
	VoiceChatMicrophoneVolume = FMath::Clamp(VoiceChatMicrophoneVolume, ShooterGameUserSettingsConstants::MinAudioVolume, ShooterGameUserSettingsConstants::MaxAudioVolume);
	VoiceChatOutputVolume = FMath::Clamp(VoiceChatOutputVolume, ShooterGameUserSettingsConstants::MinAudioVolume, ShooterGameUserSettingsConstants::MaxAudioVolume);

	const FIntPoint CurrentResolution = GetScreenResolution();
	const FIntPoint MaxResolution = GetMaxSupportedResolution();
	if (CurrentResolution.X <= 0 || CurrentResolution.Y <= 0
		|| CurrentResolution.X > MaxResolution.X
		|| CurrentResolution.Y > MaxResolution.Y)
	{
		SetScreenResolution(MaxResolution);
	}

	const float CurrentFrameRateLimit = GetFrameRateLimit();
	SetFrameRateLimit(NormalizeFrameRateLimit(CurrentFrameRateLimit));

	CrosshairThickness = FMath::Clamp(CrosshairThickness, -1.0f, 0.0f);
	CrosshairGap = FMath::Clamp(CrosshairGap, -1.0f, 0.0f);
	CrosshairOutlineThickness = FMath::Clamp(CrosshairOutlineThickness, -1.0f, 1.0f);
}


void UShooterGameUserSettings::ApplyNonResolutionSettings()
{
	const float CurrentFrameRateLimit = GetFrameRateLimit();
	const float EffectiveFrameRateLimit =
		NormalizeFrameRateLimit(CurrentFrameRateLimit);

	if (!FMath::IsNearlyEqual(CurrentFrameRateLimit, EffectiveFrameRateLimit))
	{
		SetFrameRateLimit(EffectiveFrameRateLimit);
	}

	Super::ApplyNonResolutionSettings();
	ApplyScreenBrightness();

	// 다른 초기화 경로에서 t.MaxFPS가 바뀌어도 옵션의 프레임 제한을 최종값으로 맞춘다.
	if (IConsoleVariable* MaxFPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		MaxFPSCVar->Set(EffectiveFrameRateLimit, ECVF_SetByGameSetting);
	}
}

void UShooterGameUserSettings::SetScreenBrightness(const float Brightness)
{
	ScreenBrightness = ShooterGameUserSettingsConstants::NormalizeScreenBrightness(Brightness);
	ApplyScreenBrightness();
}

void UShooterGameUserSettings::ApplyScreenBrightness() const
{
	if (!GEngine || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	const float Alpha = (ScreenBrightness - GetMinScreenBrightness())
		/ (GetMaxScreenBrightness() - GetMinScreenBrightness());
	// 밝기 50은 기본 감마 2.2이며 값이 커질수록 화면이 밝아진다.
	GEngine->DisplayGamma = FMath::Lerp(
		ShooterGameUserSettingsConstants::MinDisplayGamma,
		ShooterGameUserSettingsConstants::MaxDisplayGamma,
		Alpha);
}

void UShooterGameUserSettings::SetDynamicCrosshairEnabled(const bool bEnabled)
{
	if (bDynamicCrosshair == bEnabled)
	{
		return;
	}

	bDynamicCrosshair = bEnabled;
	// UShooterReticle이 Tick 없이 이 이벤트로 크로스헤어 간격을 다시 계산한다.
	OnCrosshairSettingsChanged.Broadcast();
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
		ShooterGameUserSettingsConstants::MinMouseSensitivity,
		ShooterGameUserSettingsConstants::MaxMouseSensitivity);
}

void UShooterGameUserSettings::SetMouseVerticalSensitivity(const float Sensitivity)
{
	MouseVerticalSensitivity = FMath::Clamp(
		Sensitivity,
		ShooterGameUserSettingsConstants::MinMouseSensitivity,
		ShooterGameUserSettingsConstants::MaxMouseSensitivity);
}

void UShooterGameUserSettings::SetAimSensitivityMultiplier(const float Multiplier)
{
	AimSensitivityMultiplier = FMath::Clamp(
		Multiplier,
		ShooterGameUserSettingsConstants::MinMouseSensitivity,
		ShooterGameUserSettingsConstants::MaxMouseSensitivity);
}

void UShooterGameUserSettings::SetInvertVerticalLookEnabled(const bool bEnabled)
{
	bInvertVerticalLook = bEnabled;
}

FVector2D UShooterGameUserSettings::ApplyLookSensitivity(
	const FVector2D RawLookInput,
	const bool bAiming) const
{
	const float VerticalDirection = bInvertVerticalLook ? 1.0f : -1.0f;
	FVector2D Result(
		RawLookInput.X * MouseHorizontalSensitivity,
		RawLookInput.Y * MouseVerticalSensitivity * VerticalDirection);

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
		ShooterGameUserSettingsConstants::MinAudioVolume,
		ShooterGameUserSettingsConstants::MaxAudioVolume);
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

void UShooterGameUserSettings::SetVoiceChatMicrophoneVolume(const float Volume)
{
	const float Clamped = FMath::Clamp(
		Volume,
		ShooterGameUserSettingsConstants::MinAudioVolume,
		ShooterGameUserSettingsConstants::MaxAudioVolume);
	if (FMath::IsNearlyEqual(VoiceChatMicrophoneVolume, Clamped))
	{
		return;
	}

	VoiceChatMicrophoneVolume = Clamped;
	OnAudioSettingsChanged.Broadcast();
}

void UShooterGameUserSettings::SetVoiceChatOutputVolume(const float Volume)
{
	const float Clamped = FMath::Clamp(
		Volume,
		ShooterGameUserSettingsConstants::MinAudioVolume,
		ShooterGameUserSettingsConstants::MaxAudioVolume);
	if (FMath::IsNearlyEqual(VoiceChatOutputVolume, Clamped))
	{
		return;
	}

	VoiceChatOutputVolume = Clamped;
	OnAudioSettingsChanged.Broadcast();
}

FName UShooterGameUserSettings::GetKeyBindingOverride(const FName BindingId) const
{
	if (const FName* Found = KeyBindingOverrides.Find(BindingId))
	{
		return *Found;
	}
	return NAME_None;
}

void UShooterGameUserSettings::SetKeyBindingOverride(const FName BindingId, const FName KeyName)
{
	if (BindingId.IsNone())
	{
		return;
	}

	if (KeyName.IsNone())
	{
		KeyBindingOverrides.Remove(BindingId);
	}
	else
	{
		KeyBindingOverrides.Add(BindingId, KeyName);
	}

	OnInputBindingsChanged.Broadcast();
}

void UShooterGameUserSettings::ResetKeyBindingOverrides()
{
	if (KeyBindingOverrides.IsEmpty())
	{
		return;
	}

	KeyBindingOverrides.Reset();
	OnInputBindingsChanged.Broadcast();
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
