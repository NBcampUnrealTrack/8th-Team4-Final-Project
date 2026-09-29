#include "UI/Settings/ShooterOptionsScreen.h"

#include "CommonButtonBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/InputKeySelector.h"
#include "Components/PanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Brushes/SlateColorBrush.h"
#include "Player/ShooterPlayerController.h"
#include "Settings/ShooterGameUserSettings.h"
#include "UI/Settings/ShooterKeyBindingProxy.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"

namespace
{
	enum class EOptionsPage : int32
	{
		Graphics = 0,
		Display,
		Controls,
		KeyBindings,
		Audio,
		VoiceChat,
		Settings,
		Count
	};

	constexpr int32 ToPageIndex(const EOptionsPage Page)
	{
		return static_cast<int32>(Page);
	}

	constexpr float VolumePercentScale = 100.0f;

	// Padding 프로퍼티를 가진 슬롯이면 타입과 무관하게 적용한다.
	bool SetSlotPadding(UPanelSlot* Slot, const FMargin& Padding)
	{
		if (!Slot)
		{
			return false;
		}

		FStructProperty* PaddingProperty =
			CastField<FStructProperty>(Slot->GetClass()->FindPropertyByName(TEXT("Padding")));
		if (!PaddingProperty || PaddingProperty->Struct != TBaseStructure<FMargin>::Get())
		{
			return false;
		}

		*PaddingProperty->ContainerPtrToValuePtr<FMargin>(Slot) = Padding;
		Slot->SynchronizeProperties();
		return true;
	}

}

UShooterOptionsScreen::UShooterOptionsScreen()
{
	InputConfig = EShooterWidgetInputMode::Menu;
}

void UShooterOptionsScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BindOptionControls();
	OnSettingsChanged.AddUniqueDynamic(this, &UShooterOptionsScreen::RefreshOptionsUI);
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

	Super::NativeOnActivated();
	ApplyOptionLanguage();
	RefreshOptionsUI();
}

void UShooterOptionsScreen::NativeOnDeactivated()
{
	RevertVideoModeInternal(false);
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
	Super::NativeDestruct();
}

TArray<FIntPoint> UShooterOptionsScreen::GetSupportedResolutions() const
{
	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

	const FIntPoint MaxResolution = UShooterGameUserSettings::GetMaxSupportedResolution();
	Resolutions.RemoveAll([MaxResolution](const FIntPoint& Resolution)
	{
		return Resolution.X <= 0 || Resolution.Y <= 0
			|| Resolution.X > MaxResolution.X
			|| Resolution.Y > MaxResolution.Y;
	});

	// 현재 디스플레이가 목록에 포함하지 않아도 설정한 최대 해상도는 선택 가능하게 둔다.
	Resolutions.AddUnique(MaxResolution);

	if (const UGameUserSettings* Settings = GetGameUserSettings())
	{
		const FIntPoint CurrentResolution = Settings->GetScreenResolution();
		if (CurrentResolution.X > 0 && CurrentResolution.Y > 0
			&& CurrentResolution.X <= MaxResolution.X
			&& CurrentResolution.Y <= MaxResolution.Y)
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

	const FIntPoint MaxResolution = UShooterGameUserSettings::GetMaxSupportedResolution();
	const FIntPoint ClampedResolution(
		FMath::Min(Resolution.X, MaxResolution.X),
		FMath::Min(Resolution.Y, MaxResolution.Y));
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

	const FIntPoint MaxResolution = UShooterGameUserSettings::GetMaxSupportedResolution();
	PendingResolution = FIntPoint(
		FMath::Min(Resolution.X, MaxResolution.X),
		FMath::Min(Resolution.Y, MaxResolution.Y));
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
	return IsValid(Settings)
		? UShooterGameUserSettings::NormalizeFrameRateLimit(Settings->GetFrameRateLimit())
		: UShooterGameUserSettings::GetDefaultFrameRateLimit();
}

void UShooterOptionsScreen::SetFrameRateLimit(const float FramesPerSecond)
{
	UGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	// Never let this screen write an unsupported FPS value.
	const float SupportedLimit = UShooterGameUserSettings::NormalizeFrameRateLimit(FramesPerSecond);
	Settings->SetFrameRateLimit(SupportedLimit);
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

float UShooterOptionsScreen::GetVoiceChatMicrophoneVolume() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetVoiceChatMicrophoneVolume() : 1.0f;
}

void UShooterOptionsScreen::SetVoiceChatMicrophoneVolume(const float Volume)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetVoiceChatMicrophoneVolume(Volume);
	OnSettingsChanged.Broadcast();
}

float UShooterOptionsScreen::GetVoiceChatOutputVolume() const
{
	const UShooterGameUserSettings* Settings = GetGameUserSettings();
	return IsValid(Settings) ? Settings->GetVoiceChatOutputVolume() : 1.0f;
}

void UShooterOptionsScreen::SetVoiceChatOutputVolume(const float Volume)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	Settings->SetVoiceChatOutputVolume(Volume);
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::HandleKeyBindingSelected(const FName BindingId, const FKey NewKey)
{
	AShooterPlayerController* Controller = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (!IsValid(Controller) || !Controller->RebindInput(BindingId, NewKey))
	{
		return;
	}

	RebuildKeyBindings();
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
	ApplyOptionLanguage();
	OnSettingsChanged.Broadcast();
}

void UShooterOptionsScreen::ResetEngineSettingsToDefaults()
{
	ResetSettingsInternal(false);
}

void UShooterOptionsScreen::ResetAllSettingsToDefaults()
{
	ResetSettingsInternal(true);
}

void UShooterOptionsScreen::ResetSettingsInternal(const bool bResetProjectSettings)
{
	UShooterGameUserSettings* Settings = GetGameUserSettings();
	if (!IsValid(Settings))
	{
		return;
	}

	RevertVideoModeInternal(false);
	const FIntPoint Resolution = Settings->GetScreenResolution();
	const EWindowMode::Type WindowMode = Settings->GetFullscreenMode();

	if (bResetProjectSettings)
	{
		Settings->SetToDefaults();
	}
	else
	{
		// 프로젝트 감도/오디오 값은 유지하고 엔진 영상/그래픽 값만 초기화한다.
		Settings->UGameUserSettings::SetToDefaults();
	}

	Settings->SetScreenResolution(Resolution);
	Settings->SetFullscreenMode(WindowMode);
	Settings->ConfirmVideoMode();

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

	if (bResetProjectSettings)
	{
		Settings->ApplyLanguageCulture();
		ApplyOptionLanguage();
	}

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
	return FText::AsNumber(FMath::RoundToInt(UShooterGameUserSettings::NormalizeFrameRateLimit(FramesPerSecond)));
}

void UShooterOptionsScreen::BindOptionControls()
{
	if (bControlsBound)
	{
		return;
	}

// CommonUI 버튼과 WBP_OptionsScreen의 UButton을 둘 다 지원한다.
#define BIND_CLICK(WidgetName, HandlerName) \
	if (UWidget* FoundWidget = GetWidgetFromName(TEXT(WidgetName))) \
	{ \
		if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(FoundWidget)) \
		{ \
			CommonButton->OnClicked().AddUObject(this, &UShooterOptionsScreen::HandlerName); \
		} \
		else if (UButton* Button = Cast<UButton>(FoundWidget)) \
		{ \
			Button->OnClicked.AddUniqueDynamic(this, &UShooterOptionsScreen::HandlerName); \
		} \
	}

#define BIND_SLIDER(WidgetName, HandlerName) \
	if (USlider* Slider = Cast<USlider>(GetWidgetFromName(TEXT(WidgetName)))) \
	{ \
		Slider->OnValueChanged.AddUniqueDynamic(this, &UShooterOptionsScreen::HandlerName); \
	}

// Editable Text / Editable Text Box 둘 다 받는다
#define BIND_TEXT_COMMIT(WidgetName, HandlerName) \
	if (UWidget* FoundWidget = GetWidgetFromName(TEXT(WidgetName))) \
	{ \
		if (UEditableTextBox* TextBox = Cast<UEditableTextBox>(FoundWidget)) \
		{ \
			TextBox->OnTextCommitted.AddUniqueDynamic(this, &UShooterOptionsScreen::HandlerName); \
		} \
		else if (UEditableText* EditableText = Cast<UEditableText>(FoundWidget)) \
		{ \
			EditableText->OnTextCommitted.AddUniqueDynamic(this, &UShooterOptionsScreen::HandlerName); \
		} \
	}


	BIND_CLICK("Btn_Side_Graphics", HandleSideGraphics);
	BIND_CLICK("Btn_Side_Display", HandleSideDisplay);
	BIND_CLICK("Btn_Side_Controls", HandleSideControls);
	BIND_CLICK("Btn_Side_KeyBindings", HandleSideKeyBindings);
	BIND_CLICK("Btn_Side_Audio", HandleSideAudio);
	BIND_CLICK("Btn_Side_VoiceChat", HandleSideVoiceChat);
	BIND_CLICK("Btn_Side_Settings", HandleSideSettings);
	BIND_CLICK("Btn_Save", HandleSave);
	BIND_CLICK("Btn_Back", HandleBack);
	BIND_CLICK("Btn_Reset", HandleReset);
	BIND_CLICK("Btn_VideoKeep", HandleVideoKeep);
	BIND_CLICK("Btn_VideoRevert", HandleVideoRevert);
	BIND_CLICK("Btn_ResolutionPrev", HandleResolutionPrev);
	BIND_CLICK("Btn_ResolutionNext", HandleResolutionNext);
	BIND_CLICK("Btn_WindowModePrev", HandleWindowModePrev);
	BIND_CLICK("Btn_WindowModeNext", HandleWindowModeNext);
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
	BIND_CLICK("Btn_LanguagePrev", HandleLanguagePrev);
	BIND_CLICK("Btn_LanguageNext", HandleLanguageNext);

	BIND_CLICK("Btn_VSyncToggle", HandleVSyncToggle);
	BIND_CLICK("Btn_DynamicResolutionToggle", HandleDynamicResolutionToggle);
	BIND_CLICK("Btn_InvertYToggle", HandleInvertYToggle);
	BIND_CLICK("Btn_DynamicCrosshairToggle", HandleDynamicCrosshairToggle);
	BIND_CLICK("Btn_MasterMuteToggle", HandleMasterMuteToggle);
	BIND_CLICK("Btn_VoiceChatToggle", HandleVoiceChatToggle);
	BIND_CLICK("Btn_ResetKeyBindings", HandleResetKeyBindings);

	BIND_SLIDER("SLD_ResolutionScale", HandleResolutionScaleChanged);
	BIND_SLIDER("SLD_HorizontalSensitivity", HandleHorizontalSensitivityChanged);
	BIND_SLIDER("SLD_VerticalSensitivity", HandleVerticalSensitivityChanged);
	BIND_SLIDER("SLD_AimSensitivity", HandleAimSensitivityChanged);
	BIND_SLIDER("SLD_MasterVolume", HandleMasterVolumeChanged);
	BIND_SLIDER("SLD_MusicVolume", HandleMusicVolumeChanged);
	BIND_SLIDER("SLD_SfxVolume", HandleSfxVolumeChanged);
	BIND_SLIDER("SLD_UiVolume", HandleUiVolumeChanged);
	BIND_SLIDER("SLD_VoiceVolume", HandleVoiceVolumeChanged);
	BIND_SLIDER("SLD_VoiceChatMicrophoneVolume", HandleVoiceChatMicrophoneVolumeChanged);
	BIND_SLIDER("SLD_VoiceChatOutputVolume", HandleVoiceChatOutputVolumeChanged);

	BIND_TEXT_COMMIT("TXT_HorizontalSensitivityValue", HandleHorizontalSensitivityCommitted);
	BIND_TEXT_COMMIT("TXT_VerticalSensitivityValue", HandleVerticalSensitivityCommitted);
	BIND_TEXT_COMMIT("TXT_AimSensitivityValue", HandleAimSensitivityCommitted);
	BIND_TEXT_COMMIT("TXT_MasterVolumeValue", HandleMasterVolumeCommitted);
	BIND_TEXT_COMMIT("TXT_MusicVolumeValue", HandleMusicVolumeCommitted);
	BIND_TEXT_COMMIT("TXT_SfxVolumeValue", HandleSfxVolumeCommitted);
	BIND_TEXT_COMMIT("TXT_UiVolumeValue", HandleUiVolumeCommitted);
	BIND_TEXT_COMMIT("TXT_VoiceVolumeValue", HandleVoiceVolumeCommitted);
	BIND_TEXT_COMMIT("TXT_VoiceChatMicrophoneVolumeValue", HandleVoiceChatMicrophoneVolumeCommitted);
	BIND_TEXT_COMMIT("TXT_VoiceChatOutputVolumeValue", HandleVoiceChatOutputVolumeCommitted);

#undef BIND_TEXT_COMMIT
#undef BIND_SLIDER
#undef BIND_CLICK

	ApplyOptionStaticStyles();
	bControlsBound = true;
}

void UShooterOptionsScreen::ApplyOptionStaticStyles()
{
	// 상태에 따라 바뀌지 않는 위젯이라 최초 1회만 적용한다.
	static const TCHAR* StepperPrefixes[] =
	{
		TEXT("Resolution"), TEXT("WindowMode"), TEXT("FrameRate"),
		TEXT("Overall"), TEXT("ViewDistance"), TEXT("AntiAliasing"), TEXT("Shadow"),
		TEXT("GlobalIllumination"), TEXT("Reflection"), TEXT("PostProcessing"), TEXT("Texture"),
		TEXT("Effects"), TEXT("Foliage"), TEXT("Shading"), TEXT("Language"),
	};

	if (bOverrideValueBoxStyle)
	{
		for (const TCHAR* Prefix : StepperPrefixes)
		{
			const FName ValueBoxName(*FString::Printf(TEXT("Border_Control_%s"), Prefix));
			if (UBorder* ValueBox = Cast<UBorder>(GetWidgetFromName(ValueBoxName)))
			{
				if (ValueBoxTexture)
				{
					ValueBox->SetBrushFromTexture(ValueBoxTexture);
				}
				// 색은 브러시에 곱해진다. 텍스처 원본 그대로 쓰려면 흰색으로 둔다.
				ValueBox->SetBrushColor(ValueBoxColor);
			}
		}
	}

	if (bOverrideStepperArrowStyle)
	{
		const struct { const TCHAR* Suffix; const FButtonStyle* Style; } ArrowSides[] =
		{
			{ TEXT("Prev"), &StepperArrowPrevStyle },
			{ TEXT("Next"), &StepperArrowNextStyle },
		};
		for (const TCHAR* Prefix : StepperPrefixes)
		{
			for (const auto& Side : ArrowSides)
			{
				const FName ArrowName(*FString::Printf(TEXT("Btn_%s%s"), Prefix, Side.Suffix));
				if (UButton* Arrow = Cast<UButton>(GetWidgetFromName(ArrowName)))
				{
					Arrow->SetStyle(*Side.Style);
					Arrow->SetBackgroundColor(StepperArrowBackgroundColor);
				}
				// 화살표 글자는 버튼 바깥 형제라 따로 칠한다.
				SetButtonTextColor(ArrowName, StepperArrowTextColor);
			}
		}
	}

	if (bOverrideStepperArrowPadding)
	{
		static const TCHAR* ArrowSuffixes[] = { TEXT("Prev"), TEXT("Next") };
		for (const TCHAR* Prefix : StepperPrefixes)
		{
			for (const TCHAR* Suffix : ArrowSuffixes)
			{
				const FName ArrowName(*FString::Printf(TEXT("Btn_%s%s"), Prefix, Suffix));
				UButton* Arrow = Cast<UButton>(GetWidgetFromName(ArrowName));
				if (!Arrow)
				{
					continue;
				}

				// 버튼을 감싼 슬롯(SizeBox 등)과 버튼 안쪽 슬롯을 각각 적용한다.
				SetSlotPadding(Arrow->Slot, StepperArrowOuterPadding);
				SetSlotPadding(Arrow->GetContentSlot(), StepperArrowContentPadding);
			}
		}
	}

	if (bOverrideSliderColors)
	{
		static const TCHAR* SliderNames[] =
		{
			TEXT("SLD_ResolutionScale"), TEXT("SLD_HorizontalSensitivity"), TEXT("SLD_VerticalSensitivity"),
			TEXT("SLD_AimSensitivity"), TEXT("SLD_MasterVolume"), TEXT("SLD_MusicVolume"),
			TEXT("SLD_SfxVolume"), TEXT("SLD_UiVolume"), TEXT("SLD_VoiceVolume"),
			TEXT("SLD_VoiceChatMicrophoneVolume"), TEXT("SLD_VoiceChatOutputVolume"),
		};
		for (const TCHAR* SliderName : SliderNames)
		{
			if (USlider* Slider = Cast<USlider>(GetWidgetFromName(FName(SliderName))))
			{
				Slider->SetSliderBarColor(SliderBarColor);
				Slider->SetSliderHandleColor(SliderHandleColor);
			}
		}
	}
}

void UShooterOptionsScreen::ApplyOptionButtonStateStyle(UButton* Button, const bool bActive) const
{
	if (!Button)
	{
		return;
	}

	if (bOverrideToggleStyle)
	{
		Button->SetStyle(bActive ? ToggleOnStyle : ToggleOffStyle);
		return;
	}

	Button->SetBackgroundColor(bActive
		? FLinearColor(0.025f, 0.330f, 0.315f, 0.78f)
		: FLinearColor(0.008f, 0.030f, 0.032f, 0.74f));
}

void UShooterOptionsScreen::SetOptionText(const FName WidgetName, const FText& Text) const
{
	UWidget* Found = GetWidgetFromName(WidgetName);
	if (UTextBlock* TextBlock = Cast<UTextBlock>(Found))
	{
		TextBlock->SetText(Text);
	}
	// 값 칸을 Editable Text / Editable Text Box로 바꾼 행도 같은 이름으로 갱신된다.
	else if (UEditableTextBox* TextBox = Cast<UEditableTextBox>(Found))
	{
		TextBox->SetText(Text);
	}
	else if (UEditableText* EditableText = Cast<UEditableText>(Found))
	{
		EditableText->SetText(Text);
	}
}


FText UShooterOptionsScreen::Localize(const TCHAR* Korean, const TCHAR* English) const
{
	return FText::FromString(GetLanguage() == EShooterOptionsLanguage::English ? English : Korean);
}

void UShooterOptionsScreen::ApplyOptionLanguage()
{
	const bool bEnglish = GetLanguage() == EShooterOptionsLanguage::English;
	const auto Set = [this, bEnglish](const TCHAR* Name, const TCHAR* Korean, const TCHAR* English)
	{
		SetOptionText(FName(Name), FText::FromString(bEnglish ? English : Korean));
	};

	Set(TEXT("TXT_HeaderEyebrow"), TEXT("GAME SETTINGS"), TEXT("GAME SETTINGS"));
	// 헤더는 영문 전용 폰트라 언어와 무관하게 영어로 둔다.
	Set(TEXT("TXT_HeaderTitle"), TEXT("GAME OPTIONS"), TEXT("GAME OPTIONS"));

	Set(TEXT("TXT_SideSection_Display"), TEXT("화면"), TEXT("DISPLAY"));
	Set(TEXT("TXT_SideSection_Gameplay"), TEXT("게임플레이"), TEXT("GAMEPLAY"));
	Set(TEXT("TXT_SideSection_Audio"), TEXT("오디오"), TEXT("AUDIO"));
	Set(TEXT("TXT_SideSection_Options"), TEXT("설정"), TEXT("OPTIONS"));

	Set(TEXT("TXT_Btn_Side_Graphics"), TEXT("그래픽 설정"), TEXT("Graphics"));
	Set(TEXT("TXT_Btn_Side_Display"), TEXT("화면 설정"), TEXT("Display"));
	Set(TEXT("TXT_Btn_Side_Controls"), TEXT("조작 설정"), TEXT("Controls"));
	Set(TEXT("TXT_Btn_Side_KeyBindings"), TEXT("키 설정"), TEXT("Key Bindings"));
	Set(TEXT("TXT_Btn_Side_Audio"), TEXT("오디오 설정"), TEXT("Audio"));
	Set(TEXT("TXT_Btn_Side_VoiceChat"), TEXT("보이스 채팅"), TEXT("Voice Chat"));
	Set(TEXT("TXT_Btn_Side_Settings"), TEXT("옵션 설정"), TEXT("Options"));

	Set(TEXT("TXT_Section_GraphicsPreset"), TEXT("GRAPHICS PRESET"), TEXT("GRAPHICS PRESET"));
	Set(TEXT("TXT_Section_Quality"), TEXT("QUALITY"), TEXT("QUALITY"));
	Set(TEXT("TXT_Section_Display"), TEXT("DISPLAY"), TEXT("DISPLAY"));
	Set(TEXT("TXT_Section_Look"), TEXT("LOOK"), TEXT("LOOK"));
	Set(TEXT("TXT_Section_Gameplay"), TEXT("GAMEPLAY"), TEXT("GAMEPLAY"));
	Set(TEXT("TXT_Section_KeyBindings"), TEXT("KEY BINDINGS"), TEXT("KEY BINDINGS"));
	Set(TEXT("TXT_Section_Volume"), TEXT("VOLUME"), TEXT("VOLUME"));
	Set(TEXT("TXT_Section_Voice"), TEXT("GAME VOICE"), TEXT("GAME VOICE"));
	Set(TEXT("TXT_Section_VoiceChat"), TEXT("VOICE CHAT"), TEXT("VOICE CHAT"));
	Set(TEXT("TXT_Section_Options"), TEXT("OPTIONS"), TEXT("OPTIONS"));

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
	Set(TEXT("TXT_Label_VoiceVolume"), TEXT("게임 음성 / 아나운서"), TEXT("Game Voice / Announcer"));
	Set(TEXT("TXT_Label_MasterMute"), TEXT("전체 음소거"), TEXT("Mute All"));
	Set(TEXT("TXT_Label_VoiceChat"), TEXT("보이스 채팅 사용"), TEXT("Enable Voice Chat"));
	Set(TEXT("TXT_Label_VoiceChatMicrophoneVolume"), TEXT("마이크 볼륨"), TEXT("Microphone Volume"));
	Set(TEXT("TXT_Label_VoiceChatOutputVolume"), TEXT("상대 음성 볼륨"), TEXT("Incoming Voice Volume"));
	Set(TEXT("TXT_KeyBindingHelp"), TEXT("바꿀 키를 클릭한 뒤 새 키를 입력하세요. 중복 키는 자동으로 서로 교환됩니다."), TEXT("Select a binding and press a new key. Conflicting keys are swapped automatically."));
	Set(TEXT("TXT_KeyBindingsEmpty"), TEXT("변경 가능한 키가 없습니다."), TEXT("No rebindable keys were found."));
	Set(TEXT("TXT_Btn_ResetKeyBindings"), TEXT("키 설정 초기화"), TEXT("Reset Key Bindings"));
	Set(TEXT("TXT_Label_Language"), TEXT("언어"), TEXT("Language"));


	Set(TEXT("TXT_Btn_Reset"), TEXT("초기화"), TEXT("Reset"));
	Set(TEXT("TXT_Btn_Save"), TEXT("저장"), TEXT("Save"));
	Set(TEXT("TXT_Btn_Back"), TEXT("뒤로"), TEXT("Back"));
	Set(TEXT("TXT_Btn_VideoKeep"), TEXT("유지"), TEXT("Keep"));
	Set(TEXT("TXT_Btn_VideoRevert"), TEXT("취소"), TEXT("Revert"));

	Set(TEXT("TXT_VideoConfirmTitle"), TEXT("변경한 화면 설정을 유지하시겠습니까?"), TEXT("Keep these display settings?"));
}

FText UShooterOptionsScreen::BuildInputBindingDisplayName(const FShooterInputBindingEntry& Entry) const
{
	FString Action = Entry.ActionName.ToString();
	Action.RemoveFromStart(TEXT("IA_"));
	Action.ReplaceInline(TEXT("_"), TEXT(" "));
	const FString Lower = Action.ToLower();

	if (Lower.Contains(TEXT("move")))
	{
		if (Entry.DefaultKey == EKeys::W || Entry.DefaultKey == EKeys::Up)
		{
			return Localize(TEXT("앞으로 이동"), TEXT("Move Forward"));
		}
		if (Entry.DefaultKey == EKeys::S || Entry.DefaultKey == EKeys::Down)
		{
			return Localize(TEXT("뒤로 이동"), TEXT("Move Back"));
		}
		if (Entry.DefaultKey == EKeys::A || Entry.DefaultKey == EKeys::Left)
		{
			return Localize(TEXT("왼쪽 이동"), TEXT("Move Left"));
		}
		if (Entry.DefaultKey == EKeys::D || Entry.DefaultKey == EKeys::Right)
		{
			return Localize(TEXT("오른쪽 이동"), TEXT("Move Right"));
		}
	}

	struct FLabelRule
	{
		const TCHAR* Token;
		const TCHAR* Korean;
		const TCHAR* English;
	};
	static const FLabelRule Rules[] =
	{
		{ TEXT("jump"), TEXT("점프"), TEXT("Jump") },
		{ TEXT("sprint"), TEXT("달리기"), TEXT("Sprint") },
		{ TEXT("crouch"), TEXT("앉기"), TEXT("Crouch") },
		{ TEXT("reload"), TEXT("재장전"), TEXT("Reload") },
		{ TEXT("interact"), TEXT("상호작용"), TEXT("Interact") },
		{ TEXT("ping"), TEXT("핑"), TEXT("Ping") },
		{ TEXT("chat"), TEXT("채팅"), TEXT("Chat") },
		{ TEXT("pushtotalk"), TEXT("보이스 채팅 말하기"), TEXT("Push To Talk") },
		{ TEXT("voice"), TEXT("보이스 채팅 말하기"), TEXT("Push To Talk") },
		{ TEXT("scoreboard"), TEXT("스코어보드"), TEXT("Scoreboard") },
		{ TEXT("menu"), TEXT("게임 메뉴"), TEXT("Game Menu") },
		{ TEXT("shop"), TEXT("상점"), TEXT("Shop") },
		{ TEXT("augment"), TEXT("증강 선택"), TEXT("Augment") },
		{ TEXT("primaryattack"), TEXT("발사"), TEXT("Fire") },
		{ TEXT("fire"), TEXT("발사"), TEXT("Fire") },
		{ TEXT("secondary"), TEXT("정조준 / 보조 행동"), TEXT("Aim / Secondary") },
		{ TEXT("aim"), TEXT("정조준"), TEXT("Aim") },
		{ TEXT("weapon1"), TEXT("주무기"), TEXT("Primary Weapon") },
		{ TEXT("primaryweapon"), TEXT("주무기"), TEXT("Primary Weapon") },
		{ TEXT("weapon2"), TEXT("보조무기"), TEXT("Secondary Weapon") },
		{ TEXT("secondaryweapon"), TEXT("보조무기"), TEXT("Secondary Weapon") },
		{ TEXT("melee"), TEXT("근접무기"), TEXT("Melee") },
	};

	FString Compact = Lower;
	Compact.ReplaceInline(TEXT(" "), TEXT(""));
	for (const FLabelRule& Rule : Rules)
	{
		if (Compact.Contains(Rule.Token))
		{
			return Localize(Rule.Korean, Rule.English);
		}
	}

	return FText::FromString(Action.IsEmpty() ? Entry.ActionName.ToString() : Action);
}

void UShooterOptionsScreen::RebuildKeyBindings()
{
	UVerticalBox* List = Cast<UVerticalBox>(GetWidgetFromName(TEXT("VB_KeyBindings")));
	AShooterPlayerController* Controller = Cast<AShooterPlayerController>(GetOwningPlayer());
	if (!List || !IsValid(Controller) || !WidgetTree)
	{
		return;
	}

	List->ClearChildren();
	KeyBindingProxies.Reset();

	TArray<FShooterInputBindingEntry> Bindings;
	Controller->GetRebindableInputBindings(Bindings);
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		const FShooterInputBindingEntry& Entry = Bindings[Index];
		const FName RowName(*FString::Printf(TEXT("Runtime_KeyRow_%d"), Index));
		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), RowName);
		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			FName(*FString::Printf(TEXT("Runtime_KeyRowBox_%d"), Index)));
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("Runtime_KeyLabelBox_%d"), Index)));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("Runtime_KeyLabel_%d"), Index)));
		USizeBox* SelectorBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("Runtime_KeySelectorBox_%d"), Index)));
		UInputKeySelector* Selector = WidgetTree->ConstructWidget<UInputKeySelector>(
			UInputKeySelector::StaticClass(),
			FName(*FString::Printf(TEXT("Runtime_KeySelector_%d"), Index)));
		if (!Row || !RowBox || !LabelBox || !Label || !SelectorBox || !Selector)
		{
			continue;
		}

		Row->SetBrushColor(FLinearColor(0.050f, 0.082f, 0.084f, 0.34f));
		Row->SetPadding(FMargin(14.0f, 7.0f));
		LabelBox->SetWidthOverride(500.0f);
		SelectorBox->SetWidthOverride(260.0f);

		FSlateFontInfo LabelFont = Label->GetFont();
		LabelFont.Size = 18;
		Label->SetFont(LabelFont);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.92f, 0.93f, 1.0f)));
		Label->SetText(BuildInputBindingDisplayName(Entry));

		Selector->SetAllowGamepadKeys(false);
		Selector->SetAllowModifierKeys(false);
		Selector->SetKeySelectionText(Localize(TEXT("키 입력..."), TEXT("Press a key...")));
		Selector->SetNoKeySpecifiedText(Localize(TEXT("미지정"), TEXT("Unbound")));
		Selector->SetSelectedKey(FInputChord(Entry.CurrentKey));
		Selector->SetMargin(FMargin(12.0f, 6.0f));

		FButtonStyle KeyButtonStyle;
		KeyButtonStyle.SetNormal(FSlateColorBrush(FLinearColor(0.008f, 0.030f, 0.032f, 0.86f)));
		KeyButtonStyle.SetHovered(FSlateColorBrush(FLinearColor(0.020f, 0.115f, 0.112f, 0.95f)));
		KeyButtonStyle.SetPressed(FSlateColorBrush(FLinearColor(0.024f, 0.260f, 0.252f, 0.95f)));
		Selector->SetButtonStyle(KeyButtonStyle);

		FTextBlockStyle KeyTextStyle;
		KeyTextStyle.SetFont(LabelFont);
		KeyTextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.96f, 0.97f, 1.0f)));
		Selector->SetTextStyle(KeyTextStyle);

		UShooterKeyBindingProxy* Proxy = NewObject<UShooterKeyBindingProxy>(this);
		Proxy->Initialize(this, Entry.BindingId);
		Selector->OnKeySelected.AddUniqueDynamic(Proxy, &UShooterKeyBindingProxy::HandleKeySelected);
		KeyBindingProxies.Add(Proxy);

		LabelBox->AddChild(Label);
		SelectorBox->AddChild(Selector);
		RowBox->AddChildToHorizontalBox(LabelBox);
		RowBox->AddChildToHorizontalBox(SelectorBox);
		Row->SetContent(RowBox);
		List->AddChildToVerticalBox(Row);
	}

	if (UTextBlock* EmptyText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TXT_KeyBindingsEmpty"))))
	{
		EmptyText->SetVisibility(Bindings.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShooterOptionsScreen::SetButtonTextColor(
	const FName ButtonName,
	const FLinearColor& Color) const
{
	const FName TextName(*FString::Printf(TEXT("TXT_%s"), *ButtonName.ToString()));
	if (UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(TextName)))
	{
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UShooterOptionsScreen::SetChoiceSegments(
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
			Segment->SetBrushColor(Index == ActiveIndex ? SegmentActiveColor : SegmentInactiveColor);
		}
	}
}


void UShooterOptionsScreen::SetToggleVisual(
	const FName ButtonName,
	const FName TextName,
	const bool bEnabled) const
{
	ApplyOptionButtonStateStyle(Cast<UButton>(GetWidgetFromName(ButtonName)), bEnabled);

	SetOptionText(TextName, bEnabled
		? Localize(TEXT("켜짐"), TEXT("ON"))
		: Localize(TEXT("꺼짐"), TEXT("OFF")));
}

void UShooterOptionsScreen::RefreshOptionsUI()
{
	if (!bControlsBound || bRefreshingUI)
	{
		return;
	}

	bRefreshingUI = true;
	ApplyOptionLanguage();

	const FText ResolutionText = FormatResolution(GetScreenResolution());
	const FText FrameLimitText = GetFrameRateLimitDisplayText(GetFrameRateLimit());
	const FText OverallText = GetQualityDisplayText(GetOverallQualityLevel());

	SetOptionText(TEXT("TXT_ResolutionValue"), ResolutionText);
	SetOptionText(TEXT("TXT_WindowModeValue"), GetWindowModeDisplayText(GetWindowMode()));
	SetOptionText(TEXT("TXT_FrameRateValue"), FrameLimitText);
	SetOptionText(TEXT("TXT_OverallValue"), OverallText);
	SetOptionText(TEXT("TXT_LanguageValue"), GetLanguage() == EShooterOptionsLanguage::English ? FText::FromString(TEXT("English")) : FText::FromString(TEXT("한국어")));
	SetChoiceSegments(TEXT("Language"), GetLanguage() == EShooterOptionsLanguage::English ? 1 : 0, 2);

	// Header is intentionally not navigation; it summarizes the current runtime state.
	SetOptionText(TEXT("TXT_HeaderStatusValue_Display"), ResolutionText);
	SetOptionText(TEXT("TXT_HeaderStatusValue_Quality"), OverallText);
	SetOptionText(TEXT("TXT_HeaderStatusValue_FPS"), FrameLimitText);


	// Show how many discrete choices each selector has and where the current value sits.
	{
		const TArray<FIntPoint> Resolutions = GetSupportedResolutions();
		const int32 ResolutionIndex = Resolutions.IndexOfByKey(GetScreenResolution());
		SetChoiceSegments(TEXT("Resolution"), ResolutionIndex, Resolutions.Num());
		SetChoiceSegments(
			TEXT("WindowMode"),
			GetWindowMode() == EShooterWindowMode::Windowed ? 1 : 0,
			2);

		const int32 FrameRateIndex = UShooterGameUserSettings::FindFrameRateLimitChoiceIndex(GetFrameRateLimit());
		SetChoiceSegments(
			TEXT("FrameRate"),
			FrameRateIndex == INDEX_NONE ? 0 : FrameRateIndex,
			UShooterGameUserSettings::GetFrameRateLimitChoiceCount());

		const int32 Overall = GetOverallQualityLevel();
		SetChoiceSegments(TEXT("Overall"), Overall >= 0 && Overall <= 4 ? Overall : INDEX_NONE, 5);
	}

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
		SetOptionText(Entry.Value, GetQualityDisplayText(GetQualityLevel(Entry.Key)));
	}

	SetChoiceSegments(TEXT("ViewDistance"), GetQualityLevel(EShooterScalabilitySetting::ViewDistance), 5);
	SetChoiceSegments(TEXT("AntiAliasing"), GetQualityLevel(EShooterScalabilitySetting::AntiAliasing), 5);
	SetChoiceSegments(TEXT("Shadow"), GetQualityLevel(EShooterScalabilitySetting::Shadow), 5);
	SetChoiceSegments(TEXT("GlobalIllumination"), GetQualityLevel(EShooterScalabilitySetting::GlobalIllumination), 5);
	SetChoiceSegments(TEXT("Reflection"), GetQualityLevel(EShooterScalabilitySetting::Reflection), 5);
	SetChoiceSegments(TEXT("PostProcessing"), GetQualityLevel(EShooterScalabilitySetting::PostProcessing), 5);
	SetChoiceSegments(TEXT("Texture"), GetQualityLevel(EShooterScalabilitySetting::Texture), 5);
	SetChoiceSegments(TEXT("Effects"), GetQualityLevel(EShooterScalabilitySetting::Effects), 5);
	SetChoiceSegments(TEXT("Foliage"), GetQualityLevel(EShooterScalabilitySetting::Foliage), 5);
	SetChoiceSegments(TEXT("Shading"), GetQualityLevel(EShooterScalabilitySetting::Shading), 5);

	float ResolutionMin = 50.0f;
	float ResolutionMax = 100.0f;
	GetResolutionScaleRange(ResolutionMin, ResolutionMax);
	if (USlider* Slider = Cast<USlider>(GetWidgetFromName(TEXT("SLD_ResolutionScale"))))
	{
		Slider->SetMinValue(ResolutionMin);
		Slider->SetMaxValue(ResolutionMax);
		Slider->SetValue(GetResolutionScalePercent());
	}
	SetOptionText(TEXT("TXT_ResolutionScaleValue"), FText::FromString(FString::Printf(TEXT("%.0f%%"), GetResolutionScalePercent())));

	SetToggleVisual(TEXT("Btn_VSyncToggle"), TEXT("TXT_VSyncValue"), IsVSyncEnabled());
	SetToggleVisual(TEXT("Btn_DynamicResolutionToggle"), TEXT("TXT_DynamicResolutionValue"), IsDynamicResolutionEnabled());
	SetToggleVisual(TEXT("Btn_InvertYToggle"), TEXT("TXT_InvertYValue"), IsInvertVerticalLookEnabled());
	SetToggleVisual(TEXT("Btn_DynamicCrosshairToggle"), TEXT("TXT_DynamicCrosshairValue"), IsDynamicCrosshairEnabled());
	SetToggleVisual(TEXT("Btn_MasterMuteToggle"), TEXT("TXT_MasterMuteValue"), IsMasterAudioMuted());
	SetToggleVisual(TEXT("Btn_VoiceChatToggle"), TEXT("TXT_VoiceChatValue"), IsVoiceChatEnabled());

	const AShooterPlayerController* OwningShooterController = Cast<AShooterPlayerController>(GetOwningPlayer());
	const bool bVoiceChatSupported = !OwningShooterController || OwningShooterController->SupportsVoiceChat();
	if (UWidget* VoiceChatTabRow = GetWidgetFromName(TEXT("HB_Side_VoiceChat")))
	{
		VoiceChatTabRow->SetVisibility(bVoiceChatSupported ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (UWidget* VoiceChatPage = GetWidgetFromName(TEXT("Page_VoiceChat")))
	{
		VoiceChatPage->SetVisibility(bVoiceChatSupported ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!bVoiceChatSupported && ActivePage == ToPageIndex(EOptionsPage::VoiceChat))
	{
		ActivePage = ToPageIndex(EOptionsPage::Audio);
		ActiveSideButton = TEXT("Btn_Side_Audio");
		ActiveOption = TEXT("MasterVolume");
	}

	auto SetSlider = [this](const FName SliderName, const FName TextName, const float Value, const int32 FractionDigits)
	{
		if (USlider* Slider = Cast<USlider>(GetWidgetFromName(SliderName)))
		{
			Slider->SetValue(Value);
		}

		const FString ValueText = FString::Printf(TEXT("%.*f"), FractionDigits, Value);
		SetOptionText(TextName, FText::FromString(ValueText));
	};

	auto ConfigureSensitivitySlider = [this](const FName SliderName)
	{
		if (USlider* Slider = Cast<USlider>(GetWidgetFromName(SliderName)))
		{
			Slider->SetMinValue(UShooterGameUserSettings::GetMinMouseSensitivity());
			Slider->SetMaxValue(UShooterGameUserSettings::GetMaxMouseSensitivity());
			Slider->SetStepSize(0.001f);
		}
	};

	ConfigureSensitivitySlider(TEXT("SLD_HorizontalSensitivity"));
	ConfigureSensitivitySlider(TEXT("SLD_VerticalSensitivity"));
	ConfigureSensitivitySlider(TEXT("SLD_AimSensitivity"));
	SetSlider(TEXT("SLD_HorizontalSensitivity"), TEXT("TXT_HorizontalSensitivityValue"), GetMouseHorizontalSensitivity(), 3);
	SetSlider(TEXT("SLD_VerticalSensitivity"), TEXT("TXT_VerticalSensitivityValue"), GetMouseVerticalSensitivity(), 3);
	SetSlider(TEXT("SLD_AimSensitivity"), TEXT("TXT_AimSensitivityValue"), GetAimSensitivityMultiplier(), 3);

	const auto SetVolumeSlider = [this](const FName SliderName, const FName TextName, const float Value)
	{
		if (USlider* Slider = Cast<USlider>(GetWidgetFromName(SliderName)))
		{
			Slider->SetMinValue(UShooterGameUserSettings::GetMinAudioVolume());
			Slider->SetMaxValue(UShooterGameUserSettings::GetMaxAudioVolume());
			Slider->SetStepSize(0.01f);
			Slider->SetValue(Value);
		}

		SetOptionText(TextName, FText::FromString(FString::Printf(TEXT("%.0f"), Value * VolumePercentScale)));
	};

	SetVolumeSlider(TEXT("SLD_MasterVolume"), TEXT("TXT_MasterVolumeValue"), GetAudioVolume(EShooterAudioChannel::Master));
	SetVolumeSlider(TEXT("SLD_MusicVolume"), TEXT("TXT_MusicVolumeValue"), GetAudioVolume(EShooterAudioChannel::Music));
	SetVolumeSlider(TEXT("SLD_SfxVolume"), TEXT("TXT_SfxVolumeValue"), GetAudioVolume(EShooterAudioChannel::SoundEffects));
	SetVolumeSlider(TEXT("SLD_UiVolume"), TEXT("TXT_UiVolumeValue"), GetAudioVolume(EShooterAudioChannel::UserInterface));
	SetVolumeSlider(TEXT("SLD_VoiceVolume"), TEXT("TXT_VoiceVolumeValue"), GetAudioVolume(EShooterAudioChannel::Voice));
	SetVolumeSlider(TEXT("SLD_VoiceChatMicrophoneVolume"), TEXT("TXT_VoiceChatMicrophoneVolumeValue"), GetVoiceChatMicrophoneVolume());
	SetVolumeSlider(TEXT("SLD_VoiceChatOutputVolume"), TEXT("TXT_VoiceChatOutputVolumeValue"), GetVoiceChatOutputVolume());

	if (ActivePage == ToPageIndex(EOptionsPage::KeyBindings))
	{
		RebuildKeyBindings();
	}

	if (UWidget* Popup = GetWidgetFromName(TEXT("Overlay_VideoConfirm")))
	{
		Popup->SetVisibility(IsVideoModeConfirmationPending() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetOptionText(
		TEXT("TXT_VideoConfirmCountdown"),
		FText::Format(Localize(TEXT("{0}초 후 이전 설정으로 복구됩니다."), TEXT("Previous settings will be restored in {0} seconds.")), FText::AsNumber(FMath::RoundToInt(GetVideoModeConfirmationTimeoutSeconds()))));

	if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("Switcher_MainPages"))))
	{
		Switcher->SetActiveWidgetIndex(FMath::Clamp(ActivePage, 0, ToPageIndex(EOptionsPage::Count) - 1));
	}

	RefreshTabVisuals();
	RefreshOptionRowVisuals();
	bRefreshingUI = false;
}

void UShooterOptionsScreen::ShowOptionsPage(const int32 PageIndex)
{
	ActivePage = FMath::Clamp(PageIndex, 0, ToPageIndex(EOptionsPage::Count) - 1);
	RefreshOptionsUI();
}

void UShooterOptionsScreen::RefreshTabVisuals()
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
		{TEXT("Btn_Side_KeyBindings"), TEXT("SB_SideAccent_KeyBindings")},
		{TEXT("Btn_Side_Audio"), TEXT("SB_SideAccent_Audio")},
		{TEXT("Btn_Side_VoiceChat"), TEXT("SB_SideAccent_VoiceChat")},
		{TEXT("Btn_Side_Settings"), TEXT("SB_SideAccent_Settings")},
	};

	// 모든 사이드 탭이 CommonButton일 때만 그룹이 선택을 관리한다. 섞여 있으면 기존 색 경로를 쓴다.
	if (!SideTabGroup)
	{
		TArray<UWidget*> CommonTabs;
		for (const auto& SideItem : SideItems)
		{
			if (UCommonButtonBase* Tab = Cast<UCommonButtonBase>(GetWidgetFromName(SideItem.Button)))
			{
				CommonTabs.Add(Tab);
			}
		}
		if (CommonTabs.Num() == static_cast<int32>(UE_ARRAY_COUNT(SideItems)))
		{
			SideTabGroup = NewObject<UCommonButtonGroupBase>(this);
			SideTabGroup->SetSelectionRequired(true);
			SideTabGroup->AddWidgets(CommonTabs);
		}
	}

	if (SideTabGroup)
	{
		SideTabGroup->SelectButtonAtIndex(ActivePage, false);
	}

	for (const auto& SideItem : SideItems)
	{
		const bool bSelected = SideItem.Button == ActiveSideButton;
		if (!SideTabGroup)
		{
			if (UButton* Button = Cast<UButton>(GetWidgetFromName(SideItem.Button)))
			{
				Button->SetBackgroundColor(bSelected
					? FLinearColor(0.024f, 0.260f, 0.252f, 0.62f)
					: FLinearColor(0.008f, 0.028f, 0.030f, 0.34f));
			}
		}
		if (USizeBox* Accent = Cast<USizeBox>(GetWidgetFromName(SideItem.Accent)))
		{
			Accent->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		SetButtonTextColor(
			SideItem.Button,
			bSelected ? FLinearColor::White : FLinearColor(0.72f, 0.77f, 0.79f, 1.0f));
	}
}

void UShooterOptionsScreen::RefreshOptionRowVisuals()
{
	const FName Prefixes[] =
	{
		TEXT("Resolution"), TEXT("WindowMode"), TEXT("ResolutionScale"), TEXT("VSync"), TEXT("FrameRate"), TEXT("DynamicResolution"),
		TEXT("Overall"), TEXT("ViewDistance"), TEXT("AntiAliasing"), TEXT("Shadow"), TEXT("GlobalIllumination"),
		TEXT("Reflection"), TEXT("PostProcessing"), TEXT("Texture"), TEXT("Effects"), TEXT("Foliage"), TEXT("Shading"),
		TEXT("HorizontalSensitivity"), TEXT("VerticalSensitivity"), TEXT("AimSensitivity"), TEXT("InvertY"), TEXT("DynamicCrosshair"),
		TEXT("MasterVolume"), TEXT("MusicVolume"), TEXT("SfxVolume"), TEXT("UiVolume"), TEXT("VoiceVolume"), TEXT("MasterMute"),
		TEXT("VoiceChat"), TEXT("VoiceChatMicrophoneVolume"), TEXT("VoiceChatOutputVolume"),
		TEXT("Language"),
	};

	// SetBrushFromTexture는 리소스만 교체해 DrawAs/Margin 등 브러시 설정을 보존한다.
	const bool bUseRowTextures = RowNormalTexture && RowActiveTexture;

	for (const FName Prefix : Prefixes)
	{
		const FName RowName(*FString::Printf(TEXT("Border_Row_%s"), *Prefix.ToString()));
		if (UBorder* Row = Cast<UBorder>(GetWidgetFromName(RowName)))
		{
			const bool bActive = Prefix == ActiveOption;
			if (bUseRowTextures)
			{
				// 텍스처 원본 색을 그대로 보여주기 위해 틴트를 흰색으로 되돌린다.
				Row->SetBrushColor(FLinearColor::White);
				Row->SetBrushFromTexture(bActive ? RowActiveTexture : RowNormalTexture);
			}
			else
			{
				Row->SetBrushColor(bActive
					? FLinearColor(0.030f, 0.175f, 0.170f, 0.46f)
					: FLinearColor(0.050f, 0.082f, 0.084f, 0.34f));
			}
		}
	}
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
	RefreshOptionsUI();
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
	RefreshOptionsUI();
}

void UShooterOptionsScreen::StepFrameRate(const int32 Delta)
{
	int32 CurrentIndex = UShooterGameUserSettings::FindFrameRateLimitChoiceIndex(GetFrameRateLimit());
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}

	const int32 ChoiceCount = UShooterGameUserSettings::GetFrameRateLimitChoiceCount();
	const int32 Direction = Delta < 0 ? -1 : 1;
	const int32 NextIndex = (CurrentIndex + Direction + ChoiceCount) % ChoiceCount;

	SetFrameRateLimit(UShooterGameUserSettings::GetFrameRateLimitChoice(NextIndex));
	RefreshOptionsUI();
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
	RefreshOptionsUI();
}

void UShooterOptionsScreen::StepQuality(const EShooterScalabilitySetting Setting, const int32 Delta)
{
	int32 Level = FMath::Clamp(GetQualityLevel(Setting), 0, 4);
	Level = (Level + Delta + 5) % 5;
	SetQualityLevel(Setting, Level);
	RefreshOptionsUI();
}

void UShooterOptionsScreen::HandleSideGraphics() { ActiveSideButton = TEXT("Btn_Side_Graphics"); ActiveOption = TEXT("Overall"); ShowOptionsPage(ToPageIndex(EOptionsPage::Graphics)); }
void UShooterOptionsScreen::HandleSideDisplay() { ActiveSideButton = TEXT("Btn_Side_Display"); ActiveOption = TEXT("Resolution"); ShowOptionsPage(ToPageIndex(EOptionsPage::Display)); }
void UShooterOptionsScreen::HandleSideControls() { ActiveSideButton = TEXT("Btn_Side_Controls"); ActiveOption = TEXT("HorizontalSensitivity"); ShowOptionsPage(ToPageIndex(EOptionsPage::Controls)); }
void UShooterOptionsScreen::HandleSideKeyBindings() { ActiveSideButton = TEXT("Btn_Side_KeyBindings"); ActiveOption = TEXT("KeyBindings"); ShowOptionsPage(ToPageIndex(EOptionsPage::KeyBindings)); }
void UShooterOptionsScreen::HandleSideAudio() { ActiveSideButton = TEXT("Btn_Side_Audio"); ActiveOption = TEXT("MasterVolume"); ShowOptionsPage(ToPageIndex(EOptionsPage::Audio)); }
void UShooterOptionsScreen::HandleSideVoiceChat() { ActiveSideButton = TEXT("Btn_Side_VoiceChat"); ActiveOption = TEXT("VoiceChat"); ShowOptionsPage(ToPageIndex(EOptionsPage::VoiceChat)); }
void UShooterOptionsScreen::HandleSideSettings() { ActiveSideButton = TEXT("Btn_Side_Settings"); ActiveOption = TEXT("Language"); ShowOptionsPage(ToPageIndex(EOptionsPage::Settings)); }
void UShooterOptionsScreen::HandleSave() { SaveOptions(); }
void UShooterOptionsScreen::HandleBack() { RequestClose(); }
void UShooterOptionsScreen::HandleReset() { ResetAllSettingsToDefaults(); RefreshOptionsUI(); }
void UShooterOptionsScreen::HandleVideoKeep() { ConfirmVideoMode(); RefreshOptionsUI(); }
void UShooterOptionsScreen::HandleVideoRevert() { RevertVideoMode(); RefreshOptionsUI(); }

void UShooterOptionsScreen::HandleResolutionPrev() { ActiveOption = TEXT("Resolution"); StepResolution(-1); }
void UShooterOptionsScreen::HandleResolutionNext() { ActiveOption = TEXT("Resolution"); StepResolution(1); }
void UShooterOptionsScreen::HandleWindowModePrev() { ActiveOption = TEXT("WindowMode"); StepWindowMode(-1); }
void UShooterOptionsScreen::HandleWindowModeNext() { ActiveOption = TEXT("WindowMode"); StepWindowMode(1); }
void UShooterOptionsScreen::HandleFrameRatePrev() { ActiveOption = TEXT("FrameRate"); StepFrameRate(-1); }
void UShooterOptionsScreen::HandleFrameRateNext() { ActiveOption = TEXT("FrameRate"); StepFrameRate(1); }
void UShooterOptionsScreen::HandleOverallPrev() { ActiveOption = TEXT("Overall"); StepOverallQuality(-1); }
void UShooterOptionsScreen::HandleOverallNext() { ActiveOption = TEXT("Overall"); StepOverallQuality(1); }
void UShooterOptionsScreen::HandleViewDistancePrev() { ActiveOption = TEXT("ViewDistance"); StepQuality(EShooterScalabilitySetting::ViewDistance, -1); }
void UShooterOptionsScreen::HandleViewDistanceNext() { ActiveOption = TEXT("ViewDistance"); StepQuality(EShooterScalabilitySetting::ViewDistance, 1); }
void UShooterOptionsScreen::HandleAntiAliasingPrev() { ActiveOption = TEXT("AntiAliasing"); StepQuality(EShooterScalabilitySetting::AntiAliasing, -1); }
void UShooterOptionsScreen::HandleAntiAliasingNext() { ActiveOption = TEXT("AntiAliasing"); StepQuality(EShooterScalabilitySetting::AntiAliasing, 1); }
void UShooterOptionsScreen::HandleShadowPrev() { ActiveOption = TEXT("Shadow"); StepQuality(EShooterScalabilitySetting::Shadow, -1); }
void UShooterOptionsScreen::HandleShadowNext() { ActiveOption = TEXT("Shadow"); StepQuality(EShooterScalabilitySetting::Shadow, 1); }
void UShooterOptionsScreen::HandleGlobalIlluminationPrev() { ActiveOption = TEXT("GlobalIllumination"); StepQuality(EShooterScalabilitySetting::GlobalIllumination, -1); }
void UShooterOptionsScreen::HandleGlobalIlluminationNext() { ActiveOption = TEXT("GlobalIllumination"); StepQuality(EShooterScalabilitySetting::GlobalIllumination, 1); }
void UShooterOptionsScreen::HandleReflectionPrev() { ActiveOption = TEXT("Reflection"); StepQuality(EShooterScalabilitySetting::Reflection, -1); }
void UShooterOptionsScreen::HandleReflectionNext() { ActiveOption = TEXT("Reflection"); StepQuality(EShooterScalabilitySetting::Reflection, 1); }
void UShooterOptionsScreen::HandlePostProcessingPrev() { ActiveOption = TEXT("PostProcessing"); StepQuality(EShooterScalabilitySetting::PostProcessing, -1); }
void UShooterOptionsScreen::HandlePostProcessingNext() { ActiveOption = TEXT("PostProcessing"); StepQuality(EShooterScalabilitySetting::PostProcessing, 1); }
void UShooterOptionsScreen::HandleTexturePrev() { ActiveOption = TEXT("Texture"); StepQuality(EShooterScalabilitySetting::Texture, -1); }
void UShooterOptionsScreen::HandleTextureNext() { ActiveOption = TEXT("Texture"); StepQuality(EShooterScalabilitySetting::Texture, 1); }
void UShooterOptionsScreen::HandleEffectsPrev() { ActiveOption = TEXT("Effects"); StepQuality(EShooterScalabilitySetting::Effects, -1); }
void UShooterOptionsScreen::HandleEffectsNext() { ActiveOption = TEXT("Effects"); StepQuality(EShooterScalabilitySetting::Effects, 1); }
void UShooterOptionsScreen::HandleFoliagePrev() { ActiveOption = TEXT("Foliage"); StepQuality(EShooterScalabilitySetting::Foliage, -1); }
void UShooterOptionsScreen::HandleFoliageNext() { ActiveOption = TEXT("Foliage"); StepQuality(EShooterScalabilitySetting::Foliage, 1); }
void UShooterOptionsScreen::HandleShadingPrev() { ActiveOption = TEXT("Shading"); StepQuality(EShooterScalabilitySetting::Shading, -1); }
void UShooterOptionsScreen::HandleShadingNext() { ActiveOption = TEXT("Shading"); StepQuality(EShooterScalabilitySetting::Shading, 1); }




void UShooterOptionsScreen::HandleLanguagePrev()
{
	ActiveOption = TEXT("Language");
	SetLanguage(GetLanguage() == EShooterOptionsLanguage::Korean
		? EShooterOptionsLanguage::English
		: EShooterOptionsLanguage::Korean);
}

void UShooterOptionsScreen::HandleLanguageNext()
{
	HandleLanguagePrev();
}

void UShooterOptionsScreen::HandleVSyncToggle() { ActiveOption = TEXT("VSync"); SetVSyncEnabled(!IsVSyncEnabled()); RefreshOptionsUI(); }
void UShooterOptionsScreen::HandleDynamicResolutionToggle() { ActiveOption = TEXT("DynamicResolution"); SetDynamicResolutionEnabled(!IsDynamicResolutionEnabled()); RefreshOptionsUI(); }
void UShooterOptionsScreen::HandleInvertYToggle() { ActiveOption = TEXT("InvertY"); SetInvertVerticalLookEnabled(!IsInvertVerticalLookEnabled()); RefreshOptionsUI(); }
void UShooterOptionsScreen::HandleDynamicCrosshairToggle() { ActiveOption = TEXT("DynamicCrosshair"); SetDynamicCrosshairEnabled(!IsDynamicCrosshairEnabled()); RefreshOptionsUI(); }
void UShooterOptionsScreen::HandleMasterMuteToggle() { ActiveOption = TEXT("MasterMute"); SetMasterAudioMuted(!IsMasterAudioMuted()); RefreshOptionsUI(); }
void UShooterOptionsScreen::HandleVoiceChatToggle() { ActiveOption = TEXT("VoiceChat"); SetVoiceChatEnabled(!IsVoiceChatEnabled()); RefreshOptionsUI(); }
void UShooterOptionsScreen::HandleResetKeyBindings()
{
	if (AShooterPlayerController* Controller = Cast<AShooterPlayerController>(GetOwningPlayer()))
	{
		Controller->ResetInputBindings();
		RebuildKeyBindings();
	}
}

bool UShooterOptionsScreen::TryParseNumber(const FText& Text, float& OutValue) const
{
	FString ValueString = Text.ToString();
	ValueString.TrimStartAndEndInline();
	if (ValueString.IsEmpty() || !ValueString.IsNumeric())
	{
		return false;
	}

	OutValue = FCString::Atof(*ValueString);
	return FMath::IsFinite(OutValue);
}

void UShooterOptionsScreen::CommitAudioPercent(
	const FText& Text,
	const ETextCommit::Type CommitMethod,
	const EShooterAudioChannel Channel,
	const FName OptionName,
	const FName TextName)
{
	if (CommitMethod != ETextCommit::OnEnter && CommitMethod != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	float Percent = 0.0f;
	if (!TryParseNumber(Text, Percent))
	{
		RefreshOptionsUI();
		return;
	}

	ActiveOption = OptionName;
	SetAudioVolume(Channel, Percent / VolumePercentScale);
	SetOptionText(
		TextName,
		FText::FromString(FString::Printf(TEXT("%.0f"), GetAudioVolume(Channel) * VolumePercentScale)));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::CommitVoiceChatPercent(
	const FText& Text,
	const ETextCommit::Type CommitMethod,
	const bool bMicrophone,
	const FName OptionName,
	const FName TextName)
{
	if (CommitMethod != ETextCommit::OnEnter && CommitMethod != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	float Percent = 0.0f;
	if (!TryParseNumber(Text, Percent))
	{
		RefreshOptionsUI();
		return;
	}

	ActiveOption = OptionName;
	const float Scalar = Percent / VolumePercentScale;
	if (bMicrophone)
	{
		SetVoiceChatMicrophoneVolume(Scalar);
	}
	else
	{
		SetVoiceChatOutputVolume(Scalar);
	}

	const float AppliedValue = bMicrophone
		? GetVoiceChatMicrophoneVolume()
		: GetVoiceChatOutputVolume();
	SetOptionText(
		TextName,
		FText::FromString(FString::Printf(TEXT("%.0f"), AppliedValue * VolumePercentScale)));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleMasterVolumeCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	CommitAudioPercent(Text, CommitMethod, EShooterAudioChannel::Master, TEXT("MasterVolume"), TEXT("TXT_MasterVolumeValue"));
}

void UShooterOptionsScreen::HandleMusicVolumeCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	CommitAudioPercent(Text, CommitMethod, EShooterAudioChannel::Music, TEXT("MusicVolume"), TEXT("TXT_MusicVolumeValue"));
}

void UShooterOptionsScreen::HandleSfxVolumeCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	CommitAudioPercent(Text, CommitMethod, EShooterAudioChannel::SoundEffects, TEXT("SfxVolume"), TEXT("TXT_SfxVolumeValue"));
}

void UShooterOptionsScreen::HandleUiVolumeCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	CommitAudioPercent(Text, CommitMethod, EShooterAudioChannel::UserInterface, TEXT("UiVolume"), TEXT("TXT_UiVolumeValue"));
}

void UShooterOptionsScreen::HandleVoiceVolumeCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	CommitAudioPercent(Text, CommitMethod, EShooterAudioChannel::Voice, TEXT("VoiceVolume"), TEXT("TXT_VoiceVolumeValue"));
}

void UShooterOptionsScreen::HandleVoiceChatMicrophoneVolumeCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	CommitVoiceChatPercent(Text, CommitMethod, true, TEXT("VoiceChatMicrophoneVolume"), TEXT("TXT_VoiceChatMicrophoneVolumeValue"));
}

void UShooterOptionsScreen::HandleVoiceChatOutputVolumeCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	CommitVoiceChatPercent(Text, CommitMethod, false, TEXT("VoiceChatOutputVolume"), TEXT("TXT_VoiceChatOutputVolumeValue"));
}

void UShooterOptionsScreen::HandleResolutionScaleChanged(const float Value)
{
	if (bRefreshingUI) return;
	ActiveOption = TEXT("ResolutionScale");
	SetResolutionScalePercent(Value);
	SetOptionText(TEXT("TXT_ResolutionScaleValue"), FText::FromString(FString::Printf(TEXT("%.0f%%"), Value)));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleHorizontalSensitivityChanged(const float Value)
{
	if (bRefreshingUI) return;
	ActiveOption = TEXT("HorizontalSensitivity");
	SetMouseHorizontalSensitivity(Value);
	SetOptionText(TEXT("TXT_HorizontalSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.3f"), Value)));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleVerticalSensitivityChanged(const float Value)
{
	if (bRefreshingUI) return;
	ActiveOption = TEXT("VerticalSensitivity");
	SetMouseVerticalSensitivity(Value);
	SetOptionText(TEXT("TXT_VerticalSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.3f"), Value)));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleAimSensitivityChanged(const float Value)
{
	if (bRefreshingUI) return;
	ActiveOption = TEXT("AimSensitivity");
	SetAimSensitivityMultiplier(Value);
	SetOptionText(TEXT("TXT_AimSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.3f"), Value)));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleHorizontalSensitivityCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter && CommitMethod != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	ActiveOption = TEXT("HorizontalSensitivity");
	SetMouseHorizontalSensitivity(FCString::Atof(*Text.ToString()));
	// 설정 클래스가 자른 값을 칸에 되쓴다.
	SetOptionText(TEXT("TXT_HorizontalSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.2f"), GetMouseHorizontalSensitivity())));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleVerticalSensitivityCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter && CommitMethod != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	ActiveOption = TEXT("VerticalSensitivity");
	SetMouseVerticalSensitivity(FCString::Atof(*Text.ToString()));
	// 설정 클래스가 자른 값을 칸에 되쓴다.
	SetOptionText(TEXT("TXT_VerticalSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.2f"), GetMouseVerticalSensitivity())));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleAimSensitivityCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter && CommitMethod != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	ActiveOption = TEXT("AimSensitivity");
	SetAimSensitivityMultiplier(FCString::Atof(*Text.ToString()));
	// 설정 클래스가 자른 값을 칸에 되쓴다.
	SetOptionText(TEXT("TXT_AimSensitivityValue"), FText::FromString(FString::Printf(TEXT("%.2f"), GetAimSensitivityMultiplier())));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleAudioVolumeChangedInternal(
	const float Value,
	const EShooterAudioChannel Channel,
	const FName OptionName,
	const FName TextName)
{
	if (bRefreshingUI)
	{
		return;
	}

	ActiveOption = OptionName;
	SetAudioVolume(Channel, Value);
	SetOptionText(TextName, FText::FromString(FString::Printf(TEXT("%.0f"), GetAudioVolume(Channel) * VolumePercentScale)));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleVoiceChatVolumeChangedInternal(
	const float Value,
	const bool bMicrophone,
	const FName OptionName,
	const FName TextName)
{
	if (bRefreshingUI)
	{
		return;
	}

	ActiveOption = OptionName;
	if (bMicrophone)
	{
		SetVoiceChatMicrophoneVolume(Value);
	}
	else
	{
		SetVoiceChatOutputVolume(Value);
	}

	const float AppliedValue = bMicrophone ? GetVoiceChatMicrophoneVolume() : GetVoiceChatOutputVolume();
	SetOptionText(TextName, FText::FromString(FString::Printf(TEXT("%.0f"), AppliedValue * VolumePercentScale)));
	RefreshOptionRowVisuals();
}

void UShooterOptionsScreen::HandleMasterVolumeChanged(const float Value)
{
	HandleAudioVolumeChangedInternal(Value, EShooterAudioChannel::Master, TEXT("MasterVolume"), TEXT("TXT_MasterVolumeValue"));
}

void UShooterOptionsScreen::HandleMusicVolumeChanged(const float Value)
{
	HandleAudioVolumeChangedInternal(Value, EShooterAudioChannel::Music, TEXT("MusicVolume"), TEXT("TXT_MusicVolumeValue"));
}

void UShooterOptionsScreen::HandleSfxVolumeChanged(const float Value)
{
	HandleAudioVolumeChangedInternal(Value, EShooterAudioChannel::SoundEffects, TEXT("SfxVolume"), TEXT("TXT_SfxVolumeValue"));
}

void UShooterOptionsScreen::HandleUiVolumeChanged(const float Value)
{
	HandleAudioVolumeChangedInternal(Value, EShooterAudioChannel::UserInterface, TEXT("UiVolume"), TEXT("TXT_UiVolumeValue"));
}

void UShooterOptionsScreen::HandleVoiceVolumeChanged(const float Value)
{
	HandleAudioVolumeChangedInternal(Value, EShooterAudioChannel::Voice, TEXT("VoiceVolume"), TEXT("TXT_VoiceVolumeValue"));
}

void UShooterOptionsScreen::HandleVoiceChatMicrophoneVolumeChanged(const float Value)
{
	HandleVoiceChatVolumeChangedInternal(Value, true, TEXT("VoiceChatMicrophoneVolume"), TEXT("TXT_VoiceChatMicrophoneVolumeValue"));
}

void UShooterOptionsScreen::HandleVoiceChatOutputVolumeChanged(const float Value)
{
	HandleVoiceChatVolumeChangedInternal(Value, false, TEXT("VoiceChatOutputVolume"), TEXT("TXT_VoiceChatOutputVolumeValue"));
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
