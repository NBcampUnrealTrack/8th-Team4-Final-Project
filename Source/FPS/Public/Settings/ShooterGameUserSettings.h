#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ShooterGameUserSettings.generated.h"

UENUM(BlueprintType)
enum class EShooterAudioChannel : uint8
{
	Master UMETA(DisplayName = "Master"),
	Music UMETA(DisplayName = "Music"),
	SoundEffects UMETA(DisplayName = "Sound Effects"),
	UserInterface UMETA(DisplayName = "User Interface"),
	Voice UMETA(DisplayName = "Voice")
};

DECLARE_MULTICAST_DELEGATE(FOnShooterAudioSettingsChanged);

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

	// The local runtime applies audio changes independently of any options widget.
	FOnShooterAudioSettingsChanged OnAudioSettingsChanged;

	UFUNCTION(BlueprintPure, Category = "FPS|Settings|Gameplay")
	bool IsDynamicCrosshairEnabled() const { return bDynamicCrosshair; }

	UFUNCTION(BlueprintCallable, Category = "FPS|Settings|Gameplay")
	void SetDynamicCrosshairEnabled(bool bEnabled);

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

protected:
	UPROPERTY(Config)
	bool bDynamicCrosshair = true;

	UPROPERTY(Config, meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float MouseHorizontalSensitivity = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float MouseVerticalSensitivity = 1.0f;

	UPROPERTY(Config, meta = (ClampMin = "0.01", ClampMax = "2.0"))
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
};
