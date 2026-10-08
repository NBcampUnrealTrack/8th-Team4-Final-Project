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


DECLARE_MULTICAST_DELEGATE(FOnShooterAudioSettingsChanged);
DECLARE_MULTICAST_DELEGATE(FOnShooterInputBindingsChanged);
DECLARE_MULTICAST_DELEGATE(FOnShooterInputModesChanged);

// 크로스헤어 설정이 바뀌면 게임 HUD와 옵션 미리보기를 즉시 갱신한다.
DECLARE_MULTICAST_DELEGATE(FOnShooterCrosshairSettingsChanged);

/** Setters change session values; the caller chooses when to SaveSettings. */
UCLASS(Config = GameUserSettings)
class FPS_API UShooterGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UShooterGameUserSettings(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings")
	static UShooterGameUserSettings* Get();

	virtual void SetToDefaults() override;
	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void ValidateSettings() override;
	virtual void ApplyNonResolutionSettings() override;

	// 런타임 검증과 옵션 UI가 같은 제한값을 사용한다.
	static FIntPoint GetMaxSupportedResolution();
	static int32 GetFrameRateLimitChoiceCount();
	static float GetFrameRateLimitChoice(int32 Index);
	static int32 FindFrameRateLimitChoiceIndex(float FramesPerSecond);
	static float NormalizeFrameRateLimit(float FramesPerSecond);
	static float GetDefaultFrameRateLimit();
	static float GetMinMouseSensitivity();
	static float GetMaxMouseSensitivity();
	static float GetMinAudioVolume();
	static float GetMaxAudioVolume();
	static int32 GetDefaultGraphicsQualityLevel();
	static float GetMinScreenBrightness();
	static float GetMaxScreenBrightness();
	static float GetDefaultScreenBrightness();

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Display")
	float GetScreenBrightness() const { return ScreenBrightness; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Display")
	void SetScreenBrightness(float Brightness);

	// The local runtime applies audio changes independently of any options widget.
	FOnShooterAudioSettingsChanged OnAudioSettingsChanged;
	FOnShooterInputBindingsChanged OnInputBindingsChanged;
	FOnShooterInputModesChanged OnInputModesChanged;

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Gameplay")
	bool IsDynamicCrosshairEnabled() const { return bDynamicCrosshair; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Gameplay")
	void SetDynamicCrosshairEnabled(bool bEnabled);

	// 색상/동적 크로스헤어 변경은 Tick 없이 이 이벤트로 반영한다.
	FOnShooterCrosshairSettingsChanged OnCrosshairSettingsChanged;

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Crosshair")
	FLinearColor GetCrosshairColor() const;

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

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Input")
	bool IsADSToggleEnabled() const { return bToggleADS; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Input")
	void SetADSToggleEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Input")
	bool IsCrouchToggleEnabled() const { return bToggleCrouch; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Input")
	void SetCrouchToggleEnabled(bool bEnabled);

	void ResetInputModes();

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

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Voice Chat")
	float GetVoiceChatMicrophoneVolume() const { return VoiceChatMicrophoneVolume; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Voice Chat")
	void SetVoiceChatMicrophoneVolume(float Volume);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Voice Chat")
	float GetVoiceChatOutputVolume() const { return VoiceChatOutputVolume; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Voice Chat")
	void SetVoiceChatOutputVolume(float Volume);

	FName GetKeyBindingOverride(FName BindingId) const;
	void SetKeyBindingOverride(FName BindingId, FName KeyName);
	void ResetKeyBindingOverrides();

	USoundMix* GetUserSettingsSoundMix();
	USoundClass* GetAudioChannelSoundClass(EShooterAudioChannel Channel);

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|General")
	EShooterOptionsLanguage GetLanguage() const { return Language; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|General")
	void SetLanguage(EShooterOptionsLanguage NewLanguage);

	/** Applies the selected culture immediately. */
	void ApplyLanguageCulture() const;


protected:
	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float ScreenBrightness = GetDefaultScreenBrightness();

	UPROPERTY(Config)
	bool bDynamicCrosshair = true;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FLinearColor CrosshairColor = FLinearColor::White;

	UPROPERTY(Config, meta = (ClampMin = "-1.0", ClampMax = "0.0"))
	float CrosshairThickness = -0.5f;

	UPROPERTY(Config, meta = (ClampMin = "-1.0", ClampMax = "0.0"))
	float CrosshairGap = -0.25f;

	UPROPERTY(Config, meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float CrosshairOutlineThickness = 0.f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MouseHorizontalSensitivity = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MouseVerticalSensitivity = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float AimSensitivityMultiplier = 1.0f;

	UPROPERTY(Config)
	bool bInvertVerticalLook = false;

	UPROPERTY(Config)
	bool bToggleADS = false;

	UPROPERTY(Config)
	bool bToggleCrouch = false;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterVolume = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MusicVolume = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SoundEffectsVolume = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float UserInterfaceVolume = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VoiceVolume = 1.0f;

	UPROPERTY(Config)
	bool bMasterAudioMuted = false;

	UPROPERTY(Config)
	bool bVoiceChatEnabled = true;

	// Outgoing voice level. Steam captures compressed voice, so this gain is applied after
	// decode on receivers and routed with each voice packet. 0 mutes outgoing voice.
	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VoiceChatMicrophoneVolume = 1.0f;

	// Local playback level for other players. Kept separate from the general Voice channel
	// so announcer/dialogue volume does not control voice chat.
	UPROPERTY(Config, meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VoiceChatOutputVolume = 1.0f;

	// BindingId -> FKey name. IDs are generated from the original mapping context and
	// never mutate the authored IMC asset.
	UPROPERTY(Config)
	TMap<FName, FName> KeyBindingOverrides;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> CachedUserSettingsSoundMix;

	UPROPERTY(Transient)
	TMap<EShooterAudioChannel, TObjectPtr<USoundClass>> CachedAudioChannelSoundClasses;


	UPROPERTY(Config)
	EShooterOptionsLanguage Language = EShooterOptionsLanguage::Korean;

private:
	void ApplyScreenBrightness() const;
};
