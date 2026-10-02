#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DominationSandstormSettings.generated.h"

class UDominationSandstormWarningWidget;
class UNiagaraSystem;

UCLASS(BlueprintType)
class FPS_API UDominationSandstormSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float InitialDelayMin = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float InitialDelayMax = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float RepeatDelayMin = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float RepeatDelayMax = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float WarningDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float FadeInDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float PeakDurationMin = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float PeakDurationMax = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float FadeOutDuration = 3.0f;

	// FadeIn부터 FadeOut 종료까지 적용한다. 0이면 폭풍 중 적 윤곽선을 모두 숨긴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Outline",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "50.0", Units = "m",
			ToolTip = "Maximum enemy outline distance during FadeIn, Peak and FadeOut. Applies to characters, attached weapons and enemy deployables, including revealed enemies. Allies are unchanged. 0 hides all enemy outlines."))
	float EnemyOutlineVisibleDistanceMeters = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Sandstorm|Outline",
		meta = (ClampMin = "0.02", ClampMax = "0.25", Units = "s",
			ToolTip = "Local distance refresh interval, only while a storm is active. Lower values respond faster to movement; no new per-frame Tick is used."))
	float EnemyOutlineUpdateInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual")
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	// 기존 맵의 맑은 날씨 값을 시작점으로 쓰고 종료 시 그대로 복구한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Fog")
	bool bUseMapFogBaseline = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Fog", meta = (ClampMin = "0.0"))
	float NormalFogDensity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Fog", meta = (ClampMin = "0.0", UIMax = "5.0"))
	float PeakFogDensity = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Fog", meta = (ClampMin = "0.0", Units = "cm"))
	float NormalFogStartDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Fog", meta = (ClampMin = "0.0", Units = "cm"))
	float PeakFogStartDistance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Fog", meta = (ClampMin = "0.001", ClampMax = "2.0"))
	float PeakFogHeightFalloff = 0.001f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Fog", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PeakFogMaxOpacity = 0.99f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Fog")
	bool bEnableVolumetricFog = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "0.0", ClampMax = "4800.0", UIMax = "4800.0"))
	float NearParticleSpawnRate = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "0.0", ClampMax = "144.0", UIMax = "144.0"))
	float MidParticleSpawnRate = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "0.0", ClampMax = "32.0", UIMax = "32.0"))
	float DustParticleSpawnRate = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "400.0", ClampMax = "6000.0", Units = "cm"))
	float NearParticleRadius = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "400.0", ClampMax = "6000.0", Units = "cm"))
	float MidParticleRadius = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "400.0", ClampMax = "6000.0", Units = "cm"))
	float DustParticleRadius = 3600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "1000.0", Units = "cm"))
	float ParticleBoundsPadding = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMax = "2.0"))
	float NearSpawnMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMax = "2.0"))
	float MidSpawnMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Particles", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMax = "2.0"))
	float DustSpawnMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Wind")
	FVector WindDirection = FVector(1.0, 0.25, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Wind", meta = (ClampMin = "0.0", ClampMax = "6000.0", Units = "cm/s"))
	float WindSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Wind", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float MidWindSpeedMultiplier = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Wind", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float DustWindSpeedMultiplier = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Wind", meta = (ClampMin = "0.0", ClampMax = "1500.0"))
	float TurbulenceStrength = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Color")
	FLinearColor SandTint = FLinearColor(0.38f, 0.32f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Color", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Desaturation = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Color", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float ContrastAdjustment = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Visual|Color", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PostProcessTintStrength = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|HUD")
	TSubclassOf<UDominationSandstormWarningWidget> WarningWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Debug", meta = (ToolTip = "Development builds only. Begin warning when the actual match flow starts."))
	bool bStartImmediately = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Debug", meta = (ToolTip = "Development builds only. Disable automatic scheduling; ForceStorm can still start a cycle."))
	bool bDisableStormScheduling = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandstorm|Debug", meta = (ClampMin = "-1.0", ClampMax = "1.0", ToolTip = "Development builds only. -1 uses replicated state; 0..1 overrides the local visual and wind intensity."))
	float PreviewIntensity = -1.0f;

	FVector GetNormalizedWindDirection() const;
};
