#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/ShooterGameUserSettings.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterOptionsScreen.generated.h"

class UButton;
class USlider;
class UTextBlock;
class UWidgetSwitcher;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;


/**
 * 옵션 UI에서 표시할 창 모드.
 * 엔진의 EWindowMode::Type을 Blueprint에서 다루기 쉬운 형태로 노출한다.
 */
UENUM(BlueprintType)
enum class EShooterWindowMode : uint8
{
	Fullscreen UMETA(DisplayName = "Fullscreen"),
	WindowedFullscreen UMETA(DisplayName = "Windowed Fullscreen"),
	Windowed UMETA(DisplayName = "Windowed")
};

/**
 * UGameUserSettings가 기본 제공하는 Scalability 항목.
 */
UENUM(BlueprintType)
enum class EShooterScalabilitySetting : uint8
{
	ViewDistance UMETA(DisplayName = "View Distance"),
	AntiAliasing UMETA(DisplayName = "Anti Aliasing"),
	Shadow UMETA(DisplayName = "Shadow"),
	GlobalIllumination UMETA(DisplayName = "Global Illumination"),
	Reflection UMETA(DisplayName = "Reflection"),
	PostProcessing UMETA(DisplayName = "Post Processing"),
	Texture UMETA(DisplayName = "Texture"),
	Effects UMETA(DisplayName = "Effects"),
	Foliage UMETA(DisplayName = "Foliage"),
	Shading UMETA(DisplayName = "Shading")
};

DECLARE_MULTICAST_DELEGATE(FOnShooterOptionsAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShooterSettingsChanged);

/**
 * CommonUI 옵션 화면의 C++ 기반 클래스.
 *
 * 화면 표시/입력만 담당하며 실제 값은 프로젝트에서 이미 사용 중인
 * UShooterGameUserSettings(UGameUserSettings)에 위임한다.
 * 일반 옵션은 즉시 적용하고 SaveOptions/닫기에서 저장한다.
 * 해상도와 창 모드는 확인한 값만 설정 객체에 반영한다.
 */
UCLASS(Abstract)
class FPS_API UShooterOptionsScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UShooterOptionsScreen();

	// TeamMatchUIInitComponent가 이 요청을 받아 이전 인게임 메뉴로 복귀시킨다.
	UFUNCTION(BlueprintCallable, Category = "UI|Options")
	void RequestClose();

	// 현재 설정을 저장한다. 확인 대기 중인 화면 모드는 포함하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "UI|Options")
	void SaveOptions();

	// 화면 소유자가 제거 직전에 호출한다. 미확정 화면 복구 후 일반 옵션을 저장한다.
	void FinishEditing();

	FOnShooterOptionsAction OnCloseRequested;

	UPROPERTY(BlueprintAssignable, Category = "UI|Options")
	FOnShooterSettingsChanged OnSettingsChanged;

	// ----- Video -----
	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	TArray<FIntPoint> GetSupportedResolutions() const;

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	FIntPoint GetScreenResolution() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void SetScreenResolution(FIntPoint Resolution);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	EShooterWindowMode GetWindowMode() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void SetWindowMode(EShooterWindowMode WindowMode);

	// Applies a resolution/window mode temporarily until confirmed by the UI.
	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void PreviewVideoMode(FIntPoint Resolution, EShooterWindowMode WindowMode);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	bool IsVideoModeConfirmationPending() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void ConfirmVideoMode();

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void RevertVideoMode();

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	float GetVideoModeConfirmationTimeoutSeconds() const
	{
		return VideoModeConfirmationTimeoutSeconds;
	}

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	float GetResolutionScalePercent() const;

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	void GetResolutionScaleRange(float& MinPercent, float& MaxPercent) const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void SetResolutionScalePercent(float Percent);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	bool IsVSyncEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void SetVSyncEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	float GetFrameRateLimit() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void SetFrameRateLimit(float FramesPerSecond);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Video")
	bool IsDynamicResolutionEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void SetDynamicResolutionEnabled(bool bEnabled);

	// ----- Graphics / Scalability -----
	// -1이면 개별 옵션이 달라져 Overall이 Custom 상태라는 뜻이다.
	UFUNCTION(BlueprintPure, Category = "UI|Options|Graphics")
	int32 GetOverallQualityLevel() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Graphics")
	void SetOverallQualityLevel(int32 QualityLevel);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Graphics")
	int32 GetQualityLevel(EShooterScalabilitySetting Setting) const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Graphics")
	void SetQualityLevel(EShooterScalabilitySetting Setting, int32 QualityLevel);

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Graphics")
	void RunHardwareBenchmarkAndApply(int32 WorkScale = 10);

	// ----- Controls / Gameplay -----
	UFUNCTION(BlueprintPure, Category = "UI|Options|Controls")
	float GetMouseHorizontalSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Controls")
	void SetMouseHorizontalSensitivity(float Sensitivity);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Controls")
	float GetMouseVerticalSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Controls")
	void SetMouseVerticalSensitivity(float Sensitivity);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Controls")
	float GetAimSensitivityMultiplier() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Controls")
	void SetAimSensitivityMultiplier(float Multiplier);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Controls")
	bool IsInvertVerticalLookEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Controls")
	void SetInvertVerticalLookEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Gameplay")
	bool IsDynamicCrosshairEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Gameplay")
	void SetDynamicCrosshairEnabled(bool bEnabled);

	// ----- Audio -----
	UFUNCTION(BlueprintPure, Category = "UI|Options|Audio")
	float GetAudioVolume(EShooterAudioChannel Channel) const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Audio")
	void SetAudioVolume(EShooterAudioChannel Channel, float Volume);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Audio")
	bool IsMasterAudioMuted() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Audio")
	void SetMasterAudioMuted(bool bMuted);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Audio")
	bool IsVoiceChatEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Audio")
	void SetVoiceChatEnabled(bool bEnabled);

	// 현재 해상도/창 모드는 유지하고 엔진 Video/Graphics 옵션을 초기화한다.
	UFUNCTION(BlueprintCallable, Category = "UI|Options")
	void ResetEngineSettingsToDefaults();

	UFUNCTION(BlueprintCallable, Category = "UI|Options")
	void ResetAllSettingsToDefaults();

	// ----- UI formatting helpers -----
	UFUNCTION(BlueprintPure, Category = "UI|Options|Formatting")
	static FText FormatResolution(FIntPoint Resolution);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Formatting")
	static FText GetWindowModeDisplayText(EShooterWindowMode WindowMode);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Formatting")
	static FText GetQualityDisplayText(int32 QualityLevel);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Formatting")
	static FText GetFrameRateLimitDisplayText(float FramesPerSecond);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual void NativeDestruct() override;

private:

	// Generated WBP support. The editor generator creates widgets with these fixed names,
	// and this screen owns their UI-only bindings while settings stay in UGameUserSettings.
	void BindGeneratedControls();
	UFUNCTION() void RefreshGeneratedUI();
	void ShowGeneratedPage(int32 PageIndex);
	void RefreshGeneratedTabVisuals();
	void RefreshGeneratedOptionRowVisuals();
	void RefreshGeneratedDetailPanel();
	void RefreshGeneratedPreview();
	void ReleaseGeneratedPreviewCapture();
	void SetGeneratedPerformanceSegments(int32 ActiveSegments, bool bRelevant) const;
	void SetGeneratedChoiceSegments(FName Prefix, int32 ActiveIndex, int32 ChoiceCount) const;
	void SetGeneratedButtonTextColor(FName ButtonName, const FLinearColor& Color) const;
	void SetGeneratedWindowModeVisual() const;
	void StepResolution(int32 Delta);
	void StepWindowMode(int32 Delta);
	void StepFrameRate(int32 Delta);
	void StepOverallQuality(int32 Delta);
	void StepQuality(EShooterScalabilitySetting Setting, int32 Delta);
	void SetGeneratedText(FName WidgetName, const FText& Text) const;
	void SetGeneratedToggleVisual(FName ButtonName, FName TextName, bool bEnabled) const;

	UFUNCTION() void HandleSideGraphics();
	UFUNCTION() void HandleSideDisplay();
	UFUNCTION() void HandleSideControls();
	UFUNCTION() void HandleSideAudio();
	UFUNCTION() void HandleSave();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleReset();
	UFUNCTION() void HandleVideoKeep();
	UFUNCTION() void HandleVideoRevert();

	UFUNCTION() void HandleResolutionPrev();
	UFUNCTION() void HandleResolutionNext();
	UFUNCTION() void HandleWindowModePrev();
	UFUNCTION() void HandleWindowModeNext();
	UFUNCTION() void HandleWindowModeFullscreen();
	UFUNCTION() void HandleWindowModeWindowed();
	UFUNCTION() void HandleFrameRatePrev();
	UFUNCTION() void HandleFrameRateNext();
	UFUNCTION() void HandleOverallPrev();
	UFUNCTION() void HandleOverallNext();
	UFUNCTION() void HandleViewDistancePrev();
	UFUNCTION() void HandleViewDistanceNext();
	UFUNCTION() void HandleAntiAliasingPrev();
	UFUNCTION() void HandleAntiAliasingNext();
	UFUNCTION() void HandleShadowPrev();
	UFUNCTION() void HandleShadowNext();
	UFUNCTION() void HandleGlobalIlluminationPrev();
	UFUNCTION() void HandleGlobalIlluminationNext();
	UFUNCTION() void HandleReflectionPrev();
	UFUNCTION() void HandleReflectionNext();
	UFUNCTION() void HandlePostProcessingPrev();
	UFUNCTION() void HandlePostProcessingNext();
	UFUNCTION() void HandleTexturePrev();
	UFUNCTION() void HandleTextureNext();
	UFUNCTION() void HandleEffectsPrev();
	UFUNCTION() void HandleEffectsNext();
	UFUNCTION() void HandleFoliagePrev();
	UFUNCTION() void HandleFoliageNext();
	UFUNCTION() void HandleShadingPrev();
	UFUNCTION() void HandleShadingNext();

	// Hover/click on selector labels updates the contextual detail panel without changing the value.
	UFUNCTION() void HandleInfoResolution();
	UFUNCTION() void HandleInfoWindowMode();
	UFUNCTION() void HandleInfoFrameRate();
	UFUNCTION() void HandleInfoOverall();
	UFUNCTION() void HandleInfoViewDistance();
	UFUNCTION() void HandleInfoAntiAliasing();
	UFUNCTION() void HandleInfoShadow();
	UFUNCTION() void HandleInfoGlobalIllumination();
	UFUNCTION() void HandleInfoReflection();
	UFUNCTION() void HandleInfoPostProcessing();
	UFUNCTION() void HandleInfoTexture();
	UFUNCTION() void HandleInfoEffects();
	UFUNCTION() void HandleInfoFoliage();
	UFUNCTION() void HandleInfoShading();

	UFUNCTION() void HandleVSyncToggle();
	UFUNCTION() void HandleDynamicResolutionToggle();
	UFUNCTION() void HandleInvertYToggle();
	UFUNCTION() void HandleDynamicCrosshairToggle();
	UFUNCTION() void HandleMasterMuteToggle();
	UFUNCTION() void HandleVoiceChatToggle();

	UFUNCTION() void HandleResolutionScaleChanged(float Value);
	UFUNCTION() void HandleHorizontalSensitivityChanged(float Value);
	UFUNCTION() void HandleVerticalSensitivityChanged(float Value);
	UFUNCTION() void HandleAimSensitivityChanged(float Value);
	UFUNCTION() void HandleMasterVolumeChanged(float Value);
	UFUNCTION() void HandleMusicVolumeChanged(float Value);
	UFUNCTION() void HandleSfxVolumeChanged(float Value);
	UFUNCTION() void HandleUiVolumeChanged(float Value);
	UFUNCTION() void HandleVoiceVolumeChanged(float Value);

	UShooterGameUserSettings* GetGameUserSettings() const;
	void RevertVideoModeInternal(bool bNotifySettingsChanged);
	void StartVideoModeConfirmationTimeout();
	void ClearVideoModeConfirmationTimeout();
	bool HandleVideoModeConfirmationTimeout(float DeltaTime);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Video", meta = (ClampMin = "1.0"))
	float VideoModeConfirmationTimeoutSeconds = 15.0f;

	FTSTicker::FDelegateHandle VideoModeConfirmationTickerHandle;
	bool bVideoModeConfirmationPending = false;
	FIntPoint PendingResolution = FIntPoint::ZeroValue;
	EShooterWindowMode PendingWindowMode = EShooterWindowMode::Windowed;
	FIntPoint VideoModeBaselineResolution = FIntPoint::ZeroValue;
	EShooterWindowMode VideoModeBaselineWindowMode = EShooterWindowMode::Windowed;

	static EShooterWindowMode FromEngineWindowMode(EWindowMode::Type WindowMode);
	static EWindowMode::Type ToEngineWindowMode(EShooterWindowMode WindowMode);

	bool bGeneratedUIBound = false;
	bool bRefreshingGeneratedUI = false;
	int32 ActiveGeneratedPage = 0;
	FName ActiveGeneratedSideButton = TEXT("Btn_Side_Graphics");
	FName ActiveGeneratedOption = TEXT("Overall");

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GeneratedPreviewRenderTarget;

	TWeakObjectPtr<USceneCaptureComponent2D> GeneratedPreviewCaptureComponent;
	bool bGeneratedPreviewOwnsRenderTarget = false;
};
