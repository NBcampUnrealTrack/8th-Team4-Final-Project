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
#include "UI/Settings/ShooterOptionsPerformanceSubsystem.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"

namespace
{
	static constexpr int32 MaxResolutionWidth = 1920;
	static constexpr int32 MaxResolutionHeight = 1080;
	static constexpr float FrameRateLimitChoices[] =
	{
		30.0f, 60.0f, 120.0f, 144.0f, 165.0f, 200.0f, 240.0f
	};

	static constexpr float SmartTargetFPSChoices[] =
	{
		30.0f, 60.0f, 75.0f, 90.0f, 100.0f, 120.0f, 144.0f, 165.0f,
		180.0f, 200.0f, 240.0f
	};

	static int32 FindNearestSmartTargetIndex(const float TargetFPS)
	{
		int32 BestIndex = 0;
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SmartTargetFPSChoices); ++Index)
		{
			const float Distance = FMath::Abs(TargetFPS - SmartTargetFPSChoices[Index]);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestIndex = Index;
			}
		}
		return BestIndex;
	}
}

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
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	// 해상도/창 모드는 선택만 보관하고 적용 버튼에서 한 번만 미리보기한다.
	if (bVideoModeSelectionDirty && !bVideoModeConfirmationPending)
	{
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
		PreviewVideoMode(PendingResolution, PendingWindowMode);
		return;
	}

	// 화면 설정 확인 창이 열린 동안에는 임시 화면 모드를 저장하지 않는다.
	if (!bVideoModeConfirmationPending)
	{
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}
}

void UShooterOptionsScreen::FinishEditing()
{
	RevertVideoModeInternal(false);
	if (UShooterGameUserSettings* Settings = GetGameUserSettings())
	{
		Settings->ApplyNonResolutionSettings();
		Settings->SaveSettings();
	}
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

		Settings->ApplyLanguageCulture();
	}

	// Snapshot the gameplay session before the menu's own frame cost contaminates it.
	bSessionRecommendationApplied = false;
	if (UShooterOptionsPerformanceSubsystem* Performance = GetPerformanceSubsystem())
	{
		const UShooterGameUserSettings* Settings = GetGameUserSettings();
		const float TargetFPS = Settings ? Settings->GetPerformanceTargetFPS() : 120.0f;
		SessionAverageFPS = Performance->GetAverageFPS();
		SessionOnePercentLowFPS = Performance->GetOnePercentLowFPS();
		SessionTargetMissPercent = Performance->GetTargetMissPercent(TargetFPS);
		SessionSampleCount = Performance->GetSampleCount();
		Performance->SetSamplingPaused(true);
	}

	Super::NativeOnActivated();
	GeneratedLabRollingFrameTimeMs = 0.0f;
	ApplyGeneratedLanguage();
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::NativeOnDeactivated()
{
	RevertVideoModeInternal(false);
	ReleaseGeneratedPreviewCapture();
	if (UShooterOptionsPerformanceSubsystem* Performance = GetPerformanceSubsystem())
	{
		Performance->SetSamplingPaused(false);
	}
	Super::NativeOnDeactivated();
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
	if (UShooterOptionsPerformanceSubsystem* Performance = GetPerformanceSubsystem())
	{
		Performance->SetSamplingPaused(false);
	}
	Super::NativeDestruct();
}

void UShooterOptionsScreen::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (FMath::IsFinite(InDeltaTime) && InDeltaTime > 0.0f && InDeltaTime < 0.5f)
	{
		const float FrameMs = InDeltaTime * 1000.0f;
		GeneratedLabRollingFrameTimeMs = GeneratedLabRollingFrameTimeMs <= 0.0f
			? FrameMs
			: FMath::Lerp(GeneratedLabRollingFrameTimeMs, FrameMs, 0.08f);
	}

	if (ActiveGeneratedPage == 0)
	{
		RefreshGeneratedLab();
	}
}

TArray<FIntPoint> UShooterOptionsScreen::GetSupportedResolutions() const
{
	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

	Resolutions.RemoveAll([](const FIntPoint& Resolution)
	{
		return Resolution.X <= 0 || Resolution.Y <= 0
			|| Resolution.X > MaxResolutionWidth
			|| Resolution.Y > MaxResolutionHeight;
	});

	// Keep a deterministic FHD ceiling even on editor/remote displays that do not
	// enumerate every common mode. Lower modes remain selectable when reported.
	Resolutions.AddUnique(FIntPoint(MaxResolutionWidth, MaxResolutionHeight));

	if (const UGameUserSettings* Settings = GetGameUserSettings())
	{
		const FIntPoint CurrentResolution = Settings->GetScreenResolution();
		if (CurrentResolution.X > 0 && CurrentResolution.Y > 0
			&& CurrentResolution.X <= MaxResolutionWidth
			&& CurrentResolution.Y <= MaxResolutionHeight)
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
	if (bVideoModeSelectionDirty || bVideoModeConfirmationPending)
	{
		return PendingResolution;
	}

	const UGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetScreenResolution() : FIntPoint::ZeroValue;
}

void UShooterOptionsScreen::SetScreenResolution(const FIntPoint Resolution)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings) || bVideoModeConfirmationPending
		|| Resolution.X <= 0 || Resolution.Y <= 0)
	{
		return;
	}

	const FIntPoint ClampedResolution(
		FMath::Min(Resolution.X, MaxResolutionWidth),
		FMath::Min(Resolution.Y, MaxResolutionHeight));
	if (!bVideoModeSelectionDirty)
	{
		PendingResolution = Settings->GetScreenResolution();
		PendingWindowMode = FromEngineWindowMode(Settings->GetFullscreenMode());
	}

	PendingResolution = ClampedResolution;
	bVideoModeSelectionDirty = PendingResolution != Settings->GetScreenResolution()
		|| PendingWindowMode != FromEngineWindowMode(Settings->GetFullscreenMode());
	OnSettingsChanged.Broadcast();
}

EShooterWindowMode UShooterOptionsScreen::GetWindowMode() const
{
	if (bVideoModeSelectionDirty || bVideoModeConfirmationPending)
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
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings) || bVideoModeConfirmationPending)
	{
		return;
	}

	if (!bVideoModeSelectionDirty)
	{
		PendingResolution = Settings->GetScreenResolution();
		PendingWindowMode = FromEngineWindowMode(Settings->GetFullscreenMode());
	}

	PendingWindowMode = WindowMode;
	bVideoModeSelectionDirty = PendingResolution != Settings->GetScreenResolution()
		|| PendingWindowMode != FromEngineWindowMode(Settings->GetFullscreenMode());
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::PreviewVideoMode(
	const FIntPoint Resolution,
	const EShooterWindowMode WindowMode)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings) || bVideoModeConfirmationPending
		|| Resolution.X <= 0 || Resolution.Y <= 0)
	{
		return;
	}

	const FIntPoint CurrentResolution = Settings->GetScreenResolution();
	if (CurrentResolution.X <= 0 || CurrentResolution.Y <= 0)
	{
		return;
	}

	VideoModeBaselineResolution = CurrentResolution;
	VideoModeBaselineWindowMode = FromEngineWindowMode(Settings->GetFullscreenMode());

	PendingResolution = FIntPoint(
		FMath::Min(Resolution.X, MaxResolutionWidth),
		FMath::Min(Resolution.Y, MaxResolutionHeight));
	PendingWindowMode = WindowMode;
	bVideoModeSelectionDirty = false;
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
	InvalidatePerformanceSamples();

	ClearVideoModeConfirmationTimeout();
	bVideoModeConfirmationPending = false;
	bVideoModeSelectionDirty = false;
	VideoModeBaselineResolution = FIntPoint::ZeroValue;
	Settings->SaveSettings();
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::RevertVideoMode()
{
	RevertVideoModeInternal(true);
}

void UShooterOptionsScreen::RevertVideoModeInternal(const bool bNotifySettingsChanged)
{
	ClearVideoModeConfirmationTimeout();

	if (!bVideoModeConfirmationPending && !bVideoModeSelectionDirty)
	{
		return;
	}

	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		bVideoModeConfirmationPending = false;
		bVideoModeSelectionDirty = false;
		VideoModeBaselineResolution = FIntPoint::ZeroValue;
		return;
	}

	if (bVideoModeConfirmationPending)
	{
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
	}

	bVideoModeConfirmationPending = false;
	bVideoModeSelectionDirty = false;
	PendingResolution = FIntPoint::ZeroValue;
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
	InvalidatePerformanceSamples();
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
	InvalidatePerformanceSamples();
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

	Settings->SetFrameRateLimit(FMath::Clamp(FramesPerSecond, 30.0f, 240.0f));
	Settings->ApplyNonResolutionSettings();
	InvalidatePerformanceSamples();
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
	InvalidatePerformanceSamples();
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
	InvalidatePerformanceSamples();
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
	InvalidatePerformanceSamples();
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
	InvalidatePerformanceSamples();
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

EShooterOptionsLanguage UShooterOptionsScreen::GetLanguage() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetLanguage() : EShooterOptionsLanguage::Korean;
}

void UShooterOptionsScreen::SetLanguage(const EShooterOptionsLanguage Language)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetLanguage(Language);
	ApplyGeneratedLanguage();
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
	InvalidatePerformanceSamples();
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
	Settings->ApplyLanguageCulture();
	ApplyGeneratedLanguage();
	InvalidatePerformanceSamples();
	OnSettingsChanged.Broadcast();
}

FText UShooterOptionsScreen::FormatResolution(const FIntPoint Resolution)
{
	return FText::FromString(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
}

FText UShooterOptionsScreen::GetWindowModeDisplayText(
	const EShooterWindowMode WindowMode)
{
	const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	const bool bEnglish = Settings && Settings->GetLanguage() == EShooterOptionsLanguage::English;

	switch (WindowMode)
	{
	case EShooterWindowMode::Fullscreen:
	case EShooterWindowMode::WindowedFullscreen:
		return FText::FromString(bEnglish ? TEXT("Fullscreen") : TEXT("전체 화면"));
	case EShooterWindowMode::Windowed:
	default:
		return FText::FromString(bEnglish ? TEXT("Windowed") : TEXT("창 모드"));
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
	BIND_CLICK("Btn_Side_Settings", HandleSideSettings);
	BIND_CLICK("Btn_Side_SmartTuning", HandleSideSmartTuning);
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
	BIND_CLICK("Btn_TargetFPSPrev", HandleTargetFPSPrev);
	BIND_CLICK("Btn_TargetFPSNext", HandleTargetFPSNext);
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
	BIND_CLICK("Btn_LanguagePrev", HandleLanguagePrev);
	BIND_CLICK("Btn_LanguageNext", HandleLanguageNext);
	BIND_CLICK("Btn_LabCaptureBaseline", HandleCaptureBaseline);
	BIND_CLICK("Btn_Profile_Visual60", HandleProfileVisual60);
	BIND_CLICK("Btn_Profile_Balanced120", HandleProfileBalanced120);
	BIND_CLICK("Btn_Profile_Competitive144", HandleProfileCompetitive144);
	BIND_CLICK("Btn_Profile_MaxQuality", HandleProfileMaxQuality);
	BIND_CLICK("Btn_ApplyAdvisorRecommendation", HandleApplyAdvisorRecommendation);
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
	BIND_FOCUS("Btn_Info_Language", HandleInfoLanguage);

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


FText UShooterOptionsScreen::Localize(const TCHAR* Korean, const TCHAR* English) const
{
	return FText::FromString(GetLanguage() == EShooterOptionsLanguage::English ? English : Korean);
}

void UShooterOptionsScreen::ApplyGeneratedLanguage()
{
	const bool bEnglish = GetLanguage() == EShooterOptionsLanguage::English;
	const auto Set = [this, bEnglish](const TCHAR* Name, const TCHAR* Korean, const TCHAR* English)
	{
		SetGeneratedText(FName(Name), FText::FromString(bEnglish ? English : Korean));
	};

	Set(TEXT("TXT_HeaderEyebrow"), TEXT("GAME SETTINGS"), TEXT("GAME SETTINGS"));
	Set(TEXT("TXT_HeaderTitle"), TEXT("게임 옵션"), TEXT("Game Options"));

	Set(TEXT("TXT_SideSection_Display"), TEXT("화면"), TEXT("DISPLAY"));
	Set(TEXT("TXT_SideSection_Gameplay"), TEXT("게임플레이"), TEXT("GAMEPLAY"));
	Set(TEXT("TXT_SideSection_Audio"), TEXT("오디오"), TEXT("AUDIO"));
	Set(TEXT("TXT_SideSection_Options"), TEXT("설정"), TEXT("OPTIONS"));
	Set(TEXT("TXT_SideSection_Smart"), TEXT("스마트 도구"), TEXT("SMART TOOLS"));

	Set(TEXT("TXT_Btn_Side_Graphics"), TEXT("그래픽 설정"), TEXT("Graphics"));
	Set(TEXT("TXT_Btn_Side_Display"), TEXT("화면 설정"), TEXT("Display"));
	Set(TEXT("TXT_Btn_Side_Controls"), TEXT("조작 설정"), TEXT("Controls"));
	Set(TEXT("TXT_Btn_Side_Audio"), TEXT("오디오 설정"), TEXT("Audio"));
	Set(TEXT("TXT_Btn_Side_Settings"), TEXT("옵션 설정"), TEXT("Options"));
	Set(TEXT("TXT_Btn_Side_SmartTuning"), TEXT("스마트 튜닝"), TEXT("Smart Tuning"));

	Set(TEXT("TXT_Section_GraphicsPreset"), TEXT("GRAPHICS PRESET"), TEXT("GRAPHICS PRESET"));
	Set(TEXT("TXT_Section_Quality"), TEXT("QUALITY"), TEXT("QUALITY"));
	Set(TEXT("TXT_Section_Display"), TEXT("DISPLAY"), TEXT("DISPLAY"));
	Set(TEXT("TXT_Section_Look"), TEXT("LOOK"), TEXT("LOOK"));
	Set(TEXT("TXT_Section_Gameplay"), TEXT("GAMEPLAY"), TEXT("GAMEPLAY"));
	Set(TEXT("TXT_Section_Volume"), TEXT("VOLUME"), TEXT("VOLUME"));
	Set(TEXT("TXT_Section_Voice"), TEXT("VOICE"), TEXT("VOICE"));
	Set(TEXT("TXT_Section_Options"), TEXT("OPTIONS"), TEXT("OPTIONS"));
	Set(TEXT("TXT_Section_TargetExperience"), TEXT("TARGET EXPERIENCE"), TEXT("TARGET EXPERIENCE"));
	Set(TEXT("TXT_Section_SessionAdvisor"), TEXT("SESSION ADVISOR"), TEXT("SESSION ADVISOR"));
	Set(TEXT("TXT_Label_TargetFPS"), TEXT("목표 프레임"), TEXT("Target FPS"));
	Set(TEXT("TXT_TargetFPSHelp"), TEXT("목표 프레임은 성능 분석 기준입니다. 화면 설정의 FPS 제한과는 별개입니다."), TEXT("Target FPS is the advisor's analysis goal. It is independent from the display frame-rate limit."));

	Set(TEXT("TXT_Label_Overall"), TEXT("전체 품질"), TEXT("Overall Quality"));
	Set(TEXT("TXT_Label_ViewDistance"), TEXT("시야 거리"), TEXT("View Distance"));
	Set(TEXT("TXT_Label_AntiAliasing"), TEXT("안티앨리어싱"), TEXT("Anti-Aliasing"));
	Set(TEXT("TXT_Label_Shadow"), TEXT("그림자 품질"), TEXT("Shadow Quality"));
	Set(TEXT("TXT_Label_GlobalIllumination"), TEXT("글로벌 일루미네이션"), TEXT("Global Illumination"));
	Set(TEXT("TXT_Label_Reflection"), TEXT("반사 품질"), TEXT("Reflection Quality"));
	Set(TEXT("TXT_Label_PostProcessing"), TEXT("포스트 프로세싱"), TEXT("Post Processing"));
	Set(TEXT("TXT_Label_Texture"), TEXT("텍스처 품질"), TEXT("Texture Quality"));
	Set(TEXT("TXT_Label_Effects"), TEXT("이펙트 품질"), TEXT("Effects Quality"));
	Set(TEXT("TXT_Label_Foliage"), TEXT("폴리지 품질"), TEXT("Foliage Quality"));
	Set(TEXT("TXT_Label_Shading"), TEXT("셰이딩 품질"), TEXT("Shading Quality"));

	Set(TEXT("TXT_Label_Resolution"), TEXT("해상도"), TEXT("Resolution"));
	Set(TEXT("TXT_Label_WindowMode"), TEXT("화면 모드"), TEXT("Window Mode"));
	Set(TEXT("TXT_Label_ResolutionScale"), TEXT("해상도 스케일"), TEXT("Resolution Scale"));
	Set(TEXT("TXT_Label_VSync"), TEXT("수직 동기화"), TEXT("VSync"));
	Set(TEXT("TXT_Label_FrameRate"), TEXT("FPS 제한"), TEXT("Frame Rate Limit"));
	Set(TEXT("TXT_Label_DynamicResolution"), TEXT("다이내믹 레졸루션"), TEXT("Dynamic Resolution"));

	Set(TEXT("TXT_Label_HorizontalSensitivity"), TEXT("수평 감도"), TEXT("Horizontal Sensitivity"));
	Set(TEXT("TXT_Label_VerticalSensitivity"), TEXT("수직 감도"), TEXT("Vertical Sensitivity"));
	Set(TEXT("TXT_Label_AimSensitivity"), TEXT("ADS 감도 배율"), TEXT("ADS Sensitivity Multiplier"));
	Set(TEXT("TXT_Label_InvertY"), TEXT("Y축 반전"), TEXT("Invert Y"));
	Set(TEXT("TXT_Label_DynamicCrosshair"), TEXT("동적 크로스헤어"), TEXT("Dynamic Crosshair"));

	Set(TEXT("TXT_Label_MasterVolume"), TEXT("마스터"), TEXT("Master"));
	Set(TEXT("TXT_Label_MusicVolume"), TEXT("음악"), TEXT("Music"));
	Set(TEXT("TXT_Label_SfxVolume"), TEXT("효과음"), TEXT("SFX"));
	Set(TEXT("TXT_Label_UiVolume"), TEXT("UI"), TEXT("UI"));
	Set(TEXT("TXT_Label_VoiceVolume"), TEXT("음성"), TEXT("Voice"));
	Set(TEXT("TXT_Label_MasterMute"), TEXT("전체 음소거"), TEXT("Mute All"));
	Set(TEXT("TXT_Label_VoiceChat"), TEXT("음성 채팅"), TEXT("Voice Chat"));
	Set(TEXT("TXT_Label_Language"), TEXT("언어"), TEXT("Language"));

	Set(TEXT("TXT_Btn_WindowModeFullscreen"), TEXT("전체 화면"), TEXT("Fullscreen"));
	Set(TEXT("TXT_Btn_WindowModeWindowed"), TEXT("창 모드"), TEXT("Windowed"));

	Set(TEXT("TXT_Btn_Reset"), TEXT("초기화"), TEXT("Reset"));
	Set(TEXT("TXT_Btn_Save"), TEXT("저장"), TEXT("Save"));
	Set(TEXT("TXT_Btn_Back"), TEXT("뒤로"), TEXT("Back"));
	Set(TEXT("TXT_Btn_VideoKeep"), TEXT("유지"), TEXT("Keep"));
	Set(TEXT("TXT_Btn_VideoRevert"), TEXT("취소"), TEXT("Revert"));

	Set(TEXT("TXT_VideoConfirmTitle"), TEXT("변경한 화면 설정을 유지하시겠습니까?"), TEXT("Keep these display settings?"));
	Set(TEXT("TXT_PreviewEyebrow"), TEXT("VISUAL PERFORMANCE LAB"), TEXT("VISUAL PERFORMANCE LAB"));
	Set(TEXT("TXT_LabBaselineLabel"), TEXT("기준 화면"), TEXT("BASELINE"));
	Set(TEXT("TXT_LabCurrentLabel"), TEXT("현재 화면"), TEXT("CURRENT"));
	Set(TEXT("TXT_BaselineStatus"), TEXT("기준 화면을 캡처하세요"), TEXT("Capture a baseline"));
	Set(TEXT("TXT_Btn_LabCaptureBaseline"), TEXT("기준 캡처"), TEXT("Capture Baseline"));
	Set(TEXT("TXT_ExplainGameplayTitle"), TEXT("게임플레이 영향"), TEXT("GAMEPLAY IMPACT"));
	Set(TEXT("TXT_ExplainPerformanceTitle"), TEXT("성능 비용"), TEXT("PERFORMANCE COST"));
	Set(TEXT("TXT_ExplainRecommendationTitle"), TEXT("추천"), TEXT("RECOMMENDATION"));
	Set(TEXT("TXT_AdvisorTargetLabel"), TEXT("목표 프레임"), TEXT("Target FPS"));
	Set(TEXT("TXT_AdvisorAverageLabel"), TEXT("지난 세션 평균"), TEXT("Session Average"));
	Set(TEXT("TXT_AdvisorOneLowLabel"), TEXT("1% Low"), TEXT("1% Low"));
	Set(TEXT("TXT_AdvisorMissLabel"), TEXT("목표 미달 비율"), TEXT("Target Miss"));
	Set(TEXT("TXT_Label_Btn_Profile_Visual60"), TEXT("비주얼 우선 · 화질 중심"), TEXT("Visual Priority · Fidelity First"));
	Set(TEXT("TXT_Label_Btn_Profile_Balanced120"), TEXT("균형 · 화질/성능 균형"), TEXT("Balanced · Quality / Performance"));
	Set(TEXT("TXT_Label_Btn_Profile_Competitive144"), TEXT("경쟁 · 가시성/프레임 우선"), TEXT("Competitive · Clarity / FPS First"));
	Set(TEXT("TXT_Label_Btn_Profile_MaxQuality"), TEXT("최고 화질 · 최대 품질"), TEXT("Max Quality · Maximum Fidelity"));
	Set(TEXT("TXT_Label_Btn_ApplyAdvisorRecommendation"), TEXT("세션 추천"), TEXT("Session Recommendation"));
	Set(TEXT("TXT_Btn_Profile_Visual60"), TEXT("프로필 적용"), TEXT("Apply Profile"));
	Set(TEXT("TXT_Btn_Profile_Balanced120"), TEXT("프로필 적용"), TEXT("Apply Profile"));
	Set(TEXT("TXT_Btn_Profile_Competitive144"), TEXT("프로필 적용"), TEXT("Apply Profile"));
	Set(TEXT("TXT_Btn_Profile_MaxQuality"), TEXT("프로필 적용"), TEXT("Apply Profile"));
	Set(TEXT("TXT_Btn_ApplyAdvisorRecommendation"), TEXT("추천 적용"), TEXT("Apply Recommendation"));
	Set(TEXT("TXT_ContextEyebrow"), TEXT("SETTING INFO"), TEXT("SETTING INFO"));
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
		? Localize(TEXT("켜짐"), TEXT("ON"))
		: Localize(TEXT("꺼짐"), TEXT("OFF")));
}

void UShooterOptionsScreen::RefreshGeneratedUI()
{
	if (!bGeneratedUIBound || bRefreshingGeneratedUI)
	{
		return;
	}

	bRefreshingGeneratedUI = true;
	ApplyGeneratedLanguage();

	const FText ResolutionText = FormatResolution(GetScreenResolution());
	const FText FrameLimitText = GetFrameRateLimitDisplayText(GetFrameRateLimit());
	const FText OverallText = GetQualityDisplayText(GetOverallQualityLevel());

	SetGeneratedText(TEXT("TXT_ResolutionValue"), ResolutionText);
	SetGeneratedText(TEXT("TXT_WindowModeValue"), GetWindowModeDisplayText(GetWindowMode()));
	SetGeneratedText(TEXT("TXT_FrameRateValue"), FrameLimitText);
	SetGeneratedText(TEXT("TXT_OverallValue"), OverallText);
	SetGeneratedText(TEXT("TXT_LanguageValue"), GetLanguage() == EShooterOptionsLanguage::English ? FText::FromString(TEXT("English")) : FText::FromString(TEXT("한국어")));
	SetGeneratedChoiceSegments(TEXT("Language"), GetLanguage() == EShooterOptionsLanguage::English ? 1 : 0, 2);

	// Header is intentionally not navigation; it summarizes the current runtime state.
	SetGeneratedText(TEXT("TXT_HeaderStatusValue_Display"), ResolutionText);
	SetGeneratedText(TEXT("TXT_HeaderStatusValue_Quality"), OverallText);
	SetGeneratedText(TEXT("TXT_HeaderStatusValue_FPS"), FrameLimitText);
	if (const UShooterGameUserSettings* LocalSettings = GetGameUserSettings())
	{
		const float TargetFPS = LocalSettings->GetPerformanceTargetFPS();
		SetGeneratedText(TEXT("TXT_HeaderStatusValue_Target"), FText::FromString(FString::Printf(TEXT("%.0f FPS"), TargetFPS)));
		SetGeneratedText(TEXT("TXT_TargetFPSValue"), FText::FromString(FString::Printf(TEXT("%.0f FPS"), TargetFPS)));
		SetGeneratedChoiceSegments(TEXT("TargetFPS"), FindNearestSmartTargetIndex(TargetFPS), UE_ARRAY_COUNT(SmartTargetFPSChoices));
	}


	// Show how many discrete choices each selector has and where the current value sits.
	{
		const TArray<FIntPoint> Resolutions = GetSupportedResolutions();
		const int32 ResolutionIndex = Resolutions.IndexOfByKey(GetScreenResolution());
		SetGeneratedChoiceSegments(TEXT("Resolution"), ResolutionIndex, Resolutions.Num());

		const float CurrentLimit = GetFrameRateLimit();
		int32 BestIndex = 0;
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameRateLimitChoices); ++Index)
		{
			const float Distance = FMath::Abs(CurrentLimit - FrameRateLimitChoices[Index]);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestIndex = Index;
			}
		}
		SetGeneratedChoiceSegments(TEXT("FrameRate"), BestIndex, UE_ARRAY_COUNT(FrameRateLimitChoices));

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

		const FString ValueText = FString::Printf(TEXT("%.*f"), FractionDigits, Value);
		SetGeneratedText(TextName, FText::FromString(ValueText));
	};

	auto ConfigureSensitivitySlider = [this](const FName SliderName)
	{
		if (USlider* Slider = Cast<USlider>(GetWidgetFromName(SliderName)))
		{
			Slider->SetMinValue(0.0f);
			Slider->SetMaxValue(1.0f);
			Slider->SetStepSize(0.001f);
		}
	};

	ConfigureSensitivitySlider(TEXT("SLD_HorizontalSensitivity"));
	ConfigureSensitivitySlider(TEXT("SLD_VerticalSensitivity"));
	ConfigureSensitivitySlider(TEXT("SLD_AimSensitivity"));
	SetSlider(TEXT("SLD_HorizontalSensitivity"), TEXT("TXT_HorizontalSensitivityValue"), GetMouseHorizontalSensitivity(), 3);
	SetSlider(TEXT("SLD_VerticalSensitivity"), TEXT("TXT_VerticalSensitivityValue"), GetMouseVerticalSensitivity(), 3);
	SetSlider(TEXT("SLD_AimSensitivity"), TEXT("TXT_AimSensitivityValue"), GetAimSensitivityMultiplier(), 3);

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
		FText::Format(Localize(TEXT("{0}초 후 이전 설정으로 복구됩니다."), TEXT("Previous settings will be restored in {0} seconds.")), FText::AsNumber(FMath::RoundToInt(GetVideoModeConfirmationTimeoutSeconds()))));

	if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("Switcher_MainPages"))))
	{
		Switcher->SetActiveWidgetIndex(FMath::Clamp(ActiveGeneratedPage, 0, 5));
	}

	RefreshGeneratedTabVisuals();
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedAdvisor();
	RefreshGeneratedDetailPanel();
	bRefreshingGeneratedUI = false;
}

void UShooterOptionsScreen::ShowGeneratedPage(const int32 PageIndex)
{
	ActiveGeneratedPage = FMath::Clamp(PageIndex, 0, 5);
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
		{TEXT("Btn_Side_Settings"), TEXT("SB_SideAccent_Settings")},
		{TEXT("Btn_Side_SmartTuning"), TEXT("SB_SideAccent_SmartTuning")},
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
		TEXT("Language"),
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
			? Localize(TEXT("예상 성능 영향도"), TEXT("Estimated Performance Impact"))
			: Localize(TEXT("성능 영향도 없음"), TEXT("No Performance Impact")));
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
			PreviewStatus->SetText(Localize(
				TEXT("레벨에 Shooter Options Preview Camera를 배치하거나\nSceneCapture2D의 Component Tag를 OptionsPreview로 지정하세요."),
				TEXT("Place a Shooter Options Preview Camera in the level,\nor tag a SceneCapture2D component with OptionsPreview.")));
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
	else if (CaptureComponent->TextureTarget != GeneratedPreviewRenderTarget)
	{
		// Refreshing our own target must preserve ownership for cleanup when closing.
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
		Help = FText::FromString(TEXT("30~240 FPS 범위에서 제한합니다. 240Hz 환경에서는 최대 240 FPS까지 선택할 수 있습니다."));
		MetricValue0 = Value;
		MetricLabel1 = FText::FromString(TEXT("선택 수"));
		MetricValue1 = FText::FromString(TEXT("7 단계"));
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
			Value = FText::FromString(FString::Printf(TEXT("%.3f"), GetMouseHorizontalSensitivity()));
			MetricLabel1 = FText::FromString(TEXT("범위"));
			MetricValue1 = FText::FromString(TEXT("0.000 ~ 1.000"));
		}
		else if (Option == FName(TEXT("VerticalSensitivity")))
		{
			Title = FText::FromString(TEXT("수직 감도"));
			Body = FText::FromString(TEXT("마우스 Y축 회전 속도를 조절합니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.3f"), GetMouseVerticalSensitivity()));
			MetricLabel1 = FText::FromString(TEXT("범위"));
			MetricValue1 = FText::FromString(TEXT("0.000 ~ 1.000"));
		}
		else if (Option == FName(TEXT("AimSensitivity")))
		{
			Title = FText::FromString(TEXT("ADS 감도 배율"));
			Body = FText::FromString(TEXT("조준 중 기본 마우스 감도에 곱해지는 배율입니다."));
			Value = FText::FromString(FString::Printf(TEXT("%.3f"), GetAimSensitivityMultiplier()));
			MetricLabel1 = FText::FromString(TEXT("범위"));
			MetricValue1 = FText::FromString(TEXT("0.000 ~ 1.000"));
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
			Body = FText::FromString(TEXT("인게임 크로스헤어의 동적 반응을 켜거나 끕니다."));
			Value = FText::FromString(IsDynamicCrosshairEnabled() ? TEXT("켜짐") : TEXT("꺼짐"));
			MetricLabel1 = FText::FromString(TEXT("선택"));
			MetricValue1 = FText::FromString(TEXT("켜짐 / 꺼짐"));
		}
		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		Help = FText::FromString(TEXT("감도·Y축 반전·ADS 배율은 플레이어 입력에 즉시 반영되며, 동적 크로스헤어는 인게임 레티클에 즉시 반영됩니다."));
		MetricValue0 = Value;
		MetricLabel2 = FText::FromString(TEXT("적용"));
		MetricValue2 = FText::FromString(TEXT("즉시"));
	}
	else if (Option == FName(TEXT("Language")))
	{
		Category = FText::FromString(TEXT("OPTIONS"));
		Title = Localize(TEXT("언어"), TEXT("Language"));
		const FText Value = GetLanguage() == EShooterOptionsLanguage::English
			? FText::FromString(TEXT("English"))
			: FText::FromString(TEXT("한국어"));
		Current = FText::Format(Localize(TEXT("현재 {0}"), TEXT("Current: {0}")), Value);
		Body = Localize(
			TEXT("옵션 화면과 게임의 현재 문화권을 한국어 또는 영어로 전환합니다."),
			TEXT("Switches the options interface and the current game culture between Korean and English."));
		Help = Localize(
			TEXT("게임 자체의 다른 텍스트도 번역하려면 프로젝트에 해당 Localization 리소스가 포함되어 있어야 합니다."),
			TEXT("Other game text changes only when the project includes localization resources for the selected culture."));
		MetricLabel0 = Localize(TEXT("현재 값"), TEXT("Current"));
		MetricValue0 = Value;
		MetricLabel1 = Localize(TEXT("선택"), TEXT("Choices"));
		MetricValue1 = FText::FromString(TEXT("한국어 / English"));
		MetricLabel2 = Localize(TEXT("저장"), TEXT("Persistence"));
		MetricValue2 = Localize(TEXT("사용자별 저장"), TEXT("Per-user"));
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
			Body = FText::FromString(TEXT("음성 채팅 송수신 사용 여부를 제어하고, Voice 채널 재생에도 즉시 반영합니다."));
			Value = FText::FromString(IsVoiceChatEnabled() ? TEXT("켜짐") : TEXT("꺼짐"));
			Help = FText::FromString(TEXT("끄면 진행 중인 마이크 캡처와 음성 재생을 중단하고, 다시 켜기 전까지 음성 송수신 시작을 막습니다."));
			MetricLabel1 = FText::FromString(TEXT("OFF 시"));
			MetricValue1 = FText::FromString(TEXT("연결된 음성 채널 음소거"));
			MetricLabel2 = FText::FromString(TEXT("적용"));
			MetricValue2 = FText::FromString(TEXT("즉시"));
		}

		Current = FText::Format(FText::FromString(TEXT("현재 {0}")), Value);
		MetricValue0 = Value;
	}

	if (GetLanguage() == EShooterOptionsLanguage::English && Option != FName(TEXT("Language")))
	{
		MetricLabel0 = FText::FromString(TEXT("Current"));
		MetricLabel1 = FText::FromString(TEXT("Choices"));
		MetricLabel2 = FText::FromString(TEXT("Apply"));
		MetricValue1 = FText::FromString(TEXT("--"));
		MetricValue2 = FText::FromString(TEXT("Immediate"));
		Help = FText::GetEmpty();

		if (Option == FName(TEXT("Resolution")))
		{
			Category = FText::FromString(TEXT("VIDEO")); Title = FText::FromString(TEXT("Resolution"));
			Current = FText::Format(FText::FromString(TEXT("Current: {0}")), FormatResolution(GetScreenResolution()));
			Body = FText::FromString(TEXT("Controls the number of pixels rendered to the display. Higher resolutions improve clarity but increase GPU workload."));
			MetricLabel1 = FText::FromString(TEXT("Supported")); MetricLabel2 = FText::FromString(TEXT("Apply")); MetricValue2 = FText::FromString(TEXT("15 sec confirm"));
			MetricValue1 = FText::AsNumber(GetSupportedResolutions().Num());
		}
		else if (Option == FName(TEXT("WindowMode")))
		{
			Category = FText::FromString(TEXT("VIDEO")); Title = FText::FromString(TEXT("Window Mode"));
			Current = FText::Format(FText::FromString(TEXT("Current: {0}")), GetWindowModeDisplayText(GetWindowMode()));
			Body = FText::FromString(TEXT("Choose between fullscreen and windowed presentation."));
			MetricLabel1 = FText::FromString(TEXT("Choices")); MetricValue1 = FText::FromString(TEXT("Fullscreen / Windowed")); MetricLabel2 = FText::FromString(TEXT("Apply")); MetricValue2 = FText::FromString(TEXT("15 sec confirm"));
		}
		else if (Option == FName(TEXT("ResolutionScale")))
		{
			Category = FText::FromString(TEXT("VIDEO")); Title = FText::FromString(TEXT("Resolution Scale"));
			Current = FText::FromString(FString::Printf(TEXT("Current: %.0f%%"), GetResolutionScalePercent()));
			Body = FText::FromString(TEXT("Adjusts internal render resolution while keeping the output resolution unchanged."));
			MetricLabel1 = FText::FromString(TEXT("Range")); MetricValue1 = FText::FromString(TEXT("50 ~ 100%")); MetricLabel2 = FText::FromString(TEXT("Apply")); MetricValue2 = FText::FromString(TEXT("Immediate"));
			Help = FText::FromString(TEXT("Lower values reduce GPU workload but can reduce image clarity."));
		}
		else if (Option == FName(TEXT("VSync")))
		{
			Category = FText::FromString(TEXT("VIDEO")); Title = FText::FromString(TEXT("VSync"));
			const FText V = IsVSyncEnabled() ? FText::FromString(TEXT("ON")) : FText::FromString(TEXT("OFF"));
			Current = FText::Format(FText::FromString(TEXT("Current: {0}")), V); Body = FText::FromString(TEXT("Synchronizes frame presentation with the display refresh cycle to reduce tearing."));
			MetricValue0 = V; MetricLabel1 = FText::FromString(TEXT("Choices")); MetricValue1 = FText::FromString(TEXT("ON / OFF")); MetricLabel2 = FText::FromString(TEXT("Apply")); MetricValue2 = FText::FromString(TEXT("Immediate"));
			Help = FText::FromString(TEXT("VSync can reduce tearing but may increase input latency."));
		}
		else if (Option == FName(TEXT("FrameRate")))
		{
			Category = FText::FromString(TEXT("VIDEO")); Title = FText::FromString(TEXT("Frame Rate Limit")); const FText V = GetFrameRateLimitDisplayText(GetFrameRateLimit());
			Current = FText::Format(FText::FromString(TEXT("Current: {0}")), V); Body = FText::FromString(TEXT("Caps the maximum frame rate. Unlimited removes the software cap.")); MetricValue0 = V; MetricLabel1 = FText::FromString(TEXT("Choices")); MetricLabel2 = FText::FromString(TEXT("Apply")); MetricValue2 = FText::FromString(TEXT("Immediate"));
			MetricValue1 = FText::FromString(TEXT("8 choices"));
			Help = FText::FromString(TEXT("Choose a limit based on display refresh rate and measured game performance."));
		}
		else if (Option == FName(TEXT("DynamicResolution")))
		{
			Category = FText::FromString(TEXT("VIDEO")); Title = FText::FromString(TEXT("Dynamic Resolution")); const FText V = IsDynamicResolutionEnabled()?FText::FromString(TEXT("ON")):FText::FromString(TEXT("OFF"));
			Current = FText::Format(FText::FromString(TEXT("Current: {0}")), V); Body = FText::FromString(TEXT("Allows supported renderers to adjust resolution dynamically to maintain performance.")); MetricValue0 = V; MetricLabel1 = FText::FromString(TEXT("Choices")); MetricValue1 = FText::FromString(TEXT("ON / OFF")); MetricLabel2 = FText::FromString(TEXT("Apply")); MetricValue2 = FText::FromString(TEXT("Immediate"));
			Help = FText::FromString(TEXT("Requires dynamic-resolution support and configuration in the project renderer."));
		}
		else if (Option == FName(TEXT("Overall")))
		{
			Category=FText::FromString(TEXT("GRAPHICS")); Title=FText::FromString(TEXT("Overall Quality")); const FText V=GetQualityDisplayText(GetOverallQualityLevel()); Current=FText::Format(FText::FromString(TEXT("Current: {0}")),V); Body=FText::FromString(TEXT("Changes the major scalability groups together as a single preset.")); MetricValue0=V;
			MetricValue1 = FText::FromString(TEXT("Low - Cinematic"));
		}
		else if (Option == FName(TEXT("ViewDistance")) || Option == FName(TEXT("AntiAliasing")) || Option == FName(TEXT("Shadow")) || Option == FName(TEXT("GlobalIllumination")) || Option == FName(TEXT("Reflection")) || Option == FName(TEXT("PostProcessing")) || Option == FName(TEXT("Texture")) || Option == FName(TEXT("Effects")) || Option == FName(TEXT("Foliage")) || Option == FName(TEXT("Shading")))
		{
			Category=FText::FromString(TEXT("GRAPHICS"));
			if(Option==FName(TEXT("ViewDistance"))){Title=FText::FromString(TEXT("View Distance")); Body=FText::FromString(TEXT("Controls how far objects and scene details remain visible."));}
			else if(Option==FName(TEXT("AntiAliasing"))){Title=FText::FromString(TEXT("Anti-Aliasing")); Body=FText::FromString(TEXT("Controls edge smoothing and temporal image stability."));}
			else if(Option==FName(TEXT("Shadow"))){Title=FText::FromString(TEXT("Shadow Quality")); Body=FText::FromString(TEXT("Controls shadow resolution, distance, and detail."));}
			else if(Option==FName(TEXT("GlobalIllumination"))){Title=FText::FromString(TEXT("Global Illumination")); Body=FText::FromString(TEXT("Controls indirect lighting quality, including Lumen-based bounce lighting."));}
			else if(Option==FName(TEXT("Reflection"))){Title=FText::FromString(TEXT("Reflection Quality")); Body=FText::FromString(TEXT("Controls the precision and stability of environment reflections."));}
			else if(Option==FName(TEXT("PostProcessing"))){Title=FText::FromString(TEXT("Post Processing")); Body=FText::FromString(TEXT("Controls bloom, ambient occlusion, color grading, and other post effects."));}
			else if(Option==FName(TEXT("Texture"))){Title=FText::FromString(TEXT("Texture Quality")); Body=FText::FromString(TEXT("Controls texture streaming and mip quality, primarily affecting VRAM usage."));}
			else if(Option==FName(TEXT("Effects"))){Title=FText::FromString(TEXT("Effects Quality")); Body=FText::FromString(TEXT("Controls particle and visual-effect quality."));}
			else if(Option==FName(TEXT("Foliage"))){Title=FText::FromString(TEXT("Foliage Quality")); Body=FText::FromString(TEXT("Controls vegetation density and view distance."));}
			else {Title=FText::FromString(TEXT("Shading Quality")); Body=FText::FromString(TEXT("Controls selected material and lighting shading quality."));}
			MetricValue0 = GetQualityDisplayText(GetCurrentQualityForOption(Option));
			MetricValue1 = FText::FromString(TEXT("Low - Cinematic"));
			Help = BuildOptionRecommendationText(Option);
		}
		else if (Option == FName(TEXT("HorizontalSensitivity")) || Option == FName(TEXT("VerticalSensitivity")) || Option == FName(TEXT("AimSensitivity")) || Option == FName(TEXT("InvertY")) || Option == FName(TEXT("DynamicCrosshair")))
		{
			Category=FText::FromString(TEXT("CONTROLS"));
			if(Option==FName(TEXT("HorizontalSensitivity"))){Title=FText::FromString(TEXT("Horizontal Sensitivity")); Body=FText::FromString(TEXT("Adjusts horizontal look sensitivity."));}
			else if(Option==FName(TEXT("VerticalSensitivity"))){Title=FText::FromString(TEXT("Vertical Sensitivity")); Body=FText::FromString(TEXT("Adjusts vertical look sensitivity."));}
			else if(Option==FName(TEXT("AimSensitivity"))){Title=FText::FromString(TEXT("ADS Sensitivity Multiplier")); Body=FText::FromString(TEXT("Multiplier applied to look sensitivity while aiming."));}
			else if(Option==FName(TEXT("InvertY"))){Title=FText::FromString(TEXT("Invert Y")); Body=FText::FromString(TEXT("Reverses vertical look input."));}
			else {Title=FText::FromString(TEXT("Dynamic Crosshair")); Body=FText::FromString(TEXT("Enables or disables dynamic behavior on the in-game crosshair."));}
			Help = FText::FromString(TEXT("Sensitivity, invert-Y and ADS scaling are applied directly to player look input; dynamic crosshair is consumed by the in-game reticle."));
			MetricLabel1 = FText::FromString(TEXT("Range"));
			MetricValue1 = FText::FromString(TEXT("0.000 - 1.000"));
			MetricValue2 = FText::FromString(TEXT("Immediate"));
			if (Option == FName(TEXT("AimSensitivity")))
			{
				MetricValue1 = FText::FromString(TEXT("0.000 - 1.000"));
			}
			else if (Option == FName(TEXT("InvertY")))
			{
				MetricValue0 = FText::FromString(IsInvertVerticalLookEnabled() ? TEXT("ON") : TEXT("OFF"));
				MetricLabel1 = FText::FromString(TEXT("Choices"));
				MetricValue1 = FText::FromString(TEXT("ON / OFF"));
			}
			else if (Option == FName(TEXT("DynamicCrosshair")))
			{
				MetricValue0 = FText::FromString(IsDynamicCrosshairEnabled() ? TEXT("ON") : TEXT("OFF"));
				MetricLabel1 = FText::FromString(TEXT("Choices"));
				MetricValue1 = FText::FromString(TEXT("ON / OFF"));
			}
		}
		else
		{
			Category=FText::FromString(TEXT("AUDIO"));
			if(Option==FName(TEXT("MasterVolume"))){Title=FText::FromString(TEXT("Master Volume")); Body=FText::FromString(TEXT("Adjusts the overall game audio output."));}
			else if(Option==FName(TEXT("MusicVolume"))){Title=FText::FromString(TEXT("Music Volume")); Body=FText::FromString(TEXT("Adjusts sounds assigned to the music channel."));}
			else if(Option==FName(TEXT("SfxVolume"))){Title=FText::FromString(TEXT("SFX Volume")); Body=FText::FromString(TEXT("Adjusts weapon, footsteps, impacts, and other sound effects."));}
			else if(Option==FName(TEXT("UiVolume"))){Title=FText::FromString(TEXT("UI Volume")); Body=FText::FromString(TEXT("Adjusts menu and interface feedback sounds."));}
			else if(Option==FName(TEXT("VoiceVolume"))){Title=FText::FromString(TEXT("Voice Volume")); Body=FText::FromString(TEXT("Adjusts received voice-chat playback volume."));}
			else if(Option==FName(TEXT("MasterMute"))){Title=FText::FromString(TEXT("Mute All")); Body=FText::FromString(TEXT("Mutes all game audio output without discarding stored channel volumes."));}
			else {Title=FText::FromString(TEXT("Voice Chat")); Body=FText::FromString(TEXT("Enables or disables the game's voice-chat capture, transport and playback flow."));}
			MetricLabel1 = FText::FromString(TEXT("Range"));
			MetricValue1 = FText::FromString(TEXT("0 - 100%"));
			Help = FText::FromString(TEXT("Category volumes require the corresponding SoundClass and SoundMix assignments in Project Settings."));
			if (Option == FName(TEXT("MasterVolume")))
			{
				Help = FText::FromString(TEXT("Master volume controls the overall game audio output directly."));
			}
			else if (Option == FName(TEXT("VoiceVolume")))
			{
				Body = FText::FromString(TEXT("Adjusts playback routed through the configured Voice SoundClass."));
				Help = FText::FromString(TEXT("This controls received voice playback volume, not microphone input sensitivity."));
			}
			else if (Option == FName(TEXT("MasterMute")))
			{
				MetricValue0 = FText::FromString(IsMasterAudioMuted() ? TEXT("ON") : TEXT("OFF"));
				MetricLabel1 = FText::FromString(TEXT("Choices"));
				MetricValue1 = FText::FromString(TEXT("ON / OFF"));
				Help = FText::FromString(TEXT("Unmuting restores the stored master and category volume settings."));
			}
			else if (Option == FName(TEXT("VoiceChat")))
			{
				MetricValue0 = FText::FromString(IsVoiceChatEnabled() ? TEXT("ON") : TEXT("OFF"));
				MetricLabel1 = FText::FromString(TEXT("When off"));
				MetricValue1 = FText::FromString(TEXT("Voice channel muted"));
				Help = FText::FromString(TEXT("When disabled, active capture/playback is stopped and new voice-chat transmission is blocked until re-enabled."));
			}
		}
		Current = FText::Format(FText::FromString(TEXT("Current: {0}")), MetricValue0);
	}

	if (Option == FName(TEXT("Advisor")))
	{
		Category = FText::FromString(TEXT("SMART TUNING"));
		Title = Localize(TEXT("세션 성능 어드바이저"), TEXT("Session Performance Advisor"));
		const UShooterGameUserSettings* LocalSettings = GetGameUserSettings();
		const float TargetFPS = LocalSettings ? LocalSettings->GetPerformanceTargetFPS() : 120.0f;
		Current = FText::FromString(FString::Printf(TEXT("Target %.0f FPS"), TargetFPS));
		Body = Localize(
			TEXT("직전 플레이 세션의 평균 FPS, 1% Low, 목표 프레임 미달 비율을 바탕으로 설정 변경을 제안합니다. 자동으로 몰래 변경하지 않고 플레이어가 직접 적용을 승인합니다."),
			TEXT("Uses the previous gameplay session's average FPS, 1% low and target-miss ratio to recommend changes. Nothing is changed silently; the player approves the recommendation."));
		Help = BuildAdvisorRecommendationText();
		MetricLabel0 = Localize(TEXT("평균 FPS"), TEXT("Average FPS"));
		MetricValue0 = SessionAverageFPS > 0.0f ? FText::FromString(FString::Printf(TEXT("%.0f"), SessionAverageFPS)) : FText::FromString(TEXT("--"));
		MetricLabel1 = FText::FromString(TEXT("1% Low"));
		MetricValue1 = SessionOnePercentLowFPS > 0.0f ? FText::FromString(FString::Printf(TEXT("%.0f"), SessionOnePercentLowFPS)) : FText::FromString(TEXT("--"));
		MetricLabel2 = Localize(TEXT("목표 미달"), TEXT("Target Miss"));
		MetricValue2 = SessionSampleCount > 0 ? FText::FromString(FString::Printf(TEXT("%.1f%%"), SessionTargetMissPercent)) : FText::FromString(TEXT("--"));
	}

	const bool bGraphicsPage = ActiveGeneratedPage == 0;
	SetGeneratedText(TEXT("TXT_DetailGameplayImpact"), bGraphicsPage ? BuildGameplayImpactText(Option) : FText::GetEmpty());
	SetGeneratedText(TEXT("TXT_DetailPerformanceCost"), bGraphicsPage ? BuildPerformanceCostText(Option) : FText::GetEmpty());
	SetGeneratedText(TEXT("TXT_DetailRecommendation"), bGraphicsPage ? BuildOptionRecommendationText(Option) : FText::GetEmpty());
	const FName ExplainCards[] = { TEXT("Border_Explain_GameplayImpact"), TEXT("Border_Explain_PerformanceCost"), TEXT("Border_Explain_Recommendation") };
	for (const FName ExplainCardName : ExplainCards)
	{
		if (UWidget* ExplainCard = GetWidgetFromName(ExplainCardName))
		{
			ExplainCard->SetVisibility(bGraphicsPage ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
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

	if (UWidgetSwitcher* DetailVisual = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("Switcher_DetailVisual"))))
	{
		DetailVisual->SetActiveWidgetIndex(bGraphicsPage ? 0 : 1);
	}

	SetGeneratedPerformanceSegments(PerformanceSegments, bGraphicsPage && bPerformanceRelevant);
	RefreshGeneratedPreview();
}



UShooterOptionsPerformanceSubsystem* UShooterOptionsScreen::GetPerformanceSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UShooterOptionsPerformanceSubsystem>();
	}
	return nullptr;
}

void UShooterOptionsScreen::InvalidatePerformanceSamples()
{
	// A session measured with the previous rendering settings cannot justify
	// another recommendation after the player applies a different configuration.
	SessionAverageFPS = 0.0f;
	SessionOnePercentLowFPS = 0.0f;
	SessionTargetMissPercent = 0.0f;
	SessionSampleCount = 0;
	bSessionRecommendationApplied = false;
	if (UShooterOptionsPerformanceSubsystem* Performance = GetPerformanceSubsystem())
	{
		Performance->ResetSamples();
	}
}

int32 UShooterOptionsScreen::GetCurrentQualityForOption(const FName Option) const
{
	if (Option == TEXT("Overall")) return GetOverallQualityLevel();
	if (Option == TEXT("ViewDistance")) return GetQualityLevel(EShooterScalabilitySetting::ViewDistance);
	if (Option == TEXT("AntiAliasing")) return GetQualityLevel(EShooterScalabilitySetting::AntiAliasing);
	if (Option == TEXT("Shadow")) return GetQualityLevel(EShooterScalabilitySetting::Shadow);
	if (Option == TEXT("GlobalIllumination")) return GetQualityLevel(EShooterScalabilitySetting::GlobalIllumination);
	if (Option == TEXT("Reflection")) return GetQualityLevel(EShooterScalabilitySetting::Reflection);
	if (Option == TEXT("PostProcessing")) return GetQualityLevel(EShooterScalabilitySetting::PostProcessing);
	if (Option == TEXT("Texture")) return GetQualityLevel(EShooterScalabilitySetting::Texture);
	if (Option == TEXT("Effects")) return GetQualityLevel(EShooterScalabilitySetting::Effects);
	if (Option == TEXT("Foliage")) return GetQualityLevel(EShooterScalabilitySetting::Foliage);
	if (Option == TEXT("Shading")) return GetQualityLevel(EShooterScalabilitySetting::Shading);
	return INDEX_NONE;
}

void UShooterOptionsScreen::RefreshGeneratedLab()
{
	if (!bGeneratedUIBound)
	{
		return;
	}

	const float CurrentMs = GeneratedLabRollingFrameTimeMs;
	SetGeneratedText(
		TEXT("TXT_LabCurrentFrameTime"),
		CurrentMs > 0.0f
			? FText::FromString(FString::Printf(TEXT("%.2f ms"), CurrentMs))
			: FText::FromString(TEXT("--")));

	SetGeneratedText(
		TEXT("TXT_LabBaselineFrameTime"),
		bGeneratedLabHasBaseline && GeneratedLabBaselineFrameTimeMs > 0.0f
			? FText::FromString(FString::Printf(TEXT("%.2f ms"), GeneratedLabBaselineFrameTimeMs))
			: FText::FromString(TEXT("--")));

	if (bGeneratedLabHasBaseline && CurrentMs > 0.0f && GeneratedLabBaselineFrameTimeMs > 0.0f)
	{
		const float DeltaMs = CurrentMs - GeneratedLabBaselineFrameTimeMs;
		SetGeneratedText(TEXT("TXT_LabDelta"), FText::FromString(FString::Printf(TEXT("%+.2f ms"), DeltaMs)));
	}
	else
	{
		SetGeneratedText(TEXT("TXT_LabDelta"), FText::FromString(TEXT("--")));
	}

	SetGeneratedText(
		TEXT("TXT_LabBaselineValue"),
		bGeneratedLabHasBaseline
			? GeneratedLabBaselineValue
			: Localize(TEXT("기준 미캡처"), TEXT("Not captured")));

	const int32 CurrentQuality = GetCurrentQualityForOption(ActiveGeneratedOption);
	SetGeneratedText(
		TEXT("TXT_LabCurrentValue"),
		CurrentQuality != INDEX_NONE
			? GetQualityDisplayText(CurrentQuality)
			: Localize(TEXT("현재 설정"), TEXT("Current")));
}

void UShooterOptionsScreen::CaptureGeneratedBaseline()
{
	if (ActiveGeneratedPage != 0)
	{
		return;
	}

	RefreshGeneratedPreview();
	USceneCaptureComponent2D* CaptureComponent = GeneratedPreviewCaptureComponent.Get();
	if (!CaptureComponent || !CaptureComponent->TextureTarget)
	{
		return;
	}

	if (!GeneratedBaselineRenderTarget)
	{
		GeneratedBaselineRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("OptionsBaselineRuntimeRT"));
		GeneratedBaselineRenderTarget->ClearColor = FLinearColor::Black;
		GeneratedBaselineRenderTarget->RenderTargetFormat = RTF_RGBA8_SRGB;
		GeneratedBaselineRenderTarget->bForceLinearGamma = false;
		GeneratedBaselineRenderTarget->InitAutoFormat(640, 360);
		GeneratedBaselineRenderTarget->UpdateResourceImmediate(true);
	}

	UTextureRenderTarget2D* CurrentTarget = CaptureComponent->TextureTarget;
	const bool bWasCaptureEveryFrame = CaptureComponent->bCaptureEveryFrame;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->TextureTarget = GeneratedBaselineRenderTarget;
	CaptureComponent->CaptureScene();
	CaptureComponent->TextureTarget = CurrentTarget;
	CaptureComponent->bCaptureEveryFrame = bWasCaptureEveryFrame;
	CaptureComponent->CaptureScene();

	if (UImage* BaselineImage = Cast<UImage>(GetWidgetFromName(TEXT("IMG_BaselinePreview"))))
	{
		FSlateBrush Brush = BaselineImage->GetBrush();
		Brush.SetResourceObject(GeneratedBaselineRenderTarget);
		Brush.ImageSize = FVector2D(640.0f, 360.0f);
		BaselineImage->SetBrush(Brush);
		BaselineImage->SetVisibility(ESlateVisibility::Visible);
	}
	if (UTextBlock* Status = Cast<UTextBlock>(GetWidgetFromName(TEXT("TXT_BaselineStatus"))))
	{
		Status->SetVisibility(ESlateVisibility::Collapsed);
	}

	bGeneratedLabHasBaseline = true;
	GeneratedLabBaselineFrameTimeMs = GeneratedLabRollingFrameTimeMs;
	GeneratedLabBaselineOption = ActiveGeneratedOption;
	const int32 Quality = GetCurrentQualityForOption(ActiveGeneratedOption);
	GeneratedLabBaselineValue = Quality != INDEX_NONE
		? GetQualityDisplayText(Quality)
		: Localize(TEXT("현재 설정"), TEXT("Current"));
	RefreshGeneratedLab();
}

void UShooterOptionsScreen::ApplyExperienceProfile(const EShooterExperienceProfile Profile)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	float ResolutionScale = 100.0f;

	// The profile controls tuning bias only. Target FPS is configured independently so
	// 60/120/144 are not hard-coded assumptions and high-refresh users can choose their own goal.
	switch (Profile)
	{
	case EShooterExperienceProfile::Visual60:
		Settings->SetOverallScalabilityLevel(3);
		break;

	case EShooterExperienceProfile::Balanced120:
		Settings->SetViewDistanceQuality(3);
		Settings->SetAntiAliasingQuality(3);
		Settings->SetShadowQuality(2);
		Settings->SetGlobalIlluminationQuality(2);
		Settings->SetReflectionQuality(2);
		Settings->SetPostProcessingQuality(2);
		Settings->SetTextureQuality(3);
		Settings->SetVisualEffectQuality(2);
		Settings->SetFoliageQuality(2);
		Settings->SetShadingQuality(2);
		break;

	case EShooterExperienceProfile::Competitive144:
		Settings->SetViewDistanceQuality(3);
		Settings->SetAntiAliasingQuality(2);
		Settings->SetShadowQuality(1);
		Settings->SetGlobalIlluminationQuality(1);
		Settings->SetReflectionQuality(1);
		Settings->SetPostProcessingQuality(1);
		Settings->SetTextureQuality(3);
		Settings->SetVisualEffectQuality(2);
		Settings->SetFoliageQuality(1);
		Settings->SetShadingQuality(2);
		Settings->SetVSyncEnabled(false);
		break;

	case EShooterExperienceProfile::MaxQuality:
	default:
		Settings->SetOverallScalabilityLevel(4);
		break;
	}

	float MinScale = 50.0f;
	float MaxScale = 100.0f;
	GetResolutionScaleRange(MinScale, MaxScale);
	Settings->SetResolutionScaleValueEx(FMath::Clamp(ResolutionScale, MinScale, MaxScale));
	Settings->ApplyNonResolutionSettings();
	InvalidatePerformanceSamples();

	Settings->SetExperienceProfile(Profile);
	ActiveGeneratedOption = TEXT("Advisor");
	OnSettingsChanged.Broadcast();
}

FText UShooterOptionsScreen::BuildAdvisorRecommendationText() const
{
	if (bSessionRecommendationApplied)
	{
		return Localize(
			TEXT("추천을 적용했습니다. 옵션을 닫고 잠시 플레이한 뒤 다시 열어 결과를 확인하세요."),
			TEXT("Recommendation applied. Close the options menu, play for a while, then reopen it to review the results."));
	}

	const UShooterGameUserSettings* LocalSettings = GetGameUserSettings();
	const float TargetFPS = LocalSettings ? LocalSettings->GetPerformanceTargetFPS() : 120.0f;
	if (SessionSampleCount < 60)
	{
		return Localize(
			TEXT("플레이 데이터가 아직 부족합니다. 잠시 플레이한 뒤 다시 옵션을 열면 실제 세션 기반 추천을 받을 수 있습니다."),
			TEXT("Not enough gameplay data yet. Play for a while and reopen the options menu for a session-based recommendation."));
	}

	if (SessionTargetMissPercent >= 25.0f)
	{
		return FText::Format(
			Localize(
				TEXT("목표 {0} FPS를 자주 놓쳤습니다. 그림자·GI·반사를 한 단계 낮추는 것을 추천합니다."),
				TEXT("Your session frequently missed the {0} FPS target. Lower Shadow, GI and Reflection by one step.")),
			FText::AsNumber(FMath::RoundToInt(TargetFPS)));
	}
	if (SessionTargetMissPercent >= 8.0f)
	{
		return Localize(
			TEXT("목표 프레임에 간헐적인 흔들림이 있습니다. 반사와 포스트 프로세싱을 한 단계 낮추는 것을 추천합니다."),
			TEXT("The target frame rate was intermittently missed. Lower Reflection and Post Processing by one step."));
	}
	if (SessionAverageFPS > TargetFPS * 1.20f && SessionTargetMissPercent < 3.0f)
	{
		return Localize(
			TEXT("성능 여유가 있습니다. 그림자와 반사 품질을 각각 한 단계 높이는 것을 추천합니다. 적용 후 목표 프레임 유지 여부를 다시 확인하세요."),
			TEXT("There is performance headroom. Raise Shadow and Reflection by one step each, then play again to check whether the target is maintained."));
	}
	return Localize(
		TEXT("현재 설정이 목표 프레임과 잘 맞습니다. 큰 변경보다 현재 구성을 유지하는 것을 추천합니다."),
		TEXT("The current settings match the target well. Keeping this configuration is recommended."));
}

void UShooterOptionsScreen::ApplyAdvisorRecommendation()
{
	if (bSessionRecommendationApplied || SessionSampleCount < 60)
	{
		return;
	}

	UGameUserSettings* Settings = GetGameUserSettings();
	const UShooterGameUserSettings* LocalSettings = GetGameUserSettings();
	if (!Settings || !LocalSettings)
	{
		return;
	}

	const float TargetFPS = LocalSettings->GetPerformanceTargetFPS();
	if (SessionTargetMissPercent >= 25.0f)
	{
		Settings->SetShadowQuality(FMath::Max(0, Settings->GetShadowQuality() - 1));
		Settings->SetGlobalIlluminationQuality(FMath::Max(0, Settings->GetGlobalIlluminationQuality() - 1));
		Settings->SetReflectionQuality(FMath::Max(0, Settings->GetReflectionQuality() - 1));
	}
	else if (SessionTargetMissPercent >= 8.0f)
	{
		Settings->SetReflectionQuality(FMath::Max(0, Settings->GetReflectionQuality() - 1));
		Settings->SetPostProcessingQuality(FMath::Max(0, Settings->GetPostProcessingQuality() - 1));
	}
	else if (SessionAverageFPS > TargetFPS * 1.20f && SessionTargetMissPercent < 3.0f)
	{
		Settings->SetShadowQuality(FMath::Min(4, Settings->GetShadowQuality() + 1));
		Settings->SetReflectionQuality(FMath::Min(4, Settings->GetReflectionQuality() + 1));
	}
	else
	{
		return;
	}

	Settings->ApplyNonResolutionSettings();
	InvalidatePerformanceSamples();
	bSessionRecommendationApplied = true;
	ActiveGeneratedOption = TEXT("Advisor");
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::RefreshGeneratedAdvisor()
{
	const UShooterGameUserSettings* LocalSettings = GetGameUserSettings();
	const float TargetFPS = LocalSettings ? LocalSettings->GetPerformanceTargetFPS() : 120.0f;
	SetGeneratedText(TEXT("TXT_AdvisorTargetValue"), FText::FromString(FString::Printf(TEXT("%.0f FPS"), TargetFPS)));
	SetGeneratedText(TEXT("TXT_AdvisorAverageValue"), SessionAverageFPS > 0.0f ? FText::FromString(FString::Printf(TEXT("%.0f FPS"), SessionAverageFPS)) : FText::FromString(TEXT("--")));
	SetGeneratedText(TEXT("TXT_AdvisorOneLowValue"), SessionOnePercentLowFPS > 0.0f ? FText::FromString(FString::Printf(TEXT("%.0f FPS"), SessionOnePercentLowFPS)) : FText::FromString(TEXT("--")));
	SetGeneratedText(TEXT("TXT_AdvisorMissValue"), SessionSampleCount > 0 ? FText::FromString(FString::Printf(TEXT("%.1f%%"), SessionTargetMissPercent)) : FText::FromString(TEXT("--")));
	SetGeneratedText(TEXT("TXT_AdvisorRecommendation"), BuildAdvisorRecommendationText());
	if (UButton* ApplyButton = Cast<UButton>(GetWidgetFromName(TEXT("Btn_ApplyAdvisorRecommendation"))))
	{
		ApplyButton->SetIsEnabled(SessionSampleCount >= 60 && !bSessionRecommendationApplied);
	}

	// Keep profile rows semantically distinct even when the WBP was regenerated or
	// the language changes at runtime. Only the final row is session-derived.
	SetGeneratedText(TEXT("TXT_Label_Btn_Profile_Visual60"), Localize(TEXT("비주얼 우선 · 화질 중심"), TEXT("Visual Priority · Fidelity First")));
	SetGeneratedText(TEXT("TXT_Label_Btn_Profile_Balanced120"), Localize(TEXT("균형 · 화질/성능 균형"), TEXT("Balanced · Quality / Performance")));
	SetGeneratedText(TEXT("TXT_Label_Btn_Profile_Competitive144"), Localize(TEXT("경쟁 · 가시성/프레임 우선"), TEXT("Competitive · Clarity / FPS First")));
	SetGeneratedText(TEXT("TXT_Label_Btn_Profile_MaxQuality"), Localize(TEXT("최고 화질 · 최대 품질"), TEXT("Max Quality · Maximum Fidelity")));
	SetGeneratedText(TEXT("TXT_Label_Btn_ApplyAdvisorRecommendation"), Localize(TEXT("세션 추천"), TEXT("Session Recommendation")));
	SetGeneratedText(TEXT("TXT_Btn_Profile_Visual60"), Localize(TEXT("프로필 적용"), TEXT("Apply Profile")));
	SetGeneratedText(TEXT("TXT_Btn_Profile_Balanced120"), Localize(TEXT("프로필 적용"), TEXT("Apply Profile")));
	SetGeneratedText(TEXT("TXT_Btn_Profile_Competitive144"), Localize(TEXT("프로필 적용"), TEXT("Apply Profile")));
	SetGeneratedText(TEXT("TXT_Btn_Profile_MaxQuality"), Localize(TEXT("프로필 적용"), TEXT("Apply Profile")));
	SetGeneratedText(TEXT("TXT_Btn_ApplyAdvisorRecommendation"), Localize(TEXT("추천 적용"), TEXT("Apply Recommendation")));

	if (LocalSettings)
	{
		const struct { FName Name; EShooterExperienceProfile Profile; } ProfileButtons[] =
		{
			{ TEXT("Btn_Profile_Visual60"), EShooterExperienceProfile::Visual60 },
			{ TEXT("Btn_Profile_Balanced120"), EShooterExperienceProfile::Balanced120 },
			{ TEXT("Btn_Profile_Competitive144"), EShooterExperienceProfile::Competitive144 },
			{ TEXT("Btn_Profile_MaxQuality"), EShooterExperienceProfile::MaxQuality }
		};
		for (const auto& Entry : ProfileButtons)
		{
			if (UButton* Button = Cast<UButton>(GetWidgetFromName(Entry.Name)))
			{
				Button->SetBackgroundColor(Entry.Profile == LocalSettings->GetExperienceProfile()
					? FLinearColor(0.025f, 0.330f, 0.315f, 0.78f)
					: FLinearColor(0.008f, 0.030f, 0.032f, 0.74f));
			}
		}
	}
}

FText UShooterOptionsScreen::BuildGameplayImpactText(const FName Option) const
{
	if (Option == TEXT("ViewDistance")) return Localize(TEXT("멀리 있는 오브젝트와 환경 정보가 더 빨리 단순화되거나 사라질 수 있습니다. 경쟁 게임에서는 너무 낮은 값이 원거리 정보 인지에 영향을 줄 수 있습니다."), TEXT("Distant objects and scene information simplify or disappear sooner. In competitive games, very low values can reduce long-range readability."));
	if (Option == TEXT("AntiAliasing")) return Localize(TEXT("게임 규칙은 바뀌지 않지만 얇은 구조물과 움직이는 가장자리의 안정성이 달라져 목표물 식별감에 영향을 줄 수 있습니다."), TEXT("Gameplay rules do not change, but thin geometry and moving edges can become less stable, affecting target readability."));
	if (Option == TEXT("Shadow")) return Localize(TEXT("그림자의 선명도와 표시 거리가 달라집니다. 낮은 값은 깊이감과 위치 파악에 쓰이는 시각 단서를 줄일 수 있습니다."), TEXT("Changes shadow clarity and distance. Low values can reduce visual depth cues used for spatial awareness."));
	if (Option == TEXT("GlobalIllumination")) return Localize(TEXT("간접광과 어두운 영역의 대비가 달라져 실내·그늘에서의 가시성 체감에 영향을 줄 수 있습니다."), TEXT("Changes indirect lighting and dark-area contrast, which can affect perceived visibility indoors and in shade."));
	if (Option == TEXT("Reflection")) return Localize(TEXT("대부분 시각적 품질에 영향을 주며 게임 규칙에는 직접 영향이 적습니다."), TEXT("Primarily affects visual fidelity with little direct gameplay impact."));
	if (Option == TEXT("PostProcessing")) return Localize(TEXT("블룸·AO·후처리 밀도가 달라집니다. 낮은 값이 일부 경쟁 환경에서는 화면을 더 또렷하게 느끼게 할 수 있습니다."), TEXT("Changes bloom, AO and post-effect density. Lower values can feel clearer in some competitive scenes."));
	if (Option == TEXT("Texture")) return Localize(TEXT("표면 디테일이 달라지지만 충돌이나 실제 지오메트리는 바뀌지 않습니다."), TEXT("Changes surface detail without changing collision or actual geometry."));
	if (Option == TEXT("Effects")) return Localize(TEXT("파티클과 이펙트 밀도가 달라질 수 있습니다. 프로젝트의 중요한 전투 신호는 어떤 품질에서도 읽히도록 설계해야 합니다."), TEXT("Particle and effect density may change. Important combat cues should remain readable at every quality level."));
	if (Option == TEXT("Foliage")) return Localize(TEXT("식생 밀도와 표시 거리가 달라질 수 있어 맵의 시각적 복잡도와 은폐감에 영향을 줄 수 있습니다."), TEXT("Foliage density and distance can change, affecting visual complexity and perceived cover."));
	if (Option == TEXT("Shading")) return Localize(TEXT("주로 재질과 조명 표현의 정교함에 영향을 줍니다."), TEXT("Primarily affects material and lighting fidelity."));
	if (Option == TEXT("Overall")) return Localize(TEXT("여러 그래픽 품질을 동시에 바꾸므로 가시성·화질·성능이 함께 변합니다."), TEXT("Changes several graphics groups together, so readability, image quality and performance all move at once."));
	return FText::GetEmpty();
}

FText UShooterOptionsScreen::BuildPerformanceCostText(const FName Option) const
{
	if (Option == TEXT("Texture")) return Localize(TEXT("GPU: 낮음 · VRAM: 높음"), TEXT("GPU: Low · VRAM: High"));
	if (Option == TEXT("ViewDistance") || Option == TEXT("Foliage") || Option == TEXT("Effects")) return Localize(TEXT("GPU: 중간 · CPU: 중간"), TEXT("GPU: Medium · CPU: Medium"));
	if (Option == TEXT("Shadow") || Option == TEXT("GlobalIllumination") || Option == TEXT("Reflection")) return Localize(TEXT("GPU: 높음"), TEXT("GPU: High"));
	if (Option == TEXT("AntiAliasing") || Option == TEXT("PostProcessing") || Option == TEXT("Shading")) return Localize(TEXT("GPU: 중간~높음"), TEXT("GPU: Medium–High"));
	if (Option == TEXT("Overall")) return Localize(TEXT("GPU/CPU/VRAM: 종합 영향"), TEXT("GPU/CPU/VRAM: Combined impact"));
	return Localize(TEXT("프로젝트 구성에 따라 다름"), TEXT("Project dependent"));
}

FText UShooterOptionsScreen::BuildOptionRecommendationText(const FName Option) const
{
	if (Option == TEXT("Shadow") || Option == TEXT("GlobalIllumination") || Option == TEXT("Reflection")) return Localize(TEXT("프레임 확보가 우선이면 HIGH보다 먼저 이 항목을 한 단계 낮춰 비교해보세요."), TEXT("If frame rate matters, try lowering this option by one step before reducing overall quality."));
	if (Option == TEXT("Texture")) return Localize(TEXT("VRAM 여유가 있다면 높은 값을 유지하는 편이 화질 대비 성능 효율이 좋습니다."), TEXT("If VRAM is available, keeping this high often offers good image quality for the performance cost."));
	if (Option == TEXT("ViewDistance")) return Localize(TEXT("경쟁 게임에서는 원거리 정보 보존을 위해 MEDIUM 이상을 권장합니다."), TEXT("For competitive games, Medium or higher is recommended to preserve distant scene information."));
	if (Option == TEXT("Overall")) return Localize(TEXT("먼저 목표 프레임 프로필을 선택한 뒤 개별 옵션을 미세 조정하는 방식을 권장합니다."), TEXT("Choose a target-experience profile first, then fine-tune individual settings."));
	return Localize(TEXT("Live Lab에서 기준 화면을 캡처한 뒤 한 단계씩 바꾸며 차이를 확인하세요."), TEXT("Capture a baseline in the Live Lab, then change one step at a time and compare."));
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
	SetScreenResolution(Resolutions[Index]);
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
	SetWindowMode(Modes[Index]);
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::StepFrameRate(const int32 Delta)
{
	const float Current = GetFrameRateLimit();
	int32 BestIndex = 0;
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FrameRateLimitChoices); ++Index)
	{
		const float Distance = FMath::Abs(Current - FrameRateLimitChoices[Index]);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestIndex = Index;
		}
	}
	BestIndex = (BestIndex + Delta + UE_ARRAY_COUNT(FrameRateLimitChoices)) % UE_ARRAY_COUNT(FrameRateLimitChoices);
	SetFrameRateLimit(FrameRateLimitChoices[BestIndex]);
	RefreshGeneratedUI();
}

void UShooterOptionsScreen::StepPerformanceTargetFPS(const int32 Delta)
{
	UShooterGameUserSettings* LocalSettings = GetGameUserSettings();
	if (!LocalSettings)
	{
		return;
	}

	const int32 CurrentIndex = FindNearestSmartTargetIndex(LocalSettings->GetPerformanceTargetFPS());
	const int32 NewIndex = FMath::Clamp(CurrentIndex + Delta, 0, UE_ARRAY_COUNT(SmartTargetFPSChoices) - 1);
	LocalSettings->SetPerformanceTargetFPS(SmartTargetFPSChoices[NewIndex]);

	if (UShooterOptionsPerformanceSubsystem* Performance = GetPerformanceSubsystem())
	{
		SessionTargetMissPercent = Performance->GetTargetMissPercent(LocalSettings->GetPerformanceTargetFPS());
	}

	ActiveGeneratedOption = TEXT("Advisor");
	RefreshGeneratedAdvisor();
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
void UShooterOptionsScreen::HandleSideSettings() { ActiveGeneratedSideButton = TEXT("Btn_Side_Settings"); ActiveGeneratedOption = TEXT("Language"); ShowGeneratedPage(4); }
void UShooterOptionsScreen::HandleSideSmartTuning() { ActiveGeneratedSideButton = TEXT("Btn_Side_SmartTuning"); ActiveGeneratedOption = TEXT("Advisor"); ShowGeneratedPage(5); }
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
	SetWindowMode(EShooterWindowMode::Fullscreen);
	RefreshGeneratedUI();
}
void UShooterOptionsScreen::HandleWindowModeWindowed()
{
	ActiveGeneratedOption = TEXT("WindowMode");
	SetWindowMode(EShooterWindowMode::Windowed);
	RefreshGeneratedUI();
}
void UShooterOptionsScreen::HandleFrameRatePrev() { ActiveGeneratedOption = TEXT("FrameRate"); StepFrameRate(-1); }
void UShooterOptionsScreen::HandleFrameRateNext() { ActiveGeneratedOption = TEXT("FrameRate"); StepFrameRate(1); }
void UShooterOptionsScreen::HandleTargetFPSPrev() { StepPerformanceTargetFPS(-1); }
void UShooterOptionsScreen::HandleTargetFPSNext() { StepPerformanceTargetFPS(1); }
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

void UShooterOptionsScreen::HandleInfoLanguage()
{
	ActiveGeneratedOption = TEXT("Language");
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}


void UShooterOptionsScreen::HandleCaptureBaseline() { CaptureGeneratedBaseline(); }
void UShooterOptionsScreen::HandleProfileVisual60() { ApplyExperienceProfile(EShooterExperienceProfile::Visual60); }
void UShooterOptionsScreen::HandleProfileBalanced120() { ApplyExperienceProfile(EShooterExperienceProfile::Balanced120); }
void UShooterOptionsScreen::HandleProfileCompetitive144() { ApplyExperienceProfile(EShooterExperienceProfile::Competitive144); }
void UShooterOptionsScreen::HandleProfileMaxQuality() { ApplyExperienceProfile(EShooterExperienceProfile::MaxQuality); }
void UShooterOptionsScreen::HandleApplyAdvisorRecommendation() { ApplyAdvisorRecommendation(); }

void UShooterOptionsScreen::HandleLanguagePrev()
{
	ActiveGeneratedOption = TEXT("Language");
	SetLanguage(GetLanguage() == EShooterOptionsLanguage::Korean
		? EShooterOptionsLanguage::English
		: EShooterOptionsLanguage::Korean);
}

void UShooterOptionsScreen::HandleLanguageNext()
{
	HandleLanguagePrev();
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
	SetGeneratedText(TEXT("TXT_HorizontalSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.3f"), Value)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleVerticalSensitivityChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("VerticalSensitivity");
	SetMouseVerticalSensitivity(Value);
	SetGeneratedText(TEXT("TXT_VerticalSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.3f"), Value)));
	RefreshGeneratedOptionRowVisuals();
	RefreshGeneratedDetailPanel();
}

void UShooterOptionsScreen::HandleAimSensitivityChanged(const float Value)
{
	if (bRefreshingGeneratedUI) return;
	ActiveGeneratedOption = TEXT("AimSensitivity");
	SetAimSensitivityMultiplier(Value);
	SetGeneratedText(TEXT("TXT_AimSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.3f"), Value)));
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
		return EShooterWindowMode::Fullscreen;
	case EWindowMode::WindowedFullscreen:
		return EShooterWindowMode::WindowedFullscreen;
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
		return EWindowMode::Fullscreen;
	case EShooterWindowMode::WindowedFullscreen:
		return EWindowMode::WindowedFullscreen;
	case EShooterWindowMode::Windowed:
	default:
		return EWindowMode::Windowed;
	}
}
