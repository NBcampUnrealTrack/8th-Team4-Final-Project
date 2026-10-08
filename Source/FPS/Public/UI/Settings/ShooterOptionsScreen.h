#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "Settings/ShooterGameUserSettings.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterOptionsScreen.generated.h"

class UButton;
class UCommonButtonGroupBase;
class UEditableText;
class UEditableTextBox;
class USlider;
class UTextBlock;
class UWidgetSwitcher;
class UTexture2D;
class UShooterKeyBindingProxy;
class UVerticalBox;
struct FShooterInputBindingEntry;

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
UCLASS(Abstract, meta = (DisableNativeTick))
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
	float GetScreenBrightness() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Video")
	void SetScreenBrightness(float Brightness);

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

	// ----- Controls / Gameplay -----
	UFUNCTION(BlueprintPure, Category = "UI|Options|Controls")
	bool IsADSToggleEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Controls")
	void SetADSToggleEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Controls")
	bool IsCrouchToggleEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Controls")
	void SetCrouchToggleEnabled(bool bEnabled);

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

	// ----- Crosshair -----
	UFUNCTION(BlueprintPure, Category = "UI|Options|Crosshair")
	FLinearColor GetCrosshairColor() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Crosshair")
	void SetCrosshairColor(FLinearColor Color);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Crosshair")
	bool IsEnemyCrosshairColorEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Crosshair")
	void SetEnemyCrosshairColorEnabled(bool bEnabled);

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

	UFUNCTION(BlueprintPure, Category = "UI|Options|Voice Chat")
	float GetVoiceChatMicrophoneVolume() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Voice Chat")
	void SetVoiceChatMicrophoneVolume(float Volume);

	UFUNCTION(BlueprintPure, Category = "UI|Options|Voice Chat")
	float GetVoiceChatOutputVolume() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|Voice Chat")
	void SetVoiceChatOutputVolume(float Volume);

	// Runtime key mapping uses transient copies of the authored IMCs. This callback is
	// public only for the per-row UInputKeySelector proxy; WBP logic does not depend on it.
	void HandleKeyBindingSelected(FName BindingId, FKey NewKey);

	// ----- Options / Language -----
	UFUNCTION(BlueprintPure, Category = "UI|Options|General")
	EShooterOptionsLanguage GetLanguage() const;

	UFUNCTION(BlueprintCallable, Category = "UI|Options|General")
	void SetLanguage(EShooterOptionsLanguage Language);

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
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual void NativeDestruct() override;

private:

	// WBP_OptionsScreen의 고정 위젯 이름을 바인딩하고 실제 저장값은 UShooterGameUserSettings가 관리한다.
	void BindOptionControls();
	void ApplyOptionStaticStyles();
	// 켜짐/선택 상태 버튼의 배경을 스타일 또는 기존 색 방식으로 칠한다.
	void ApplyOptionButtonStateStyle(UButton* Button, bool bActive) const;
	UFUNCTION() void RefreshOptionsUI();
	void ShowOptionsPage(int32 PageIndex);
	void RefreshTabVisuals();
	void RefreshOptionRowVisuals();
	void SetChoiceSegments(FName Prefix, int32 ActiveIndex, int32 ChoiceCount) const;
	void SetButtonTextColor(FName ButtonName, const FLinearColor& Color) const;
	void StepResolution(int32 Delta);
	void StepWindowMode(int32 Delta);
	void StepFrameRate(int32 Delta);
	void StepOverallQuality(int32 Delta);
	void StepQuality(EShooterScalabilitySetting Setting, int32 Delta);
	void SetOptionText(FName WidgetName, const FText& Text) const;
	void SetToggleVisual(FName ButtonName, FName TextName, bool bEnabled) const;
	void ApplyOptionLanguage();
	FText Localize(const TCHAR* Korean, const TCHAR* English) const;
	void RebuildKeyBindings();
	FText BuildInputBindingDisplayName(const FShooterInputBindingEntry& Entry) const;
	bool TryParseNumber(const FText& Text, float& OutValue) const;
	void CommitAudioPercent(const FText& Text, ETextCommit::Type CommitMethod, EShooterAudioChannel Channel, FName OptionName);
	void CommitVoiceChatPercent(const FText& Text, ETextCommit::Type CommitMethod, bool bMicrophone, FName OptionName);
	void HandleAudioVolumeChangedInternal(float Value, EShooterAudioChannel Channel, FName OptionName);
	void HandleVoiceChatVolumeChangedInternal(float Value, bool bMicrophone, FName OptionName);
	void RefreshCrosshairOptionsUI(bool bRefreshText);
	void HandleCrosshairSettingsChanged();
	void SetCrosshairColorChannel(int32 Channel, float Value, bool bFromText);
	void ChangeCrosshairColorText(int32 Channel, const FText& Text);
	void CommitCrosshairColorText(int32 Channel, const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION() void HandleSideGraphics();
	UFUNCTION() void HandleSideDisplay();
	UFUNCTION() void HandleSideControls();
	UFUNCTION() void HandleSideKeyBindings();
	UFUNCTION() void HandleSideAudio();
	UFUNCTION() void HandleSideVoiceChat();
	UFUNCTION() void HandleSideSettings();
	UFUNCTION() void HandleSideCrosshair();
	UFUNCTION() void HandleSave();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleReset();
	UFUNCTION() void HandleVideoKeep();
	UFUNCTION() void HandleVideoRevert();

	UFUNCTION() void HandleResolutionPrev();
	UFUNCTION() void HandleResolutionNext();
	UFUNCTION() void HandleWindowModePrev();
	UFUNCTION() void HandleWindowModeNext();
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

	UFUNCTION() void HandleVSyncToggle();
	UFUNCTION() void HandleDynamicResolutionToggle();
	UFUNCTION() void HandleInvertYToggle();
	UFUNCTION() void HandleADSModeChanged();
	UFUNCTION() void HandleCrouchModeChanged();
	UFUNCTION() void HandleDynamicCrosshairToggle();
	UFUNCTION() void HandleEnemyCrosshairColorToggle();
	UFUNCTION() void HandleMasterMuteToggle();
	UFUNCTION() void HandleVoiceChatToggle();
	UFUNCTION() void HandleResetKeyBindings();

	UFUNCTION() void HandleResolutionScaleChanged(float Value);
	UFUNCTION() void HandleResolutionScaleCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleScreenBrightnessChanged(float Value);
	UFUNCTION() void HandleScreenBrightnessCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleHorizontalSensitivityChanged(float Value);
	UFUNCTION() void HandleVerticalSensitivityChanged(float Value);
	UFUNCTION() void HandleAimSensitivityChanged(float Value);

	// 감도 칸에 직접 입력한 값을 받는다. 범위는 설정 클래스가 자른다.
	UFUNCTION() void HandleHorizontalSensitivityCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleVerticalSensitivityCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleAimSensitivityCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION() void HandleMasterVolumeCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleMusicVolumeCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleSfxVolumeCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleUiVolumeCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleVoiceVolumeCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleVoiceChatMicrophoneVolumeCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleVoiceChatOutputVolumeCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION() void HandleMasterVolumeChanged(float Value);
	UFUNCTION() void HandleMusicVolumeChanged(float Value);
	UFUNCTION() void HandleSfxVolumeChanged(float Value);
	UFUNCTION() void HandleUiVolumeChanged(float Value);
	UFUNCTION() void HandleVoiceVolumeChanged(float Value);
	UFUNCTION() void HandleVoiceChatMicrophoneVolumeChanged(float Value);
	UFUNCTION() void HandleVoiceChatOutputVolumeChanged(float Value);
	UFUNCTION() void HandleCrosshairRedChanged(float Value);
	UFUNCTION() void HandleCrosshairRedTextChanged(const FText& Text);
	UFUNCTION() void HandleCrosshairRedCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleCrosshairGreenChanged(float Value);
	UFUNCTION() void HandleCrosshairGreenTextChanged(const FText& Text);
	UFUNCTION() void HandleCrosshairGreenCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleCrosshairBlueChanged(float Value);
	UFUNCTION() void HandleCrosshairBlueTextChanged(const FText& Text);
	UFUNCTION() void HandleCrosshairBlueCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleCrosshairAlphaChanged(float Value);
	UFUNCTION() void HandleCrosshairAlphaTextChanged(const FText& Text);
	UFUNCTION() void HandleCrosshairAlphaCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleLanguagePrev();
	UFUNCTION() void HandleLanguageNext();

	UShooterGameUserSettings* GetGameUserSettings() const;
	void ResetSettingsInternal(bool bResetProjectSettings);
	void RevertVideoModeInternal(bool bNotifySettingsChanged);
	void StartVideoModeConfirmationTimeout();
	void ClearVideoModeConfirmationTimeout();
	bool HandleVideoModeConfirmationTimeout(float DeltaTime);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Video", meta = (ClampMin = "1.0"))
	float VideoModeConfirmationTimeoutSeconds = 15.0f;

	// 둘 다 지정해야 행 배경이 텍스처로 바뀐다. 하나라도 비면 기존 색 방식으로 동작한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	TObjectPtr<UTexture2D> RowNormalTexture;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	TObjectPtr<UTexture2D> RowActiveTexture;

	// 값 박스(Border_Control_*) - HIGH 같은 현재 값과 선택 칸이 들어있는 상자.
	// 체크를 끄면 WBP에 설정한 모양이 그대로 유지된다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	bool bOverrideValueBoxStyle = false;

	// 비워두면 WBP의 브러시를 그대로 쓰고 색만 곱한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideValueBoxStyle"))
	TObjectPtr<UTexture2D> ValueBoxTexture;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideValueBoxStyle"))
	FLinearColor ValueBoxColor = FLinearColor::White;

	// 좌우 화살표에 한 번만 적용한다. 체크를 끄면 WBP에 설정한 모양이 그대로 유지된다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	bool bOverrideStepperArrowStyle = false;

	// 왼쪽(Prev) 화살표.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideStepperArrowStyle"))
	FButtonStyle StepperArrowPrevStyle;

	// 오른쪽(Next) 화살표.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideStepperArrowStyle"))
	FButtonStyle StepperArrowNextStyle;

	// 위 스타일 브러시에 곱해지는 틴트. 브러시를 그대로 보여주려면 흰색으로 둔다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideStepperArrowStyle"))
	FLinearColor StepperArrowBackgroundColor = FLinearColor::White;

	// 화살표 글자(TXT_Btn_*Prev/Next) 색.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideStepperArrowStyle"))
	FLinearColor StepperArrowTextColor = FLinearColor::White;

	// 화살표 여백. 스타일과 따로 켜고 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	bool bOverrideStepperArrowPadding = false;

	// 값 박스와의 바깥 간격(HB_Selector_* 슬롯).
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideStepperArrowPadding"))
	FMargin StepperArrowOuterPadding;

	// 글자 주변 안쪽 여백. 버튼 크기를 좌우한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideStepperArrowPadding"))
	FMargin StepperArrowContentPadding;

	// 토글/창 모드 버튼을 켜짐·꺼짐 상태에 따라 교체한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	bool bOverrideToggleStyle = false;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideToggleStyle"))
	FButtonStyle ToggleOnStyle;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideToggleStyle"))
	FButtonStyle ToggleOffStyle;

	// 런타임 상태 색은 WBP 기본값에서 함께 조정한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	FLinearColor RowNormalColor = FLinearColor(0.026f, 0.032f, 0.042f, 0.40f);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	FLinearColor RowActiveColor = FLinearColor(0.065f, 0.125f, 0.132f, 0.55f);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	FLinearColor TabNormalColor = FLinearColor(0.028f, 0.035f, 0.045f, 0.35f);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	FLinearColor TabActiveColor = FLinearColor(0.090f, 0.165f, 0.175f, 0.70f);

	// 선택 칸 표시줄 색.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	FLinearColor SegmentActiveColor = FLinearColor(0.300f, 0.680f, 0.650f, 1.00f);

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	FLinearColor SegmentInactiveColor = FLinearColor(0.160f, 0.195f, 0.210f, 0.45f);

	// 슬라이더에 한 번만 적용한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style")
	bool bOverrideSliderColors = false;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideSliderColors"))
	FLinearColor SliderBarColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Options|Style", meta = (EditCondition = "bOverrideSliderColors"))
	FLinearColor SliderHandleColor = FLinearColor::White;

	FTSTicker::FDelegateHandle VideoModeConfirmationTickerHandle;
	bool bVideoModeConfirmationPending = false;
	bool bVideoModeSelectionDirty = false;
	FIntPoint PendingResolution = FIntPoint::ZeroValue;
	EShooterWindowMode PendingWindowMode = EShooterWindowMode::Windowed;
	FIntPoint VideoModeBaselineResolution = FIntPoint::ZeroValue;
	EShooterWindowMode VideoModeBaselineWindowMode = EShooterWindowMode::Windowed;

	static EShooterWindowMode FromEngineWindowMode(EWindowMode::Type WindowMode);
	static EWindowMode::Type ToEngineWindowMode(EShooterWindowMode WindowMode);

	bool bControlsBound = false;
	bool bRefreshingUI = false;
	bool bEditingCrosshairText = false;
	int32 ActivePage = 0;
	FName ActiveSideButton = TEXT("Btn_Side_Graphics");
	FName ActiveOption = TEXT("Overall");

	// 사이드 탭이 모두 CommonButton일 때만 생성되며 선택 상태를 대신 관리한다.
	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonGroupBase> SideTabGroup;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterKeyBindingProxy>> KeyBindingProxies;

};
