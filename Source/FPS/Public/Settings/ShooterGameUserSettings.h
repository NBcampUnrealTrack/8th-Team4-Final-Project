#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ShooterGameUserSettings.generated.h"

class USoundClass;
class USoundMix;

UENUM(BlueprintType)
enum class EShooterAudioChannel : uint8
{
	Master UMETA(DisplayName = "Master"),
	Music UMETA(DisplayName = "Music"),
	SoundEffects UMETA(DisplayName = "Sound Effects"),
	UserInterface UMETA(DisplayName = "User Interface"),
	Voice UMETA(DisplayName = "Voice")
};

UENUM(BlueprintType)
enum class EShooterOptionsLanguage : uint8
{
	Korean UMETA(DisplayName = "한국어"),
	English UMETA(DisplayName = "English")
};

UENUM(BlueprintType)
enum class EShooterExperienceProfile : uint8
{
	Visual60 UMETA(DisplayName = "Visual 60"),
	Balanced120 UMETA(DisplayName = "Balanced 120"),
	Competitive144 UMETA(DisplayName = "Competitive 144"),
	MaxQuality UMETA(DisplayName = "Max Quality")
};

DECLARE_MULTICAST_DELEGATE(FOnShooterAudioSettingsChanged);

// 크로스헤어 커스터마이징이 바뀔 때마다 발신. UShooterReticle이 구독해서 Dynamic Material
// Instance 파라미터를 즉시 다시 적용한다(설정 화면에서 슬라이더 움직이는 동안 바로 미리보기됨).
DECLARE_MULTICAST_DELEGATE(FOnShooterCrosshairSettingsChanged);

/** Setters change session values; the caller chooses when to SaveSettings. */
UCLASS(Config = GameUserSettings)
class FPS_API UShooterGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FPS|Settings")
	static UShooterGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void ValidateSettings() override;
	virtual void ApplyNonResolutionSettings() override;

	// The local runtime applies audio changes independently of any options widget.
	FOnShooterAudioSettingsChanged OnAudioSettingsChanged;

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Gameplay")
	bool IsDynamicCrosshairEnabled() const { return bDynamicCrosshair; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Gameplay")
	void SetDynamicCrosshairEnabled(bool bEnabled);

	// 크로스헤어 외형(M_UI_Base_ReticleBuilder 계열 머티리얼) 커스터마이징. 설정 화면에서
	// 슬라이더/컬러피커가 바뀔 때마다 호출되고, UShooterReticle이 OnCrosshairSettingsChanged로
	// 구독해서 즉시 미리보기에 반영한다.
	FOnShooterCrosshairSettingsChanged OnCrosshairSettingsChanged;

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Crosshair")
	FLinearColor GetCrosshairColor() const { return CrosshairColor; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Crosshair")
	void SetCrosshairColor(FLinearColor NewColor);

	// 십자선 두께. 머티리얼의 CrossHairThickness 파라미터를 그대로 받는다(음수일수록 두꺼움).
	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Crosshair")
	float GetCrosshairThickness() const { return CrosshairThickness; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Crosshair")
	void SetCrosshairThickness(float NewThickness);

	// 중앙에서 십자선까지의 간격. 머티리얼의 CrossHair_InnerEdge 파라미터를 그대로 받는다.
	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Crosshair")
	float GetCrosshairGap() const { return CrosshairGap; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Crosshair")
	void SetCrosshairGap(float NewGap);

	// 십자선 테두리(아웃라인) 두께. 0이면 테두리 없음.
	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Crosshair")
	float GetCrosshairOutlineThickness() const { return CrosshairOutlineThickness; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Crosshair")
	void SetCrosshairOutlineThickness(float NewThickness);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Input")
	float GetMouseHorizontalSensitivity() const { return MouseHorizontalSensitivity; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Input")
	void SetMouseHorizontalSensitivity(float Sensitivity);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Input")
	float GetMouseVerticalSensitivity() const { return MouseVerticalSensitivity; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Input")
	void SetMouseVerticalSensitivity(float Sensitivity);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Input")
	float GetAimSensitivityMultiplier() const { return AimSensitivityMultiplier; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Input")
	void SetAimSensitivityMultiplier(float Multiplier);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Input")
	bool IsInvertVerticalLookEnabled() const { return bInvertVerticalLook; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Input")
	void SetInvertVerticalLookEnabled(bool bEnabled);

	/** Applies the saved X/Y sensitivity, invert-Y and ADS multiplier in one place. */
	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Input")
	FVector2D ApplyLookSensitivity(FVector2D RawLookInput, bool bAiming) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Audio")
	float GetAudioVolume(EShooterAudioChannel Channel) const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Audio")
	void SetAudioVolume(EShooterAudioChannel Channel, float Volume);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Audio")
	bool IsMasterAudioMuted() const { return bMasterAudioMuted; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Audio")
	void SetMasterAudioMuted(bool bMuted);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Audio")
	bool IsVoiceChatEnabled() const { return bVoiceChatEnabled; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Audio")
	void SetVoiceChatEnabled(bool bEnabled);

	USoundMix* GetUserSettingsSoundMix();
	USoundClass* GetAudioChannelSoundClass(EShooterAudioChannel Channel);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|General")
	EShooterOptionsLanguage GetLanguage() const { return Language; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|General")
	void SetLanguage(EShooterOptionsLanguage NewLanguage);

	/** Applies the selected culture immediately. */
	void ApplyLanguageCulture() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Smart Tuning")
	EShooterExperienceProfile GetExperienceProfile() const { return ExperienceProfile; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Smart Tuning")
	void SetExperienceProfile(EShooterExperienceProfile NewProfile) { ExperienceProfile = NewProfile; }

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Smart Tuning")
	float GetPerformanceTargetFPS() const { return PerformanceTargetFPS; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Smart Tuning")
	void SetPerformanceTargetFPS(float TargetFPS) { PerformanceTargetFPS = FMath::Clamp(TargetFPS, 30.0f, 240.0f); }

protected:
	UPROPERTY(Config)
	bool bDynamicCrosshair = true;

	// M_UI_Base_ReticleBuilder 계열 머티리얼 파라미터를 그대로 저장한다. 기본값은
	// MI_UI_Reticles_BaseInstance의 기본값과 맞춰뒀다.
	UPROPERTY(Config)
	FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.4f);

	UPROPERTY(Config, meta = (ClampMin = "-1.0", ClampMax = "0.0"))
	float CrosshairThickness = -0.5f;

	UPROPERTY(Config, meta = (ClampMin = "-1.0", ClampMax = "0.0"))
	float CrosshairGap = -0.25f;

	UPROPERTY(Config, meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float CrosshairOutlineThickness = 0.f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MouseHorizontalSensitivity = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MouseVerticalSensitivity = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimSensitivityMultiplier = 1.0f;

	UPROPERTY(Config)
	bool bInvertVerticalLook = false;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterVolume = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MusicVolume = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SoundEffectsVolume = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UserInterfaceVolume = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VoiceVolume = 1.0f;

	UPROPERTY(Config)
	bool bMasterAudioMuted = false;

	UPROPERTY(Config)
	bool bVoiceChatEnabled = true;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> CachedUserSettingsSoundMix;

	UPROPERTY(Transient)
	TMap<EShooterAudioChannel, TObjectPtr<USoundClass>> CachedAudioChannelSoundClasses;


	UPROPERTY(Config)
	EShooterOptionsLanguage Language = EShooterOptionsLanguage::Korean;

	UPROPERTY(Config)
	EShooterExperienceProfile ExperienceProfile = EShooterExperienceProfile::Balanced120;

	UPROPERTY(Config, meta = (ClampMin = "30.0", ClampMax = "240.0"))
	float PerformanceTargetFPS = 120.0f;
};
