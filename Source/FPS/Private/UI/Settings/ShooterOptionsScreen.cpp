#include "UI/Settings/ShooterOptionsScreen.h"

#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Slate/SceneViewport.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Settings/ShooterGameUserSettings.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"

UShooterOptionsScreen::UShooterOptionsScreen()
{
	InputConfig = EShooterWidgetInputMode::Menu;
}

void UShooterOptionsScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindGeneratedControls();
	OnSettingsChanged.AddUniqueDynamic(this, &UShooterOptionsScreen::RefreshGeneratedUI);
}

void UShooterOptionsScreen::RequestClose()
{
	if (OnCloseRequested.IsBound())
	{
		OnCloseRequested.Broadcast();
		return;
	}

	FinishEditing();
	DeactivateWidget();
}

void UShooterOptionsScreen::SaveOptions()
{
	if (UShooterGameUserSettings* Settings = GetGameUserSettings())
	{
		Settings->SaveSettings();
	}
}

void UShooterOptionsScreen::FinishEditing()
{
	RevertVideoModeInternal(false);
	SaveOptions();
}

void UShooterOptionsScreen::NativeOnActivated()
{
	if (UShooterGameUserSettings* Settings = GetGameUserSettings())
	{
		const FIntPoint Resolution = Settings->GetScreenResolution();
		if (Resolution.X <= 0 || Resolution.Y <= 0)
		{
			FViewport* Viewport = GEngine && GEngine->GameViewport
				? GEngine->GameViewport->Viewport : nullptr;
			if (Viewport)
			{
				Settings->SetScreenResolution(Viewport->GetSizeXY());
				Settings->SetFullscreenMode(Viewport->GetWindowMode());
			}
		}

		// UE defaults can temporarily expose ResolutionQuality == 0 before a concrete
		// screen percentage is established. Normalize that sentinel once for runtime use.
		float Normalized = 1.0f;
		float CurrentScale = 100.0f;
		float EngineMin = 50.0f;
		float EngineMax = 100.0f;
		Settings->GetResolutionScaleInformationEx(Normalized, CurrentScale, EngineMin, EngineMax);
		if (!FMath::IsFinite(CurrentScale) || CurrentScale <= 0.0f)
		{
			const float MinScale = FMath::Max(50.0f, FMath::IsFinite(EngineMin) ? EngineMin : 50.0f);
			const float MaxScale = FMath::Max(MinScale, FMath::Min(100.0f, FMath::IsFinite(EngineMax) ? EngineMax : 100.0f));
			float DefaultScale = Settings->GetDefaultResolutionScale();
			if (!FMath::IsFinite(DefaultScale) || DefaultScale <= 0.0f)
			{
				DefaultScale = 100.0f;
			}
			Settings->SetResolutionScaleValueEx(FMath::Clamp(DefaultScale, MinScale, MaxScale));
			Settings->ApplyNonResolutionSettings();
		}
	}

	Super::NativeOnActivated();
	RefreshGeneratedUI();
}

bool UShooterOptionsScreen::NativeOnHandleBackAction()
{
	RequestClose();
	return true;
}

void UShooterOptionsScreen::NativeDestruct()
{
	RevertVideoModeInternal(false);
	ReleaseGeneratedPreviewCapture();
	Super::NativeDestruct();
}

TArray<FIntPoint> UShooterOptionsScreen::GetSupportedResolutions() const
{
	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

	if (const UGameUserSettings* Settings = GetGameUserSettings())
	{
		const FIntPoint CurrentResolution = Settings->GetScreenResolution();
		if (CurrentResolution.X > 0 && CurrentResolution.Y > 0)
		{
			Resolutions.AddUnique(CurrentResolution);
		}
	}

	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X == B.X ? A.Y < B.Y : A.X < B.X;
	});

	return Resolutions;
}

FIntPoint UShooterOptionsScreen::GetScreenResolution() const
{
	if (bVideoModeConfirmationPending)
	{
		return PendingResolution;
	}

	const UGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetScreenResolution() : FIntPoint::ZeroValue;
}

void UShooterOptionsScreen::SetScreenResolution(const FIntPoint Resolution)
{
	PreviewVideoMode(Resolution, GetWindowMode());
}

EShooterWindowMode UShooterOptionsScreen::GetWindowMode() const
{
	if (bVideoModeConfirmationPending)
	{
		return PendingWindowMode;
	}

	const UGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings)
		? FromEngineWindowMode(Settings->GetFullscreenMode())
		: EShooterWindowMode::Windowed;
}

void UShooterOptionsScreen::SetWindowMode(const EShooterWindowMode WindowMode)
{
	PreviewVideoMode(GetScreenResolution(), WindowMode);
}

void UShooterOptionsScreen::PreviewVideoMode(
	const FIntPoint Resolution,
	const EShooterWindowMode WindowMode)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings) || Resolution.X <= 0 || Resolution.Y <= 0)
	{
		return;
	}

	const FIntPoint CurrentResolution = Settings->GetScreenResolution();
	if (CurrentResolution.X <= 0 || CurrentResolution.Y <= 0)
	{
		return;
	}

	if (!bVideoModeConfirmationPending)
	{
		VideoModeBaselineResolution = CurrentResolution;
		VideoModeBaselineWindowMode = FromEngineWindowMode(Settings->GetFullscreenMode());
	}

	PendingResolution = Resolution;
	PendingWindowMode = WindowMode;
	bVideoModeConfirmationPending = true;

	// Apply through UGameUserSettings so standalone/packaged windows actually resize
	// during the preview. We keep our own baseline and restore it if the user cancels.
	Settings->SetScreenResolution(PendingResolution);
	Settings->SetFullscreenMode(ToEngineWindowMode(PendingWindowMode));
	Settings->ApplyResolutionSettings(false);

	// A floating PIE window has its own scene viewport. Resize that exact local
	// viewport as well so resolution changes are visible during two-player PIE tests.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGameViewportClient* ViewportClient = GameInstance->GetGameViewportClient())
		{
			if (ViewportClient->bIsPlayInEditorViewport)
			{
				if (FSceneViewport* SceneViewport = ViewportClient->GetGameViewport())
				{
					SceneViewport->ResizeFrame(
						static_cast<uint32>(PendingResolution.X),
						static_cast<uint32>(PendingResolution.Y),
						ToEngineWindowMode(PendingWindowMode));
				}
			}
		}
	}

	StartVideoModeConfirmationTimeout();
	OnSettingsChanged.Broadcast();
}

bool UShooterOptionsScreen::IsVideoModeConfirmationPending() const
{
	return bVideoModeConfirmationPending;
}

void UShooterOptionsScreen::ConfirmVideoMode()
{
	if (!bVideoModeConfirmationPending)
	{
		return;
	}

	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetScreenResolution(PendingResolution);
	Settings->SetFullscreenMode(ToEngineWindowMode(PendingWindowMode));
	Settings->ApplyResolutionSettings(false);
	Settings->ConfirmVideoMode();

	ClearVideoModeConfirmationTimeout();
	bVideoModeConfirmationPending = false;
	VideoModeBaselineResolution = FIntPoint::ZeroValue;
	SaveOptions();
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::RevertVideoMode()
{
	RevertVideoModeInternal(true);
}

void UShooterOptionsScreen::RevertVideoModeInternal(const bool bNotifySettingsChanged)
{
	ClearVideoModeConfirmationTimeout();

	if (!bVideoModeConfirmationPending)
	{
		return;
	}

	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		bVideoModeConfirmationPending = false;
		VideoModeBaselineResolution = FIntPoint::ZeroValue;
		return;
	}

	const FIntPoint Resolution =
		VideoModeBaselineResolution.X > 0 && VideoModeBaselineResolution.Y > 0
			? VideoModeBaselineResolution
			: Settings->GetLastConfirmedScreenResolution();
	const EShooterWindowMode WindowMode =
		VideoModeBaselineResolution.X > 0 && VideoModeBaselineResolution.Y > 0
			? VideoModeBaselineWindowMode
			: FromEngineWindowMode(Settings->GetLastConfirmedFullscreenMode());

	Settings->SetScreenResolution(Resolution);
	Settings->SetFullscreenMode(ToEngineWindowMode(WindowMode));
	Settings->ApplyResolutionSettings(false);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGameViewportClient* ViewportClient = GameInstance->GetGameViewportClient())
		{
			if (ViewportClient->bIsPlayInEditorViewport)
			{
				if (FSceneViewport* SceneViewport = ViewportClient->GetGameViewport())
				{
					SceneViewport->ResizeFrame(
						static_cast<uint32>(Resolution.X),
						static_cast<uint32>(Resolution.Y),
						ToEngineWindowMode(WindowMode));
				}
			}
		}
	}

	bVideoModeConfirmationPending = false;
	VideoModeBaselineResolution = FIntPoint::ZeroValue;

	if (bNotifySettingsChanged)
	{
		OnSettingsChanged.Broadcast();
	}
}

void UShooterOptionsScreen::StartVideoModeConfirmationTimeout()
{
	ClearVideoModeConfirmationTimeout();
	VideoModeConfirmationTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(
			this,
			&ThisClass::HandleVideoModeConfirmationTimeout),
		FMath::Max(1.0f, VideoModeConfirmationTimeoutSeconds));
}

void UShooterOptionsScreen::ClearVideoModeConfirmationTimeout()
{
	if (!VideoModeConfirmationTickerHandle.IsValid())
	{
		return;
	}

	FTSTicker::GetCoreTicker().RemoveTicker(VideoModeConfirmationTickerHandle);
	VideoModeConfirmationTickerHandle.Reset();
}

bool UShooterOptionsScreen::HandleVideoModeConfirmationTimeout(const float DeltaTime)
{
	(void)DeltaTime;
	VideoModeConfirmationTickerHandle.Reset();
	RevertVideoMode();
	return false;
}

float UShooterOptionsScreen::GetResolutionScalePercent() const
{
	// UE 5.7: GetDefaultResolutionScale() is not const, even though it only queries
	// the engine default. Keep the settings pointer mutable so the fallback call compiles.
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return 100.0f;
	}

	float Normalized = 1.0f;
	float Current = 100.0f;
	float EngineMin = 50.0f;
	float EngineMax = 100.0f;
	Settings->GetResolutionScaleInformationEx(Normalized, Current, EngineMin, EngineMax);

	// SetToDefaults/first-run can leave ResolutionQuality at 0 as an engine sentinel.
	// 0% is not a useful render scale for this menu, so present the effective default instead.
	const float Min = FMath::Max(50.0f, FMath::IsFinite(EngineMin) ? EngineMin : 50.0f);
	const float Max = FMath::Max(Min, FMath::Min(100.0f, FMath::IsFinite(EngineMax) ? EngineMax : 100.0f));
	if (!FMath::IsFinite(Current) || Current <= 0.0f)
	{
		Current = Settings->GetDefaultResolutionScale();
	}
	if (!FMath::IsFinite(Current) || Current <= 0.0f)
	{
		Current = 100.0f;
	}

	return FMath::Clamp(Current, Min, Max);
}

void UShooterOptionsScreen::GetResolutionScaleRange(
	float& MinPercent,
	float& MaxPercent) const
{
	MinPercent = 50.0f;
	MaxPercent = 100.0f;

	const UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	float Normalized = 1.0f;
	float Current = 100.0f;
	float EngineMin = 50.0f;
	float EngineMax = 100.0f;
	Settings->GetResolutionScaleInformationEx(Normalized, Current, EngineMin, EngineMax);

	if (FMath::IsFinite(EngineMin))
	{
		MinPercent = FMath::Max(50.0f, EngineMin);
	}
	if (FMath::IsFinite(EngineMax))
	{
		MaxPercent = FMath::Min(100.0f, EngineMax);
	}
	MaxPercent = FMath::Max(MinPercent, MaxPercent);
}

void UShooterOptionsScreen::SetResolutionScalePercent(const float Percent)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	float Min = 50.0f;
	float Max = 100.0f;
	GetResolutionScaleRange(Min, Max);
	Settings->SetResolutionScaleValueEx(FMath::Clamp(Percent, Min, Max));
	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

bool UShooterOptionsScreen::IsVSyncEnabled() const
{
	const UGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) && Settings->IsVSyncEnabled();
}

void UShooterOptionsScreen::SetVSyncEnabled(const bool bEnabled)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetVSyncEnabled(bEnabled);
	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

float UShooterOptionsScreen::GetFrameRateLimit() const
{
	const UGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetFrameRateLimit() : 0.0f;
}

void UShooterOptionsScreen::SetFrameRateLimit(const float FramesPerSecond)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	// UGameUserSettings에서 0은 제한 없음이다.
	Settings->SetFrameRateLimit(FMath::Max(0.0f, FramesPerSecond));
	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

bool UShooterOptionsScreen::IsDynamicResolutionEnabled() const
{
	const UGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) && Settings->IsDynamicResolutionEnabled();
}

void UShooterOptionsScreen::SetDynamicResolutionEnabled(const bool bEnabled)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetDynamicResolutionEnabled(bEnabled);
	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

int32 UShooterOptionsScreen::GetOverallQualityLevel() const
{
	const UGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetOverallScalabilityLevel() : -1;
}

void UShooterOptionsScreen::SetOverallQualityLevel(const int32 QualityLevel)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetOverallScalabilityLevel(FMath::Clamp(QualityLevel, 0, 4));
	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

int32 UShooterOptionsScreen::GetQualityLevel(
	const EShooterScalabilitySetting Setting) const
{
	const UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return 0;
	}

	switch (Setting)
	{
	case EShooterScalabilitySetting::ViewDistance:
		return Settings->GetViewDistanceQuality();
	case EShooterScalabilitySetting::AntiAliasing:
		return Settings->GetAntiAliasingQuality();
	case EShooterScalabilitySetting::Shadow:
		return Settings->GetShadowQuality();
	case EShooterScalabilitySetting::GlobalIllumination:
		return Settings->GetGlobalIlluminationQuality();
	case EShooterScalabilitySetting::Reflection:
		return Settings->GetReflectionQuality();
	case EShooterScalabilitySetting::PostProcessing:
		return Settings->GetPostProcessingQuality();
	case EShooterScalabilitySetting::Texture:
		return Settings->GetTextureQuality();
	case EShooterScalabilitySetting::Effects:
		return Settings->GetVisualEffectQuality();
	case EShooterScalabilitySetting::Foliage:
		return Settings->GetFoliageQuality();
	case EShooterScalabilitySetting::Shading:
		return Settings->GetShadingQuality();
	default:
		return 0;
	}
}

void UShooterOptionsScreen::SetQualityLevel(
	const EShooterScalabilitySetting Setting,
	const int32 QualityLevel)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	const int32 ClampedQuality = FMath::Clamp(QualityLevel, 0, 4);

	switch (Setting)
	{
	case EShooterScalabilitySetting::ViewDistance:
		Settings->SetViewDistanceQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::AntiAliasing:
		Settings->SetAntiAliasingQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::Shadow:
		Settings->SetShadowQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::GlobalIllumination:
		Settings->SetGlobalIlluminationQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::Reflection:
		Settings->SetReflectionQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::PostProcessing:
		Settings->SetPostProcessingQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::Texture:
		Settings->SetTextureQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::Effects:
		Settings->SetVisualEffectQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::Foliage:
		Settings->SetFoliageQuality(ClampedQuality);
		break;
	case EShooterScalabilitySetting::Shading:
		Settings->SetShadingQuality(ClampedQuality);
		break;
	default:
		return;
	}

	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::RunHardwareBenchmarkAndApply(const int32 WorkScale)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->RunHardwareBenchmark(FMath::Max(1, WorkScale), 1.0f, 1.0f);
	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

float UShooterOptionsScreen::GetMouseHorizontalSensitivity() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetMouseHorizontalSensitivity() : 1.0f;
}

void UShooterOptionsScreen::SetMouseHorizontalSensitivity(const float Sensitivity)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetMouseHorizontalSensitivity(Sensitivity);
	OnSettingsChanged.Broadcast();
}

float UShooterOptionsScreen::GetMouseVerticalSensitivity() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetMouseVerticalSensitivity() : 1.0f;
}

void UShooterOptionsScreen::SetMouseVerticalSensitivity(const float Sensitivity)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetMouseVerticalSensitivity(Sensitivity);
	OnSettingsChanged.Broadcast();
}

float UShooterOptionsScreen::GetAimSensitivityMultiplier() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetAimSensitivityMultiplier() : 1.0f;
}

void UShooterOptionsScreen::SetAimSensitivityMultiplier(const float Multiplier)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetAimSensitivityMultiplier(Multiplier);
	OnSettingsChanged.Broadcast();
}

bool UShooterOptionsScreen::IsInvertVerticalLookEnabled() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) && Settings->IsInvertVerticalLookEnabled();
}

void UShooterOptionsScreen::SetInvertVerticalLookEnabled(const bool bEnabled)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetInvertVerticalLookEnabled(bEnabled);
	OnSettingsChanged.Broadcast();
}

bool UShooterOptionsScreen::IsDynamicCrosshairEnabled() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return !IsValid(Settings) || Settings->IsDynamicCrosshairEnabled();
}

void UShooterOptionsScreen::SetDynamicCrosshairEnabled(const bool bEnabled)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetDynamicCrosshairEnabled(bEnabled);
	OnSettingsChanged.Broadcast();
}

float UShooterOptionsScreen::GetAudioVolume(const EShooterAudioChannel Channel) const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetAudioVolume(Channel) : 1.0f;
}

void UShooterOptionsScreen::SetAudioVolume(
	const EShooterAudioChannel Channel,
	const float Volume)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetAudioVolume(Channel, Volume);
	OnSettingsChanged.Broadcast();
}

bool UShooterOptionsScreen::IsMasterAudioMuted() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) && Settings->IsMasterAudioMuted();
}

void UShooterOptionsScreen::SetMasterAudioMuted(const bool bMuted)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetMasterAudioMuted(bMuted);
	OnSettingsChanged.Broadcast();
}

bool UShooterOptionsScreen::IsVoiceChatEnabled() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return !IsValid(Settings) || Settings->IsVoiceChatEnabled();
}

void UShooterOptionsScreen::SetVoiceChatEnabled(const bool bEnabled)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetVoiceChatEnabled(bEnabled);
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::ResetEngineSettingsToDefaults()
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	RevertVideoModeInternal(false);
	const FIntPoint Resolution = Settings->GetScreenResolution();
	const EWindowMode::Type WindowMode = Settings->GetFullscreenMode();
	// 사용자에게 확인받은 화면 모드와 프로젝트 전용 감도/오디오 값은 유지한다.
	Settings->UGameUserSettings::SetToDefaults();
	Settings->SetScreenResolution(Resolution);
	Settings->SetFullscreenMode(WindowMode);
	Settings->ConfirmVideoMode(); // 초기화 전 확정 화면 모드를 보존한다.

	// Reset 후 엔진 내부에서 0이 sentinel처럼 남는 경우가 있어 실제 렌더 스케일을 명시한다.
	float ScaleMin = 50.0f;
	float ScaleMax = 100.0f;
	GetResolutionScaleRange(ScaleMin, ScaleMax);
	float DefaultScale = Settings->GetDefaultResolutionScale();
	if (!FMath::IsFinite(DefaultScale) || DefaultScale <= 0.0f)
	{
		DefaultScale = 100.0f;
	}
	Settings->SetResolutionScaleValueEx(FMath::Clamp(DefaultScale, ScaleMin, ScaleMax));
	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::ResetAllSettingsToDefaults()
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	RevertVideoModeInternal(false);
	const FIntPoint Resolution = Settings->GetScreenResolution();
	const EWindowMode::Type WindowMode = Settings->GetFullscreenMode();
	Settings->SetToDefaults();
	Settings->SetScreenResolution(Resolution);
	Settings->SetFullscreenMode(WindowMode);
	Settings->ConfirmVideoMode(); // 초기화 전 확정 화면 모드를 보존한다.

	// Reset 후 엔진 내부에서 0이 sentinel처럼 남는 경우가 있어 실제 렌더 스케일을 명시한다.
	float ScaleMin = 50.0f;
	float ScaleMax = 100.0f;
	GetResolutionScaleRange(ScaleMin, ScaleMax);
	float DefaultScale = Settings->GetDefaultResolutionScale();
	if (!FMath::IsFinite(DefaultScale) || DefaultScale <= 0.0f)
	{
		DefaultScale = 100.0f;
	}
	Settings->SetResolutionScaleValueEx(FMath::Clamp(DefaultScale, ScaleMin, ScaleMax));
	Settings->ApplyNonResolutionSettings();
	OnSettingsChanged.Broadcast();
}

FText UShooterOptionsScreen::FormatResolution(const FIntPoint Resolution)
{
	return FText::FromString(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
}

FText UShooterOptionsScreen::GetWindowModeDisplayText(
	const EShooterWindowMode WindowMode)
{
	switch (WindowMode)
	{
	case EShooterWindowMode::Fullscreen:
	case EShooterWindowMode::WindowedFullscreen:
		return NSLOCTEXT("ShooterOptions", "FullscreenSimple", "전체 화면");
	case EShooterWindowMode::Windowed:
	default:
		return NSLOCTEXT("ShooterOptions", "WindowedSimple", "창 모드");
	}
}

FText UShooterOptionsScreen::GetQualityDisplayText(const int32 QualityLevel)
{
	switch (QualityLevel)
	{
	case 0:
		return NSLOCTEXT("ShooterOptions", "QualityLow", "LOW");
	case 1:
		return NSLOCTEXT("ShooterOptions", "QualityMedium", "MEDIUM");
	case 2:
		return NSLOCTEXT("ShooterOptions", "QualityHigh", "HIGH");
	case 3:
		return NSLOCTEXT("ShooterOptions", "QualityEpic", "EPIC");
	case 4:
		return NSLOCTEXT("ShooterOptions", "QualityCinematic", "CINEMATIC");
	default:
		return NSLOCTEXT("ShooterOptions", "QualityCustom", "CUSTOM");
	}
}

FText UShooterOptionsScreen::GetFrameRateLimitDisplayText(const float FramesPerSecond)
{
	if (FramesPerSecond <= 0.0f)
	{
		return NSLOCTEXT("ShooterOptions", "FrameRateUnlimited", "UNLIMITED");
	}

	return FText::AsNumber(FMath::RoundToInt(FramesPerSecond));
}

void UShooterOptionsScreen::BindGeneratedControls()
{
	if (bGeneratedUIBound)
	{
		return;
	}

#define BIND_CLICK(WidgetName, HandlerName) \
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT(WidgetName)))) \
	{ \
		Button->OnClicked.AddUniqueDynamic(this, &UShooterOptionsScreen::HandlerName); \
	}

#define BIND_SLIDER(WidgetName, HandlerName) \
	if (USlider* Slider = Cast<USlider>(GetWidgetFromName(TEXT(WidgetName)))) \
	{ \
		Slider->OnValueChanged.AddUniqueDynamic(this, &UShooterOptionsScreen::HandlerName); \
	}

#define BIND_FOCUS(WidgetName, HandlerName) \
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT(WidgetName)))) \
	{ \
		Button->OnClicked.AddUniqueDynamic(this, &UShooterOptionsScreen::HandlerName); \
		Button->OnHovered.AddUniqueDynamic(this, &UShooterOptionsScreen::HandlerName); \
	}

	BIND_CLICK("Btn_Side_Graphics", HandleSideGraphics);
	BIND_CLICK("Btn_Side_Display", HandleSideDisplay);
	BIND_CLICK("Btn_Side_Controls", HandleSideControls);
	BIND_CLICK("Btn_Side_Audio", HandleSideAudio);
	BIND_CLICK("Btn_Save", HandleSave);
	BIND_CLICK("Btn_Back", HandleBack);
	BIND_CLICK("Btn_Reset", HandleReset);
	BIND_CLICK("Btn_VideoKeep", HandleVideoKeep);
	BIND_CLICK("Btn_VideoRevert", HandleVideoRevert);
	BIND_CLICK("Btn_ResolutionPrev", HandleResolutionPrev);
	BIND_CLICK("Btn_ResolutionNext", HandleResolutionNext);
	BIND_CLICK("Btn_WindowModePrev", HandleWindowModePrev);
	BIND_CLICK("Btn_WindowModeNext", HandleWindowModeNext);
	BIND_CLICK("Btn_WindowModeFullscreen", HandleWindowModeFullscreen);
	BIND_CLICK("Btn_WindowModeWindowed", HandleWindowModeWindowed);
	BIND_CLICK("Btn_FrameRatePrev", HandleFrameRatePrev);
	BIND_CLICK("Btn_FrameRateNext", HandleFrameRateNext);
	BIND_CLICK("Btn_OverallPrev", HandleOverallPrev);
	BIND_CLICK("Btn_OverallNext", HandleOverallNext);
	BIND_CLICK("Btn_ViewDistancePrev", HandleViewDistancePrev);
	BIND_CLICK("Btn_ViewDistanceNext", HandleViewDistanceNext);
	BIND_CLICK("Btn_AntiAliasingPrev", HandleAntiAliasingPrev);
	BIND_CLICK("Btn_AntiAliasingNext", HandleAntiAliasingNext);
	BIND_CLICK("Btn_ShadowPrev", HandleShadowPrev);
	BIND_CLICK("Btn_ShadowNext", HandleShadowNext);
	BIND_CLICK("Btn_GlobalIlluminationPrev", HandleGlobalIlluminationPrev);
	BIND_CLICK("Btn_GlobalIlluminationNext", HandleGlobalIlluminationNext);
	BIND_CLICK("Btn_ReflectionPrev", HandleReflectionPrev);
	BIND_CLICK("Btn_ReflectionNext", HandleReflectionNext);
	BIND_CLICK("Btn_PostProcessingPrev", HandlePostProcessingPrev);
	BIND_CLICK("Btn_PostProcessingNext", HandlePostProcessingNext);
	BIND_CLICK("Btn_TexturePrev", HandleTexturePrev);
	BIND_CLICK("Btn_TextureNext", HandleTextureNext);
	BIND_CLICK("Btn_EffectsPrev", HandleEffectsPrev);
	BIND_CLICK("Btn_EffectsNext", HandleEffectsNext);
	BIND_CLICK("Btn_FoliagePrev", HandleFoliagePrev);
	BIND_CLICK("Btn_FoliageNext", HandleFoliageNext);
	BIND_CLICK("Btn_ShadingPrev", HandleShadingPrev);
	BIND_CLICK("Btn_ShadingNext", HandleShadingNext);
	BIND_FOCUS("Btn_Info_Resolution", HandleInfoResolution);
	BIND_FOCUS("Btn_Info_WindowMode", HandleInfoWindowMode);
	BIND_FOCUS("Btn_Info_FrameRate", HandleInfoFrameRate);
	BIND_FOCUS("Btn_Info_Overall", HandleInfoOverall);
	BIND_FOCUS("Btn_Info_ViewDistance", HandleInfoViewDistance);
	BIND_FOCUS("Btn_Info_AntiAliasing", HandleInfoAntiAliasing);
	BIND_FOCUS("Btn_Info_Shadow", HandleInfoShadow);
	BIND_FOCUS("Btn_Info_GlobalIllumination", HandleInfoGlobalIllumination);
	BIND_FOCUS("Btn_Info_Reflection", HandleInfoReflection);
	BIND_FOCUS("Btn_Info_PostProcessing", HandleInfoPostProcessing);
	BIND_FOCUS("Btn_Info_Texture", HandleInfoTexture);
	BIND_FOCUS("Btn_Info_Effects", HandleInfoEffects);
	BIND_FOCUS("Btn_Info_Foliage", HandleInfoFoliage);
	BIND_FOCUS("Btn_Info_Shading", HandleInfoShading);

	BIND_CLICK("Btn_VSyncToggle", HandleVSyncToggle);
	BIND_CLICK("Btn_DynamicResolutionToggle", HandleDynamicResolutionToggle);
	BIND_CLICK("Btn_InvertYToggle", HandleInvertYToggle);
	BIND_CLICK("Btn_DynamicCrosshairToggle", HandleDynamicCrosshairToggle);
	BIND_CLICK("Btn_MasterMuteToggle", HandleMasterMuteToggle);
	BIND_CLICK("Btn_VoiceChatToggle", HandleVoiceChatToggle);

	BIND_SLIDER("SLD_ResolutionScale", HandleResolutionScaleChanged);
	BIND_SLIDER("SLD_HorizontalSensitivity", HandleHorizontalSensitivityChanged);
	BIND_SLIDER("SLD_VerticalSensitivity", HandleVerticalSensitivityChanged);
	BIND_SLIDER("SLD_AimSensitivity", HandleAimSensitivityChanged);
	BIND_SLIDER("SLD_MasterVolume", HandleMasterVolumeChanged);
	BIND_SLIDER("SLD_MusicVolume", HandleMusicVolumeChanged);
	BIND_SLIDER("SLD_SfxVolume", HandleSfxVolumeChanged);
	BIND_SLIDER("SLD_UiVolume", HandleUiVolumeChanged);
	BIND_SLIDER("SLD_VoiceVolume", HandleVoiceVolumeChanged);

#undef BIND_FOCUS
#undef BIND_SLIDER
#undef BIND_CLICK

	bGeneratedUIBound = true;
}

void UShooterOptionsScreen::SetGeneratedText(const FName WidgetName, const FText& Text) const
{
	if (UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(WidgetName)))
	{
		TextBlock->SetText(Text);
	}
}


void UShooterOptionsScreen::SetGeneratedButtonTextColor(
	const FName ButtonName,
	const FLinearColor& Color) const
{
	const FName TextName(*FString::Printf(TEXT("TXT_%s"), *ButtonName.ToString()));
	if (UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(TextName)))
	{
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UShooterOptionsScreen::SetGeneratedChoiceSegments(
	const FName Prefix,
	const int32 ActiveIndex,
	const int32 ChoiceCount) const
{
	static constexpr int32 MaxSegments = 24;
	const int32 VisibleCount = FMath::Clamp(ChoiceCount, 0, MaxSegments);
	const FString PrefixString = Prefix.ToString();
	for (int32 Index = 0; Index < MaxSegments; ++Index)
	{
		const bool bVisible = Index < VisibleCount;
		const FName SizeName(*FString::Printf(TEXT("SB_Choice_%s_%d"), *PrefixString, Index));
		if (USizeBox* SegmentSize = Cast<USizeBox>(GetWidgetFromName(SizeName)))
		{
			SegmentSize->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}

		const FName SegmentName(*FString::Printf(TEXT("Border_Choice_%s_%d"), *PrefixString, Index));
		if (UBorder* Segment = Cast<UBorder>(GetWidgetFromName(SegmentName)))
		{
			Segment->SetBrushColor(Index == ActiveIndex
				? FLinearColor(0.030f, 0.900f, 0.820f, 1.0f)
				: FLinearColor(0.130f, 0.210f, 0.215f, 0.72f));
		}
	}
}

void UShooterOptionsScreen::SetGeneratedWindowModeVisual() const
{
	const bool bFullscreen = GetWindowMode() != EShooterWindowMode::Windowed;
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Btn_WindowModeFullscreen"))))
	{
		Button->SetBackgroundColor(bFullscreen
			? FLinearColor(0.025f, 0.330f, 0.315f, 0.78f)
			: FLinearColor(0.008f, 0.030f, 0.032f, 0.74f));
	}
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Btn_WindowModeWindowed"))))
	{
		Button->SetBackgroundColor(!bFullscreen
			? FLinearColor(0.025f, 0.330f, 0.315f, 0.78f)
			: FLinearColor(0.008f, 0.030f, 0.032f, 0.74f));
	}
	SetGeneratedButtonTextColor(TEXT("Btn_WindowModeFullscreen"), FLinearColor::White);
	SetGeneratedButtonTextColor(TEXT("Btn_WindowModeWindowed"), FLinearColor::White);
}

void UShooterOptionsScreen::SetGeneratedToggleVisual(
	const FName ButtonName,
	const FName TextName,
	const bool bEnabled) const
{
	if (UButton* Button = Cast<UButton>(GetWidgetFromName(ButtonName)))
	{
		Button->SetBackgroundColor(bEnabled
			? FLinearColor(0.025f, 0.330f, 0.315f, 0.78f)
			: FLinearColor(0.008f, 0.030f, 0.032f, 0.74f));
	}

	SetGeneratedText(TextName, bEnabled
		? NSLOCTEXT("ShooterOptions", "GeneratedOn", "켜짐")
		: NSLOCTEXT("ShooterOptions", "GeneratedOff", "꺼짐"));
}

void UShooterOptionsScreen::RefreshGeneratedUI()
{
	if (!bGeneratedUIBound || bRefreshingGeneratedUI)
	{
		return;
	}

	bRefreshingGeneratedUI = true;

	const FText ResolutionText = FormatResolution(GetScreenResolution());
	const FText FrameLimitText = GetFrameRateLimitDisplayText(GetFrameRateLimit());
	const FText OverallText = GetQualityDisplayText(GetOverallQualityLevel());

	SetGeneratedText(TEXT("TXT_ResolutionValue"), ResolutionText);
	SetGeneratedText(TEXT("TXT_WindowModeValue"), GetWindowModeDisplayText(GetWindowMode()));
	SetGeneratedText(TEXT("TXT_FrameRateValue"), FrameLimitText);
	SetGeneratedText(TEXT("TXT_OverallValue"), OverallText);

	// Header is intentionally not navigation; it summarizes the current runtime state.
	SetGeneratedText(TEXT("TXT_HeaderStatusValue_Display"), ResolutionText);
	SetGeneratedText(TEXT("TXT_HeaderStatusValue_Quality"), OverallText);
	SetGeneratedText(TEXT("TXT_HeaderStatusValue_FPS"), FrameLimitText);


	// Show how many discrete choices each selector has and where the current value sits.
	{
		const TArray<FIntPoint> Resolutions = GetSupportedResolutions();
		const int32 ResolutionIndex = Resolutions.IndexOfByKey(GetScreenResolution());
		SetGeneratedChoiceSegments(TEXT("Resolution"), ResolutionIndex, Resolutions.Num());

		static const float Limits[] = {0.0f, 30.0f, 60.0f, 120.0f, 144.0f, 165.0f, 240.0f, 360.0f};
		const float CurrentLimit = GetFrameRateLimit();
		int32 BestIndex = 0;
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Limits); ++Index)
		{
			const float Distance = FMath::Abs(CurrentLimit - Limits[Index]);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestIndex = Index;
			}
		}
		SetGeneratedChoiceSegments(TEXT("FrameRate"), BestIndex, UE_ARRAY_COUNT(Limits));

		const int32 Overall = GetOverallQualityLevel();
		SetGeneratedChoiceSegments(TEXT("Overall"), Overall >= 0 && Overall <= 4 ? Overall : INDEX_NONE, 5);
	}
	SetGeneratedWindowModeVisual();

	const TPair<EShooterScalabilitySetting, FName> QualityTexts[] =
	{
		{EShooterScalabilitySetting::ViewDistance, TEXT("TXT_ViewDistanceValue")},
		{EShooterScalabilitySetting::AntiAliasing, TEXT("TXT_AntiAliasingValue")},
		{EShooterScalabilitySetting::Shadow, TEXT("TXT_ShadowValue")},
		{EShooterScalabilitySetting::GlobalIllumination, TEXT("TXT_GlobalIlluminationValue")},
		{EShooterScalabilitySetting::Reflection, TEXT("TXT_ReflectionValue")},
		{EShooterScalabilitySetting::PostProcessing, TEXT("TXT_PostProcessingValue")},
		{EShooterScalabilitySetting::Texture, TEXT("TXT_TextureValue")},
		{EShooterScalabilitySetting::Effects, TEXT("TXT_EffectsValue")},
		{EShooterScalabilitySetting::Foliage, TEXT("TXT_FoliageValue")},
		{EShooterScalabilitySetting::Shading, TEXT("TXT_ShadingValue")},
	};


	for (const TPair<EShooterScalabilitySetting, FName>& Entry : QualityTexts)
	{
		SetGeneratedText(Entry.Value, GetQualityDisplayText(GetQualityLevel(Entry.Key)));
	}

	SetGeneratedChoiceSegments(TEXT("ViewDistance"), GetQualityLevel(EShooterScalabilitySetting::ViewDistance), 5);
	SetGeneratedChoiceSegments(TEXT("AntiAliasing"), GetQualityLevel(EShooterScalabilitySetting::AntiAliasing), 5);
	SetGeneratedChoiceSegments(TEXT("Shadow"), GetQualityLevel(EShooterScalabilitySetting::Shadow), 5);
	SetGeneratedChoiceSegments(TEXT("GlobalIllumination"), GetQualityLevel(EShooterScalabilitySetting::GlobalIllumination), 5);
	SetGeneratedChoiceSegments(TEXT("Reflection"), GetQualityLevel(EShooterScalabilitySetting::Reflection), 5);
	SetGeneratedChoiceSegments(TEXT("PostProcessing"), GetQualityLevel(EShooterScalabilitySetting::PostProcessing), 5);
	SetGeneratedChoiceSegments(TEXT("Texture"), GetQualityLevel(EShooterScalabilitySetting::Texture), 5);
	SetGeneratedChoiceSegments(TEXT("Effects"), GetQualityLevel(EShooterScalabilitySetting::Effects), 5);
	SetGeneratedChoiceSegments(TEXT("Foliage"), GetQualityLevel(EShooterScalabilitySetting::Foliage), 5);
	SetGeneratedChoiceSegments(TEXT("Shading"), GetQualityLevel(EShooterScalabilitySetting::Shading), 5);

	float ResolutionMin = 50.0f;
	float ResolutionMax = 100.0f;
	GetResolutionScaleRange(ResolutionMin, ResolutionMax);
	if (USlider* Slider = Cast<USlider>(GetWidgetFromName(TEXT("SLD_ResolutionScale"))))
	{
		Slider->SetMinValue(ResolutionMin);
		Slider->SetMaxValue(ResolutionMax);
		Slider->SetValue(GetResolutionScalePercent());
	}
	SetGeneratedText(TEXT("TXT_ResolutionScaleValue"), FText::FromString(FString::Printf(TEXT("%.0f%%"), GetResolutionScalePercent())));

	SetGeneratedToggleVisual(TEXT("Btn_VSyncToggle"), TEXT("TXT_VSyncValue"), IsVSyncEnabled());
	SetGeneratedToggleVisual(TEXT("Btn_DynamicResolutionToggle"), TEXT("TXT_DynamicResolutionValue"), IsDynamicResolutionEnabled());
	SetGeneratedToggleVisual(TEXT("Btn_InvertYToggle"), TEXT("TXT_InvertYValue"), IsInvertVerticalLookEnabled());
	SetGeneratedToggleVisual(TEXT("Btn_DynamicCrosshairToggle"), TEXT("TXT_DynamicCrosshairValue"), IsDynamicCrosshairEnabled());
	SetGeneratedToggleVisual(TEXT("Btn_MasterMuteToggle"), TEXT("TXT_MasterMuteValue"), IsMasterAudioMuted());
	SetGeneratedToggleVisual(TEXT("Btn_VoiceChatToggle"), TEXT("TXT_VoiceChatValue"), IsVoiceChatEnabled());

	auto SetSlider = [this](const FName SliderName, const FName TextName, const float Value, const int32 FractionDigits)
	{
		if (USlider* Slider = Cast<USlider>(GetWidgetFromName(SliderName)))
		{
			Slider->SetValue(Value);
		}

		const FString ValueText = FractionDigits > 1
			? FString::Printf(TEXT("%.2f"), Value)
			: FString::Printf(TEXT("%.1f"), Value);
		SetGeneratedText(TextName, FText::FromString(ValueText));
	};

	SetSlider(TEXT("SLD_HorizontalSensitivity"), TEXT("TXT_HorizontalSensitivityValue"), GetMouseHorizontalSensitivity(), 2);
	SetSlider(TEXT("SLD_VerticalSensitivity"), TEXT("TXT_VerticalSensitivityValue"), GetMouseVerticalSensitivity(), 2);
	SetSlider(TEXT("SLD_AimSensitivity"), TEXT("TXT_AimSensitivityValue"), GetAimSensitivityMultiplier(), 2);

	auto SetVolume = [this](const FName SliderName, const FName TextName, const EShooterAudioChannel Channel)
	{
		const float Value = GetAudioVolume(Channel);
		if (USlider* Slider = Cast<USlider>(GetWidgetFromName(SliderName)))
		{
			Slider->SetValue(Value);
		}
		SetGeneratedText(TextName, FText::FromString(FString::Printf(TEXT("%.0f"), Value * 100.0f)));
	};

	SetVolume(TEXT("SLD_MasterVolume"), TEXT("TXT_MasterVolumeValue"), EShooterAudioChannel::Master);
	SetVolume(TEXT("SLD_MusicVolume"), TEXT("TXT_MusicVolumeValue"), EShooterAudioChannel::Music);
	SetVolume(TEXT("SLD_SfxVolume"), TEXT("TXT_SfxVolumeValue"), EShooterAudioChannel::SoundEffects);
	SetVolume(TEXT("SLD_UiVolume"), TEXT("TXT_UiVolumeValue"), EShooterAudioChannel::UserInterface);
	SetVolume(TEXT("SLD_VoiceVolume"), TEXT("TXT_VoiceVolumeValue"), EShooterAudioChannel::Voice);

	if (UWidget* Popup = GetWidgetFromName(TEXT("Overlay_VideoConfirm")))
	{
		Popup->SetVisibility(IsVideoModeConfirmationPending() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetGeneratedText(
		TEXT("TXT_VideoConfirmCountdown"),
		FText::FromString(FString::Printf(TEXT("%.0f초 후 이전 설정으로 복구됩니다."), GetVideoModeConfirmationTimeoutSeconds())));

	if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("Switcher_MainPages"))))
	{
		Switcher->SetActiveWidgetIndex(FMath::Clamp(ActiveGeneratedPage, 0, 3));
	}

	RefreshGeneratedTabVisuals();
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
	bRefreshingGeneratedUI = false;
}

void UShooterOptionsScreen::ShowGeneratedPage(const int32 PageIndex)
{
	ActiveGeneratedPage = FMath::Clamp(PageIndex, 0, 3);
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::RefreshGeneratedTabVisuals()
{

	const struct
	{
		FName Button;
		FName Accent;
	} SideItems[] =
	{
		{TEXT("Btn_Side_Graphics"), TEXT("SB_SideAccent_Graphics")},
		{TEXT("Btn_Side_Display"), TEXT("SB_SideAccent_Display")},
		{TEXT("Btn_Side_Controls"), TEXT("SB_SideAccent_Controls")},
		{TEXT("Btn_Side_Audio"), TEXT("SB_SideAccent_Audio")},
	};

	for (const auto& SideItem : SideItems)
	{
		const bool bSelected = SideItem.Button == ActiveGeneratedSideButton;
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(SideItem.Button)))
		{
			Button->SetBackgroundColor(bSelected
				? FLinearColor(0.024f, 0.260f, 0.252f, 0.62f)
				: FLinearColor(0.008f, 0.028f, 0.030f, 0.34f));
		}
		if (USizeBox* Accent = Cast<USizeBox>(GetWidgetFromName(SideItem.Accent)))
		{
			Accent->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		SetGeneratedButtonTextColor(
			SideItem.Button,
			bSelected ? FLinearColor::White : FLinearColor(0.72f, 0.77f, 0.79f, 1.0f));
	}
}

void UShooterOptionsScreen::RefreshGeneratedOptionRowVisuals()
{
	const FName Prefixes[] =
	{
		TEXT("Resolution"), TEXT("WindowMode"), TEXT("ResolutionScale"), TEXT("VSync"), TEXT("FrameRate"), TEXT("DynamicResolution"),
		TEXT("Overall"), TEXT("ViewDistance"), TEXT("AntiAliasing"), TEXT("Shadow"), TEXT("GlobalIllumination"),
		TEXT("Reflection"), TEXT("PostProcessing"), TEXT("Texture"), TEXT("Effects"), TEXT("Foliage"), TEXT("Shading"),
		TEXT("HorizontalSensitivity"), TEXT("VerticalSensitivity"), TEXT("AimSensitivity"), TEXT("InvertY"), TEXT("DynamicCrosshair"),
		TEXT("MasterVolume"), TEXT("MusicVolume"), TEXT("SfxVolume"), TEXT("UiVolume"), TEXT("VoiceVolume"), TEXT("MasterMute"), TEXT("VoiceChat"),
	};

	for (const FName Prefix : Prefixes)
	{
		const FName RowName(*FString::Printf(TEXT("Border_Row_%s"), *Prefix.ToString()));
		if (UBorder* Row = Cast<UBorder>(GetWidgetFromName(RowName)))
		{
			Row->SetBrushColor(Prefix == ActiveGeneratedOption
				? FLinearColor(0.030f, 0.175f, 0.170f, 0.46f)
				: FLinearColor(0.050f, 0.082f, 0.084f, 0.34f));
		}
	}
}

void UShooterOptionsScreen::SetGeneratedPerformanceSegments(
	const int32 ActiveSegments,
	const bool bRelevant) const
{
	const int32 ClampedSegments = FMath::Clamp(ActiveSegments, 0, 6);
	const FLinearColor Accent(0.03f, 0.90f, 0.82f, 1.0f);
	const FLinearColor Inactive(0.13f, 0.21f, 0.215f, 0.72f);

	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FName SegmentName(*FString::Printf(TEXT("Border_PerfSegment_%d"), Index));
		if (UBorder* Segment = Cast<UBorder>(GetWidgetFromName(SegmentName)))
		{
			Segment->SetBrushColor(bRelevant && Index < ClampedSegments ? Accent : Inactive);
		}
	}

	SetGeneratedText(
		TEXT("TXT_PerformanceTitle"),
		bRelevant
			? FText::FromString(TEXT("예상 성능 영향도"))
			: FText::FromString(TEXT("성능 영향도 없음")));
}

void UShooterOptionsScreen::RefreshGeneratedPreview()
{
	UImage* PreviewImage = Cast<UImage>(GetWidgetFromName(TEXT("IMG_Preview")));
	UTextBlock* PreviewStatus = Cast<UTextBlock>(GetWidgetFromName(TEXT("TXT_PreviewStatus")));
	if (!PreviewImage)
	{
		return;
	}

	if (ActiveGeneratedPage != 0)
	{
		PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		if (PreviewStatus)
		{
			PreviewStatus->SetVisibility(ESlateVisibility::Collapsed);
		}
		ReleaseGeneratedPreviewCapture();
		return;
	}

	ASceneCapture2D* PreviewActor = nullptr;
	USceneCaptureComponent2D* CaptureComponent = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ASceneCapture2D> It(World); It; ++It)
		{
			USceneCaptureComponent2D* CandidateCapture = It->GetCaptureComponent2D();
			if (CandidateCapture && CandidateCapture->ComponentHasTag(TEXT("OptionsPreview")))
			{
				PreviewActor = *It;
				CaptureComponent = CandidateCapture;
				break;
			}
		}
	}

	if (!PreviewActor || !CaptureComponent)
	{
		PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		if (PreviewStatus)
		{
			PreviewStatus->SetVisibility(ESlateVisibility::Visible);
			PreviewStatus->SetText(FText::FromString(
				TEXT("레벨에 SceneCapture2D를 배치하고\nCaptureComponent2D의 Component Tag를 OptionsPreview로 지정하세요.")));
		}
		return;
	}

	if (!CaptureComponent->TextureTarget)
	{
		if (!GeneratedPreviewRenderTarget)
		{
			GeneratedPreviewRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("OptionsPreviewRuntimeRT"));
			GeneratedPreviewRenderTarget->ClearColor = FLinearColor::Black;
			GeneratedPreviewRenderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
			GeneratedPreviewRenderTarget->bForceLinearGamma = false;
			GeneratedPreviewRenderTarget->InitAutoFormat(640, 360);
			GeneratedPreviewRenderTarget->UpdateResourceImmediate(true);
		}
		CaptureComponent->TextureTarget = GeneratedPreviewRenderTarget;
		bGeneratedPreviewOwnsRenderTarget = true;
	}
	else
	{
		GeneratedPreviewRenderTarget = CaptureComponent->TextureTarget;
		bGeneratedPreviewOwnsRenderTarget = false;
	}

	// SceneCapture2D는 일반 플레이어 카메라와 달리 Lumen이 기본적으로 비활성화될 수 있다.
	// 특히 bCaptureEveryFrame=false 이고 bAlwaysPersistRenderingState=false 이면
	// Lumen의 persistent view state가 없어져 간접광/반사가 검게 보일 수 있다.
	GeneratedPreviewCaptureComponent = CaptureComponent;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;

	// 이 전용 프리뷰 캡처는 실제 게임 화면과 같은 Lumen GI/Reflection을 사용한다.
	CaptureComponent->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
	CaptureComponent->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
	CaptureComponent->PostProcessSettings.bOverride_ReflectionMethod = true;
	CaptureComponent->PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;

	// 첫 프레임도 즉시 채우고, 이후에는 옵션 창이 열린 동안 계속 갱신한다.
	CaptureComponent->CaptureScene();

	FSlateBrush Brush = PreviewImage->GetBrush();
	Brush.SetResourceObject(CaptureComponent->TextureTarget);
	Brush.ImageSize = FVector2D(640.0f, 360.0f);
	PreviewImage->SetBrush(Brush);
	PreviewImage->SetVisibility(ESlateVisibility::Visible);

	if (PreviewStatus)
	{
		PreviewStatus->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UShooterOptionsScreen::ReleaseGeneratedPreviewCapture()
{
	USceneCaptureComponent2D* CaptureComponent = GeneratedPreviewCaptureComponent.Get();
	if (!CaptureComponent)
	{
		return;
	}

	// OptionsPreview 전용 SceneCapture이므로 옵션 화면이 닫히면 지속 캡처 비용을 끈다.
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = false;

	if (bGeneratedPreviewOwnsRenderTarget && CaptureComponent->TextureTarget == GeneratedPreviewRenderTarget)
	{
		CaptureComponent->TextureTarget = nullptr;
	}

	bGeneratedPreviewOwnsRenderTarget = false;
	GeneratedPreviewCaptureComponent.Reset();
}

void UShooterOptionsScreen::RefreshGeneratedDetailPanel()
{
	FText Category = FText::FromString(TEXT("GRAPHICS"));
	FText Title = FText::FromString(TEXT("전체 품질"));
	FText Current = FText::FromString(TEXT("현재 -"));
	FText Body = FText::FromString(TEXT("선택한 옵션의 동작과 현재 값을 확인합니다."));
	FText Help = FText::GetEmpty();

	FText MetricLabel0 = FText::FromString(TEXT("현재 값"));
	FText MetricValue0 = FText::FromString(TEXT("-"));
	FText MetricLabel1 = FText::FromString(TEXT("범위 / 선택"));
	FText MetricValue1 = FText::FromString(TEXT("-"));
	FText MetricLabel2 = FText::FromString(TEXT("적용"));
	FText MetricValue2 = FText::FromString(TEXT("-"));

	int32 PerformanceSegments = 0;
	bool bPerformanceRelevant = false;
	const FName Option = ActiveGeneratedOption;

	const auto QualityImpact = [](const int32 Level, const int32 MaxSegments = 6)
	{
		const int32 Clamped = FMath::Clamp(Level, 0, 4);
		return FMath::Clamp(FMath::RoundToInt(1.0f + (static_cast<float>(Clamped) / 4.0f) * (MaxSegments - 1)), 1, MaxSegments);
	};

	if (Option == FName(TEXT("Resolution")))
	{
		const FText ResolutionText = FormatResolution(GetScreenResolution());
		Category = FText::FromString(TEXT("VIDEO"));
		Title = FText::FromString(TEXT("해상도"));
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), ResolutionText);
		Body = FText::FromString(TEXT("게임이 출력되는 화면의 픽셀 수를 정합니다. 값이 높을수록 화면은 선명해지지만 GPU가 렌더링해야 하는 픽셀 수도 증가합니다."));
		Help = FText::GetEmpty();
		MetricValue0 = ResolutionText;
		MetricLabel1 = FText::FromString(TEXT("지원 해상도"));
		MetricValue1 = FText::AsNumber(GetSupportedResolutions().Num());
		MetricLabel2 = FText::FromString(TEXT("변경 방식"));
		MetricValue2 = FText::FromString(TEXT("15초 확인 후 확정"));
	}
	else if (Option == FName(TEXT("WindowMode")))
	{
		const FText ModeText = GetWindowModeDisplayText(GetWindowMode());
		Category = FText::FromString(TEXT("VIDEO"));
		Title = FText::FromString(TEXT("화면 모드"));
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), ModeText);
		Body = FText::FromString(TEXT("복잡한 중간 모드를 제거하고 전체 화면과 창 모드 두 가지로만 구성했습니다."));
		Help = FText::GetEmpty();
		MetricValue0 = ModeText;
		MetricLabel1 = FText::FromString(TEXT("선택"));
		MetricValue1 = FText::FromString(TEXT("전체 화면 / 창 모드"));
		MetricLabel2 = FText::FromString(TEXT("변경 방식"));
		MetricValue2 = FText::FromString(TEXT("15초 확인 후 확정"));
	}
	else if (Option == FName(TEXT("ResolutionScale")))
	{
		const float Scale = GetResolutionScalePercent();
		const FText ScaleText = FText::FromString(FString::Printf(TEXT("%.0f%%"), Scale));
		Category = FText::FromString(TEXT("VIDEO"));
		Title = FText::FromString(TEXT("해상도 스케일"));
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), ScaleText);
		Body = FText::FromString(TEXT("출력 해상도는 그대로 두고 실제 3D 장면을 렌더링하는 내부 해상도를 조절합니다."));
		Help = FText::FromString(TEXT("낮추면 GPU 부담이 줄어드는 대신 이미지 선명도가 떨어질 수 있습니다."));
		MetricValue0 = ScaleText;
		MetricLabel1 = FText::FromString(TEXT("권장 기준"));
		MetricValue1 = FText::FromString(TEXT("화질 우선 100%"));
		MetricLabel2 = FText::FromString(TEXT("주요 영향"));
		MetricValue2 = FText::FromString(TEXT("GPU / 선명도"));
	}
	else if (Option == FName(TEXT("VSync")))
	{
		const FText Value = FText::FromString(IsVSyncEnabled() ? TEXT("켜짐") : TEXT("꺼짐"));
		Category = FText::FromString(TEXT("VIDEO"));
		Title = FText::FromString(TEXT("수직 동기화"));
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		Body = FText::FromString(TEXT("프레임 출력 시점을 모니터 주사율과 맞춰 화면 찢어짐을 줄입니다."));
		Help = FText::FromString(TEXT("경쟁 플레이에서는 입력 지연을 줄이기 위해 끄는 경우가 많지만, 화면 찢어짐이 거슬리면 켜는 편이 낫습니다."));
		MetricValue0 = Value;
		MetricLabel1 = FText::FromString(TEXT("장점"));
		MetricValue1 = FText::FromString(TEXT("화면 찢어짐 감소"));
		MetricLabel2 = FText::FromString(TEXT("주의"));
		MetricValue2 = FText::FromString(TEXT("입력 지연 가능"));
	}
	else if (Option == FName(TEXT("FrameRate")))
	{
		const FText Value = GetFrameRateLimitDisplayText(GetFrameRateLimit());
		Category = FText::FromString(TEXT("VIDEO"));
		Title = FText::FromString(TEXT("FPS 제한"));
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		Body = FText::FromString(TEXT("게임이 출력하려는 최대 프레임 수를 제한해 발열, 전력 사용량, 프레임 변동 폭을 조절합니다."));
		Help = FText::FromString(TEXT("UNLIMITED는 별도의 제한을 두지 않습니다. 보통 모니터 주사율과 시스템 성능을 기준으로 선택합니다."));
		MetricValue0 = Value;
		MetricLabel1 = FText::FromString(TEXT("선택 수"));
		MetricValue1 = FText::FromString(TEXT("8 단계"));
		MetricLabel2 = FText::FromString(TEXT("적용"));
		MetricValue2 = FText::FromString(TEXT("즉시 / 로컬"));
	}
	else if (Option == FName(TEXT("DynamicResolution")))
	{
		const FText Value = FText::FromString(IsDynamicResolutionEnabled() ? TEXT("켜짐") : TEXT("꺼짐"));
		Category = FText::FromString(TEXT("VIDEO"));
		Title = FText::FromString(TEXT("다이내믹 레졸루션"));
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		Body = FText::FromString(TEXT("성능 목표를 유지하기 위해 내부 렌더 해상도를 상황에 따라 자동으로 조절합니다."));
		Help = FText::FromString(TEXT("프레임 유지가 우선일 때 사용하면 장면 복잡도에 따라 내부 해상도를 자동으로 조절합니다."));
		MetricValue0 = Value;
		MetricLabel1 = FText::FromString(TEXT("목적"));
		MetricValue1 = FText::FromString(TEXT("프레임 안정화"));
		MetricLabel2 = FText::FromString(TEXT("변화"));
		MetricValue2 = FText::FromString(TEXT("내부 해상도 자동"));
	}
	else if (Option == FName(TEXT("Overall")))
	{
		const int32 Level = GetOverallQualityLevel();
		const FText Value = GetQualityDisplayText(Level);
		Category = FText::FromString(TEXT("GRAPHICS"));
		Title = FText::FromString(TEXT("전체 품질"));
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		Body = FText::FromString(TEXT("시야 거리, 그림자, 조명, 반사, 텍스처 등 여러 그래픽 품질을 하나의 프리셋으로 동시에 조절합니다."));
		Help = FText::GetEmpty();
		PerformanceSegments = Level < 0 ? 3 : QualityImpact(Level);
		bPerformanceRelevant = true;
	}
	else if (Option == FName(TEXT("ViewDistance")) || Option == FName(TEXT("AntiAliasing")) ||
		Option == FName(TEXT("Shadow")) || Option == FName(TEXT("GlobalIllumination")) ||
		Option == FName(TEXT("Reflection")) || Option == FName(TEXT("PostProcessing")) ||
		Option == FName(TEXT("Texture")) || Option == FName(TEXT("Effects")) ||
		Option == FName(TEXT("Foliage")) || Option == FName(TEXT("Shading")))
	{
		EShooterScalabilitySetting Setting = EShooterScalabilitySetting::ViewDistance;
		FString TitleText = TEXT("시야 거리");
		FString BodyText = TEXT("멀리 있는 오브젝트와 LOD가 어느 거리까지 높은 품질로 유지되는지에 영향을 줍니다.");
		FString HelpText = TEXT("높이면 원거리 장면이 풍부해지지만 더 많은 오브젝트를 처리하게 되어 CPU/GPU 비용이 증가할 수 있습니다.");
		int32 MaxImpact = 5;

		if (Option == FName(TEXT("AntiAliasing")))
		{
			Setting = EShooterScalabilitySetting::AntiAliasing;
			TitleText = TEXT("안티앨리어싱");
			BodyText = TEXT("오브젝트 가장자리의 계단 현상과 반짝임을 줄이는 품질입니다.");
			HelpText = TEXT("높을수록 가장자리가 안정적으로 보이지만 후처리 비용과 이미지 특성이 달라질 수 있습니다.");
		}
		else if (Option == FName(TEXT("Shadow")))
		{
			Setting = EShooterScalabilitySetting::Shadow;
			TitleText = TEXT("그림자 품질");
			BodyText = TEXT("그림자의 해상도, 표시 거리, 세부 표현 품질에 영향을 줍니다.");
			HelpText = TEXT("낮추면 GPU 비용을 크게 줄일 수 있지만 먼 거리의 그림자나 세부 그림자가 단순해질 수 있습니다.");
			MaxImpact = 6;
		}
		else if (Option == FName(TEXT("GlobalIllumination")))
		{
			Setting = EShooterScalabilitySetting::GlobalIllumination;
			TitleText = TEXT("글로벌 일루미네이션");
			BodyText = TEXT("직접광이 표면에 반사되어 주변을 밝히는 간접광의 품질을 조절합니다.");
			HelpText = TEXT("Lumen 기반 장면에서는 실내외 밝기와 색 번짐 품질에 큰 영향을 주며 GPU 비용도 높은 편입니다.");
			MaxImpact = 6;
		}
		else if (Option == FName(TEXT("Reflection")))
		{
			Setting = EShooterScalabilitySetting::Reflection;
			TitleText = TEXT("반사 품질");
			BodyText = TEXT("금속, 유리, 젖은 바닥 등 표면에 보이는 주변 환경 반사의 정밀도를 조절합니다.");
			HelpText = TEXT("높이면 반사 디테일과 안정성이 좋아지지만 Lumen/스크린 반사 계산 비용이 증가할 수 있습니다.");
			MaxImpact = 6;
		}
		else if (Option == FName(TEXT("PostProcessing")))
		{
			Setting = EShooterScalabilitySetting::PostProcessing;
			TitleText = TEXT("포스트 프로세싱");
			BodyText = TEXT("블룸, 앰비언트 오클루전, 색보정 등 최종 화면 후처리 품질에 영향을 줍니다.");
			HelpText = TEXT("경쟁 게임에서는 과한 후처리를 낮춰 시인성과 프레임 안정성을 우선하기도 합니다.");
			MaxImpact = 5;
		}
		else if (Option == FName(TEXT("Texture")))
		{
			Setting = EShooterScalabilitySetting::Texture;
			TitleText = TEXT("텍스처 품질");
			BodyText = TEXT("표면 이미지의 밉 레벨과 스트리밍 품질을 조절해 디테일과 VRAM 사용량에 영향을 줍니다.");
			HelpText = TEXT("GPU 연산량보다 VRAM 사용량에 더 직접적인 영향을 주는 옵션입니다.");
			MaxImpact = 4;
		}
		else if (Option == FName(TEXT("Effects")))
		{
			Setting = EShooterScalabilitySetting::Effects;
			TitleText = TEXT("이펙트 품질");
			BodyText = TEXT("파티클, 일부 시각 효과와 관련된 품질 및 표현량에 영향을 줍니다.");
			HelpText = TEXT("전투 중 이펙트가 많은 장면에서 프레임 비용 차이가 커질 수 있습니다.");
			MaxImpact = 5;
		}
		else if (Option == FName(TEXT("Foliage")))
		{
			Setting = EShooterScalabilitySetting::Foliage;
			TitleText = TEXT("폴리지 품질");
			BodyText = TEXT("풀, 나무 같은 식생의 밀도와 표시 거리를 조절합니다.");
			HelpText = TEXT("식생이 많은 맵에서는 인스턴스 수와 오버드로우에 큰 영향을 줄 수 있습니다.");
			MaxImpact = 6;
		}
		else if (Option == FName(TEXT("Shading")))
		{
			Setting = EShooterScalabilitySetting::Shading;
			TitleText = TEXT("셰이딩 품질");
			BodyText = TEXT("머티리얼과 조명 계산에서 사용되는 일부 셰이딩 품질을 조절합니다.");
			HelpText = TEXT("복잡한 재질이 많은 장면일수록 품질 단계에 따른 비용 차이가 커질 수 있습니다.");
			MaxImpact = 5;
		}

		const int32 Level = GetQualityLevel(Setting);
		const FText Value = GetQualityDisplayText(Level);
		Category = FText::FromString(TEXT("GRAPHICS"));
		Title = FText::FromString(TitleText);
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		Body = FText::FromString(BodyText);
		Help = FText::FromString(HelpText);
		PerformanceSegments = QualityImpact(Level, MaxImpact);
		bPerformanceRelevant = true;
	}
	else if (Option == FName(TEXT("HorizontalSensitivity")) || Option == FName(TEXT("VerticalSensitivity")) ||
		Option == FName(TEXT("AimSensitivity")) || Option == FName(TEXT("InvertY")) ||
		Option == FName(TEXT("DynamicCrosshair")))
	{
		Category = FText::FromString(TEXT("CONTROLS"));
		FText Value = FText::FromString(TEXT("-"));
		if (Option == FName(TEXT("HorizontalSensitivity")))
		{
			Title = FText::FromString(TEXT("수평 감도"));
			Body = FText::FromString(TEXT("마우스 X축 회전 속도를 조절합니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.2f"), GetMouseHorizontalSensitivity()));
			MetricLabel1 = FText::FromString(TEXT("범위"));
			MetricValue1 = FText::FromString(TEXT("0.01 ~ 10.00"));
		}
		else if (Option == FName(TEXT("VerticalSensitivity")))
		{
			Title = FText::FromString(TEXT("수직 감도"));
			Body = FText::FromString(TEXT("마우스 Y축 회전 속도를 조절합니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.2f"), GetMouseVerticalSensitivity()));
			MetricLabel1 = FText::FromString(TEXT("범위"));
			MetricValue1 = FText::FromString(TEXT("0.01 ~ 10.00"));
		}
		else if (Option == FName(TEXT("AimSensitivity")))
		{
			Title = FText::FromString(TEXT("ADS 감도 배율"));
			Body = FText::FromString(TEXT("조준 중 기본 마우스 감도에 곱해지는 배율입니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.2f"), GetAimSensitivityMultiplier()));
			MetricLabel1 = FText::FromString(TEXT("범위"));
			MetricValue1 = FText::FromString(TEXT("0.01 ~ 2.00"));
		}
		else if (Option == FName(TEXT("InvertY")))
		{
			Title = FText::FromString(TEXT("Y축 반전"));
			Body = FText::FromString(TEXT("마우스의 상하 입력 방향을 반대로 적용합니다."));
			Value = FText::FromString(IsInvertVerticalLookEnabled() ? TEXT("켜짐") : TEXT("꺼짐"));
			MetricLabel1 = FText::FromString(TEXT("선택"));
			MetricValue1 = FText::FromString(TEXT("켜짐 / 꺼짐"));
		}
		else
		{
			Title = FText::FromString(TEXT("동적 크로스헤어"));
			Body = FText::FromString(TEXT("이동과 사격 상태에 따라 크로스헤어 확산 표현을 동적으로 표시합니다."));
			Value = FText::FromString(IsDynamicCrosshairEnabled() ? TEXT("켜짐") : TEXT("꺼짐"));
			MetricLabel1 = FText::FromString(TEXT("선택"));
			MetricValue1 = FText::FromString(TEXT("켜짐 / 꺼짐"));
		}
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		Help = FText::GetEmpty();
		MetricValue0 = Value;
		MetricLabel2 = FText::FromString(TEXT("적용"));
		MetricValue2 = FText::FromString(TEXT("즉시 / 로컬 플레이어"));
	}
	else
	{
		Category = FText::FromString(TEXT("AUDIO"));
		FText Value = FText::FromString(TEXT("-"));
		if (Option == FName(TEXT("MasterVolume")))
		{
			Title = FText::FromString(TEXT("마스터 볼륨"));
			Body = FText::FromString(TEXT("게임에서 들리는 모든 소리의 전체 크기를 조절합니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.0f%%"), GetAudioVolume(EShooterAudioChannel::Master) * 100.0f));
			Help = FText::GetEmpty();
			MetricLabel1 = FText::FromString(TEXT("대상"));
			MetricValue1 = FText::FromString(TEXT("모든 게임 사운드"));
			MetricLabel2 = FText::FromString(TEXT("반영"));
			MetricValue2 = FText::FromString(TEXT("즉시"));
		}
		else if (Option == FName(TEXT("MusicVolume")))
		{
			Title = FText::FromString(TEXT("음악 볼륨"));
			Body = FText::FromString(TEXT("배경 음악에 분류된 사운드의 볼륨을 조절합니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.0f%%"), GetAudioVolume(EShooterAudioChannel::Music) * 100.0f));
			Help = FText::GetEmpty();
			MetricLabel1 = FText::FromString(TEXT("대상"));
			MetricValue1 = FText::FromString(TEXT("배경 음악"));
			MetricLabel2 = FText::FromString(TEXT("범위"));
			MetricValue2 = FText::FromString(TEXT("0 ~ 100%"));
		}
		else if (Option == FName(TEXT("SfxVolume")))
		{
			Title = FText::FromString(TEXT("효과음 볼륨"));
			Body = FText::FromString(TEXT("총기, 발소리, 충돌음 등 효과음 채널의 볼륨을 조절합니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.0f%%"), GetAudioVolume(EShooterAudioChannel::SoundEffects) * 100.0f));
			Help = FText::GetEmpty();
			MetricLabel1 = FText::FromString(TEXT("대상"));
			MetricValue1 = FText::FromString(TEXT("총기 / 발소리 / 효과음"));
			MetricLabel2 = FText::FromString(TEXT("범위"));
			MetricValue2 = FText::FromString(TEXT("0 ~ 100%"));
		}
		else if (Option == FName(TEXT("UiVolume")))
		{
			Title = FText::FromString(TEXT("UI 볼륨"));
			Body = FText::FromString(TEXT("버튼 클릭음과 메뉴 피드백 등 UI 사운드의 볼륨을 조절합니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.0f%%"), GetAudioVolume(EShooterAudioChannel::UserInterface) * 100.0f));
			Help = FText::GetEmpty();
			MetricLabel1 = FText::FromString(TEXT("대상"));
			MetricValue1 = FText::FromString(TEXT("메뉴 / 알림 사운드"));
			MetricLabel2 = FText::FromString(TEXT("범위"));
			MetricValue2 = FText::FromString(TEXT("0 ~ 100%"));
		}
		else if (Option == FName(TEXT("VoiceVolume")))
		{
			Title = FText::FromString(TEXT("음성 볼륨"));
			Body = FText::FromString(TEXT("다른 플레이어의 음성 채팅 재생 볼륨을 조절합니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.0f%%"), GetAudioVolume(EShooterAudioChannel::Voice) * 100.0f));
			Help = FText::FromString(TEXT("마이크 입력 감도가 아니라 다른 플레이어에게서 들리는 음성 크기입니다."));
			MetricLabel1 = FText::FromString(TEXT("대상"));
			MetricValue1 = FText::FromString(TEXT("상대 플레이어 음성"));
			MetricLabel2 = FText::FromString(TEXT("마이크 감도"));
			MetricValue2 = FText::FromString(TEXT("영향 없음"));
		}
		else if (Option == FName(TEXT("MasterMute")))
		{
			Title = FText::FromString(TEXT("전체 음소거"));
			Body = FText::FromString(TEXT("게임의 전체 오디오 출력을 즉시 0으로 만듭니다."));
			Value = FText::FromString(IsMasterAudioMuted() ? TEXT("켜짐") : TEXT("꺼짐"));
			Help = FText::FromString(TEXT("음소거를 해제하면 저장된 마스터/채널 볼륨 값으로 복구됩니다."));
			MetricLabel1 = FText::FromString(TEXT("적용 범위"));
			MetricValue1 = FText::FromString(TEXT("전체 출력"));
			MetricLabel2 = FText::FromString(TEXT("복구"));
			MetricValue2 = FText::FromString(TEXT("기존 볼륨 유지"));
		}
		else
		{
			Title = FText::FromString(TEXT("음성 채팅"));
			Body = FText::FromString(TEXT("마이크 캡처와 다른 플레이어의 음성 패킷 재생을 함께 켜거나 끕니다."));
			Value = FText::FromString(IsVoiceChatEnabled() ? TEXT("켜짐") : TEXT("꺼짐"));
			Help = FText::FromString(TEXT("OFF 시 현재 캡처/재생을 중단하고 새 음성 처리를 막습니다. 음성 볼륨 슬라이더는 수신 음성 크기만 조절합니다."));
			MetricLabel1 = FText::FromString(TEXT("OFF 시"));
			MetricValue1 = FText::FromString(TEXT("캡처 + 재생 중단"));
			MetricLabel2 = FText::FromString(TEXT("적용"));
			MetricValue2 = FText::FromString(TEXT("즉시"));
		}

		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		MetricValue0 = Value;
	}

	SetGeneratedText(TEXT("TXT_DetailCategory"), Category);
	SetGeneratedText(TEXT("TXT_DetailTitle"), Title);
	SetGeneratedText(TEXT("TXT_DetailCurrentValue"), Current);
	SetGeneratedText(TEXT("TXT_DetailBody"), Body);
	SetGeneratedText(TEXT("TXT_DetailHelp"), Help);
	SetGeneratedText(TEXT("TXT_ContextLabel0"), MetricLabel0);
	SetGeneratedText(TEXT("TXT_ContextValue0"), MetricValue0);
	SetGeneratedText(TEXT("TXT_ContextLabel1"), MetricLabel1);
	SetGeneratedText(TEXT("TXT_ContextValue1"), MetricValue1);
	SetGeneratedText(TEXT("TXT_ContextLabel2"), MetricLabel2);
	SetGeneratedText(TEXT("TXT_ContextValue2"), MetricValue2);

	const bool bGraphicsPage = ActiveGeneratedPage == 0;
	if (UWidgetSwitcher* DetailVisual = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("Switcher_DetailVisual"))))
	{
		DetailVisual->SetActiveWidgetIndex(bGraphicsPage ? 0 : 1);
	}

	SetGeneratedPerformanceSegments(PerformanceSegments, bGraphicsPage && bPerformanceRelevant);
	RefreshGeneratedPreview();
}


void UShooterOptionsScreen::StepResolution(const int32 Delta)
{
	TArray<FIntPoint> Resolutions = GetSupportedResolutions();
	if (Resolutions.IsEmpty())
	{
		return;
	}

	const FIntPoint Current = GetScreenResolution();
	int32 Index = Resolutions.IndexOfByKey(Current);
	if (Index == INDEX_NONE)
	{
		Index = 0;
	}
	Index = (Index + Delta + Resolutions.Num()) % Resolutions.Num();
	PreviewVideoMode(Resolutions[Index], GetWindowMode());
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::StepWindowMode(const int32 Delta)
{
	const EShooterWindowMode Modes[] =
	{
		EShooterWindowMode::Fullscreen,
		EShooterWindowMode::Windowed,
	};

	int32 Index = GetWindowMode() == EShooterWindowMode::Windowed ? 1 : 0;
	Index = (Index + Delta + UE_ARRAY_COUNT(Modes)) % UE_ARRAY_COUNT(Modes);
	PreviewVideoMode(GetScreenResolution(), Modes[Index]);
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::StepFrameRate(const int32 Delta)
{
	static const float Limits[] = {0.0f, 30.0f, 60.0f, 120.0f, 144.0f, 165.0f, 240.0f, 360.0f};
	const float Current = GetFrameRateLimit();
	int32 BestIndex = 0;
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Limits); ++Index)
	{
		const float Distance = FMath::Abs(Current - Limits[Index]);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestIndex = Index;
		}
	}
	BestIndex = (BestIndex + Delta + UE_ARRAY_COUNT(Limits)) % UE_ARRAY_COUNT(Limits);
	SetFrameRateLimit(Limits[BestIndex]);
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::StepOverallQuality(const int32 Delta)
{
	int32 Level = GetOverallQualityLevel();
	if (Level < 0 || Level > 4)
	{
		Level = 2;
	}
	Level = (Level + Delta + 5) % 5;
	SetOverallQualityLevel(Level);
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::StepQuality(const EShooterScalabilitySetting Setting, const int32 Delta)
{
	int32 Level = FMath::Clamp(GetQualityLevel(Setting), 0, 4);
	Level = (Level + Delta + 5) % 5;
	SetQualityLevel(Setting, Level);
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::HandleSideGraphics() { ActiveGeneratedSideButton = TEXT("Btn_Side_Graphics"); ActiveGeneratedOption = TEXT("Overall"); ShowGeneratedPage(0); }
void UShooterOptionsScreen::HandleSideDisplay() { ActiveGeneratedSideButton = TEXT("Btn_Side_Display"); ActiveGeneratedOption = TEXT("Resolution"); ShowGeneratedPage(1); }
void UShooterOptionsScreen::HandleSideControls() { ActiveGeneratedSideButton = TEXT("Btn_Side_Controls"); ActiveGeneratedOption = TEXT("HorizontalSensitivity"); ShowGeneratedPage(2); }
void UShooterOptionsScreen::HandleSideAudio() { ActiveGeneratedSideButton = TEXT("Btn_Side_Audio"); ActiveGeneratedOption = TEXT("MasterVolume"); ShowGeneratedPage(3); }
void UShooterOptionsScreen::HandleSave() { SaveOptions(); }
void UShooterOptionsScreen::HandleBack() { RequestClose(); }
void UShooterOptionsScreen::HandleReset() { ResetAllSettingsToDefaults(); RefreshGeneratedUI(); }
void UShooterOptionsScreen::HandleVideoKeep() { ConfirmVideoMode(); RefreshGeneratedUI(); }
void UShooterOptionsScreen::HandleVideoRevert() { RevertVideoMode(); RefreshGeneratedUI(); }

void UShooterOptionsScreen::HandleResolutionPrev() { ActiveGeneratedOption = TEXT("Resolution"); StepResolution(-1); }
void UShooterOptionsScreen::HandleResolutionNext() { ActiveGeneratedOption = TEXT("Resolution"); StepResolution(1); }
void UShooterOptionsScreen::HandleWindowModePrev() { ActiveGeneratedOption = TEXT("WindowMode"); StepWindowMode(-1); }
void UShooterOptionsScreen::HandleWindowModeNext() { ActiveGeneratedOption = TEXT("WindowMode"); StepWindowMode(1); }
void UShooterOptionsScreen::HandleWindowModeFullscreen()
{
	ActiveGeneratedOption = TEXT("WindowMode");
	PreviewVideoMode(GetScreenResolution(), EShooterWindowMode::Fullscreen);
	RefreshGeneratedUI();
}
void UShooterOptionsScreen::HandleWindowModeWindowed()
{
	ActiveGeneratedOption = TEXT("WindowMode");
	PreviewVideoMode(GetScreenResolution(), EShooterWindowMode::Windowed);
	RefreshGeneratedUI();
}
void UShooterOptionsScreen::HandleFrameRatePrev() { ActiveGeneratedOption = TEXT("FrameRate"); StepFrameRate(-1); }
void UShooterOptionsScreen::HandleFrameRateNext() { ActiveGeneratedOption = TEXT("FrameRate"); StepFrameRate(1); }
void UShooterOptionsScreen::HandleOverallPrev() { ActiveGeneratedOption = TEXT("Overall"); StepOverallQuality(-1); }
void UShooterOptionsScreen::HandleOverallNext() { ActiveGeneratedOption = TEXT("Overall"); StepOverallQuality(1); }
void UShooterOptionsScreen::HandleViewDistancePrev() { ActiveGeneratedOption = TEXT("ViewDistance"); StepQuality(EShooterScalabilitySetting::ViewDistance, -1); }
void UShooterOptionsScreen::HandleViewDistanceNext() { ActiveGeneratedOption = TEXT("ViewDistance"); StepQuality(EShooterScalabilitySetting::ViewDistance, 1); }
void UShooterOptionsScreen::HandleAntiAliasingPrev() { ActiveGeneratedOption = TEXT("AntiAliasing"); StepQuality(EShooterScalabilitySetting::AntiAliasing, -1); }
void UShooterOptionsScreen::HandleAntiAliasingNext() { ActiveGeneratedOption = TEXT("AntiAliasing"); StepQuality(EShooterScalabilitySetting::AntiAliasing, 1); }
void UShooterOptionsScreen::HandleShadowPrev() { ActiveGeneratedOption = TEXT("Shadow"); StepQuality(EShooterScalabilitySetting::Shadow, -1); }
void UShooterOptionsScreen::HandleShadowNext() { ActiveGeneratedOption = TEXT("Shadow"); StepQuality(EShooterScalabilitySetting::Shadow, 1); }
void UShooterOptionsScreen::HandleGlobalIlluminationPrev() { ActiveGeneratedOption = TEXT("GlobalIllumination"); StepQuality(EShooterScalabilitySetting::GlobalIllumination, -1); }
void UShooterOptionsScreen::HandleGlobalIlluminationNext() { ActiveGeneratedOption = TEXT("GlobalIllumination"); StepQuality(EShooterScalabilitySetting::GlobalIllumination, 1); }
void UShooterOptionsScreen::HandleReflectionPrev() { ActiveGeneratedOption = TEXT("Reflection"); StepQuality(EShooterScalabilitySetting::Reflection, -1); }
void UShooterOptionsScreen::HandleReflectionNext() { ActiveGeneratedOption = TEXT("Reflection"); StepQuality(EShooterScalabilitySetting::Reflection, 1); }
void UShooterOptionsScreen::HandlePostProcessingPrev() { ActiveGeneratedOption = TEXT("PostProcessing"); StepQuality(EShooterScalabilitySetting::PostProcessing, -1); }
void UShooterOptionsScreen::HandlePostProcessingNext() { ActiveGeneratedOption = TEXT("PostProcessing"); StepQuality(EShooterScalabilitySetting::PostProcessing, 1); }
void UShooterOptionsScreen::HandleTexturePrev() { ActiveGeneratedOption = TEXT("Texture"); StepQuality(EShooterScalabilitySetting::Texture, -1); }
void UShooterOptionsScreen::HandleTextureNext() { ActiveGeneratedOption = TEXT("Texture"); StepQuality(EShooterScalabilitySetting::Texture, 1); }
void UShooterOptionsScreen::HandleEffectsPrev() { ActiveGeneratedOption = TEXT("Effects"); StepQuality(EShooterScalabilitySetting::Effects, -1); }
void UShooterOptionsScreen::HandleEffectsNext() { ActiveGeneratedOption = TEXT("Effects"); StepQuality(EShooterScalabilitySetting::Effects, 1); }
void UShooterOptionsScreen::HandleFoliagePrev() { ActiveGeneratedOption = TEXT("Foliage"); StepQuality(EShooterScalabilitySetting::Foliage, -1); }
void UShooterOptionsScreen::HandleFoliageNext() { ActiveGeneratedOption = TEXT("Foliage"); StepQuality(EShooterScalabilitySetting::Foliage, 1); }
void UShooterOptionsScreen::HandleShadingPrev() { ActiveGeneratedOption = TEXT("Shading"); StepQuality(EShooterScalabilitySetting::Shading, -1); }
void UShooterOptionsScreen::HandleShadingNext() { ActiveGeneratedOption = TEXT("Shading"); StepQuality(EShooterScalabilitySetting::Shading, 1); }

void UShooterOptionsScreen::HandleInfoResolution()
{
	ActiveGeneratedOption = TEXT("Resolution");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoWindowMode()
{
	ActiveGeneratedOption = TEXT("WindowMode");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoFrameRate()
{
	ActiveGeneratedOption = TEXT("FrameRate");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoOverall()
{
	ActiveGeneratedOption = TEXT("Overall");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoViewDistance()
{
	ActiveGeneratedOption = TEXT("ViewDistance");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoAntiAliasing()
{
	ActiveGeneratedOption = TEXT("AntiAliasing");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoShadow()
{
	ActiveGeneratedOption = TEXT("Shadow");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoGlobalIllumination()
{
	ActiveGeneratedOption = TEXT("GlobalIllumination");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoReflection()
{
	ActiveGeneratedOption = TEXT("Reflection");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoPostProcessing()
{
	ActiveGeneratedOption = TEXT("PostProcessing");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoTexture()
{
	ActiveGeneratedOption = TEXT("Texture");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoEffects()
{
	ActiveGeneratedOption = TEXT("Effects");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoFoliage()
{
	ActiveGeneratedOption = TEXT("Foliage");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleInfoShading()
{
	ActiveGeneratedOption = TEXT("Shading");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleVSyncToggle() { ActiveGeneratedOption = TEXT("VSync"); SetVSyncEnabled(!IsVSyncEnabled()); RefreshGeneratedUI(); }
void UShooterOptionsScreen::HandleDynamicResolutionToggle() { ActiveGeneratedOption = TEXT("DynamicResolution"); SetDynamicResolutionEnabled(!IsDynamicResolutionEnabled()); RefreshGeneratedUI(); }
void UShooterOptionsScreen::HandleInvertYToggle() { ActiveGeneratedOption = TEXT("InvertY"); SetInvertVerticalLookEnabled(!IsInvertVerticalLookEnabled()); RefreshGeneratedUI(); }
void UShooterOptionsScreen::HandleDynamicCrosshairToggle() { ActiveGeneratedOption = TEXT("DynamicCrosshair"); SetDynamicCrosshairEnabled(!IsDynamicCrosshairEnabled()); RefreshGeneratedUI(); }
void UShooterOptionsScreen::HandleMasterMuteToggle() { ActiveGeneratedOption = TEXT("MasterMute"); SetMasterAudioMuted(!IsMasterAudioMuted()); RefreshGeneratedUI(); }
void UShooterOptionsScreen::HandleVoiceChatToggle() { ActiveGeneratedOption = TEXT("VoiceChat"); SetVoiceChatEnabled(!IsVoiceChatEnabled()); RefreshGeneratedUI(); }

void UShooterOptionsScreen::HandleResolutionScaleChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("ResolutionScale");
	SetResolutionScalePercent(Value);
	SetGeneratedText(TEXT("TXT_ResolutionScaleValue"), FText::FromString(FString::Printf(TEXT("%.0f%%"), Value)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleHorizontalSensitivityChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("HorizontalSensitivity");
	SetMouseHorizontalSensitivity(Value);
	SetGeneratedText(TEXT("TXT_HorizontalSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleVerticalSensitivityChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("VerticalSensitivity");
	SetMouseVerticalSensitivity(Value);
	SetGeneratedText(TEXT("TXT_VerticalSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleAimSensitivityChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("AimSensitivity");
	SetAimSensitivityMultiplier(Value);
	SetGeneratedText(TEXT("TXT_AimSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleMasterVolumeChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("MasterVolume");
	SetAudioVolume(EShooterAudioChannel::Master, Value);
	SetGeneratedText(TEXT("TXT_MasterVolumeValue"), FText::FromString(FString::Printf(TEXT("%.0f"), Value * 100.0f)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleMusicVolumeChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("MusicVolume");
	SetAudioVolume(EShooterAudioChannel::Music, Value);
	SetGeneratedText(TEXT("TXT_MusicVolumeValue"), FText::FromString(FString::Printf(TEXT("%.0f"), Value * 100.0f)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleSfxVolumeChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("SfxVolume");
	SetAudioVolume(EShooterAudioChannel::SoundEffects, Value);
	SetGeneratedText(TEXT("TXT_SfxVolumeValue"), FText::FromString(FString::Printf(TEXT("%.0f"), Value * 100.0f)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleUiVolumeChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("UiVolume");
	SetAudioVolume(EShooterAudioChannel::UserInterface, Value);
	SetGeneratedText(TEXT("TXT_UiVolumeValue"), FText::FromString(FString::Printf(TEXT("%.0f"), Value * 100.0f)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleVoiceVolumeChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("VoiceVolume");
	SetAudioVolume(EShooterAudioChannel::Voice, Value);
	SetGeneratedText(TEXT("TXT_VoiceVolumeValue"), FText::FromString(FString::Printf(TEXT("%.0f"), Value * 100.0f)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}


UShooterGameUserSettings* UShooterOptionsScreen::GetGameUserSettings() const
{
	return UShooterGameUserSettings::Get();
}

EShooterWindowMode UShooterOptionsScreen::FromEngineWindowMode(
	const EWindowMode::Type WindowMode)
{
	switch (WindowMode)
	{
	case EWindowMode::Fullscreen:
	case EWindowMode::WindowedFullscreen:
		return EShooterWindowMode::Fullscreen;
	case EWindowMode::Windowed:
	default:
		return EShooterWindowMode::Windowed;
	}
}

EWindowMode::Type UShooterOptionsScreen::ToEngineWindowMode(
	const EShooterWindowMode WindowMode)
{
	switch (WindowMode)
	{
	case EShooterWindowMode::Fullscreen:
	case EShooterWindowMode::WindowedFullscreen:
		return EWindowMode::Fullscreen;
	case EShooterWindowMode::Windowed:
	default:
		return EWindowMode::Windowed;
	}
}
