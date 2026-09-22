// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Loading/LoadingScreenWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Curves/CurveFloat.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UI/ShooterUISettings.h"

void ULoadingScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	//  팁을 한 번만 뽑음
	PickSessionTip();
}

void ULoadingScreenWidget::SetLoadingProgress(const float InProgress)
{
	const float ClampedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	LoadingProgress = ClampedProgress;

	Tracker.SetTarget(ClampedProgress);

	OnLoadingProgressChanged(ClampedProgress);
}

void ULoadingScreenWidget::ApplyLoadingContext(const FShooterLoadingContext& InContext)
{
	// 모드명과 그 아래 장식선
	const ESlateVisibility ModeVisibility = InContext.HasModeName()
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;

	if (Text_ModeName)
	{
		Text_ModeName->SetText(InContext.ModeName);
		Text_ModeName->SetVisibility(ModeVisibility);
	}

	if (Img_ModeRule)
	{
		Img_ModeRule->SetVisibility(ModeVisibility);
	}

	RequestPreviewTexture(InContext.Thumbnail);
}

void ULoadingScreenWidget::RequestPreviewTexture(const TSoftObjectPtr<UTexture2D>& InThumbnail)
{
	if (InThumbnail.IsNull())
	{
		return;
	}

	// ShowLoadingScreen 은 한 세션에 두 번 이상 온다. 앞선 요청을 먼저 버린다
	if (PreviewHandle.IsValid())
	{
		PreviewHandle->CancelHandle();
		PreviewHandle.Reset();
	}

	const FSoftObjectPath RequestedThumbnail = InThumbnail.ToSoftObjectPath();
	PendingThumbnail = RequestedThumbnail;

	PreviewHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		RequestedThumbnail,
		FStreamableDelegate::CreateWeakLambda(this, [this, RequestedThumbnail]()
		{
			// CancelHandle 은 이미 큐에 들어간 콜백을 되돌리지 못한다
			if (RequestedThumbnail != PendingThumbnail)
			{
				return;
			}

			PendingThumbnail.Reset();

			ApplyPreviewTexture(Cast<UTexture2D>(RequestedThumbnail.ResolveObject()));
		}));
}

void ULoadingScreenWidget::ApplyPreviewTexture(UTexture2D* InTexture)
{
	if (!InTexture)
	{
		return;
	}

	// 스코프 안 블러와 원형 마스크는 머티리얼이 한다
	if (Img_Preview)
	{
		if (!PreviewMID)
		{
			PreviewMID = Img_Preview->GetDynamicMaterial();
		}

		if (PreviewMID)
		{
			PreviewMID->SetTextureParameterValue(PreviewTextureParameterName, InTexture);
			Img_Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// 스코프 밖 배경 같은 머티리얼에 마스크만 끄고 블러는 고정
	if (Img_Backdrop)
	{
		if (!BackdropMID)
		{
			BackdropMID = Img_Backdrop->GetDynamicMaterial();

			if (BackdropMID)
			{
				BackdropMID->SetScalarParameterValue(BlurParameterName, BackdropBlurAmount);
				BackdropMID->SetScalarParameterValue(CircleMaskParameterName, 0.0f);
			}
		}

		if (BackdropMID)
		{
			BackdropMID->SetTextureParameterValue(PreviewTextureParameterName, InTexture);
			Img_Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void ULoadingScreenWidget::PickSessionTip()
{
	const TArray<FText>& Tips = GetDefault<UShooterUISettings>()->LoadingTips;

	if (!Text_Tip || Tips.IsEmpty())
	{
		ApplyTipVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Text_Tip->SetText(Tips[FMath::RandRange(0, Tips.Num() - 1)]);
	ApplyTipVisibility(ESlateVisibility::HitTestInvisible);
}

void ULoadingScreenWidget::ApplyTipVisibility(const ESlateVisibility InVisibility)
{
	if (TipPanel)
	{
		TipPanel->SetVisibility(InVisibility);
		return;
	}

	if (Text_Tip)
	{
		Text_Tip->SetVisibility(InVisibility);
	}
}

void ULoadingScreenWidget::BeginEnterAnimation()
{
	if (Anim_DecorRailIdle)
	{
		PlayAnimation(Anim_DecorRailIdle, 0.0f, 0);
	}

	if (Anim_Enter)
	{
		PlayAnimation(Anim_Enter);
	}
}

void ULoadingScreenWidget::BeginCompletionExit(FSimpleDelegate OnFinished)
{
	Tracker.SetTarget(1.0f);

	if (bCompletionExitPending)
	{
		// 재진입. 타이머는 유지하고 콜백만 최신으로 바꾼다
		CompletionExitDelegate = OnFinished;
		return;
	}

	bCompletionExitPending = true;
	CompletionHoldElapsed = 0.0f;
	CompletionWaitElapsed = 0.0f;
	CompletionExitDelegate = OnFinished;
}

void ULoadingScreenWidget::TickCompletionExit(const float DeltaSeconds)
{
	if (!bCompletionExitPending)
	{
		return;
	}

	CompletionWaitElapsed += DeltaSeconds;

	// 보간이 늦으면 붙인다. 건너뛰지는 않는다
	if (!Tracker.IsVisuallyComplete(ProgressSettings)
		&& CompletionWaitElapsed >= ProgressSettings.CompletionSnapSeconds)
	{
		Tracker.SnapToTarget();

		// 이 프레임의 Tick 은 이미 지나갔다. 부르지 않으면 100% 가 한 프레임 늦는다
		ApplyDisplayProgress();
	}

	// 100% 를 그리기 전에는 걷지 않는다
	if (!Tracker.IsVisuallyComplete(ProgressSettings))
	{
		return;
	}

	// 100% 에 처음 닿은 프레임. 세션당 한 번이다
	if (!bCompletionSoundPlayed)
	{
		bCompletionSoundPlayed = true;

		// 아이들 사운드가 애니메이션 안에 있다.
		// StopAnimation 은 pre-animated 복원을 타서 오디오까지 끊는다 — 틱이 끊겨 멈추는 경로는 안 끊는다
		if (Anim_DecorRailIdle)
		{
			StopAnimation(Anim_DecorRailIdle);
		}

		if (CompletionSound)
		{
			UGameplayStatics::PlaySound2D(this, CompletionSound);
		}
	}

	CompletionHoldElapsed += DeltaSeconds;
	if (CompletionHoldElapsed < ProgressSettings.CompletionHoldSeconds)
	{
		return;
	}

	// Exit 애니가 도는 동안 Tick 이 다시 들어오면 안 됨
	bCompletionExitPending = false;

	if (Anim_Exit)
	{
		bExitAnimationPlaying = true;

		UnbindAllFromAnimationFinished(Anim_Exit);
		ExitAnimationFinishedDelegate.BindDynamic(this, &ThisClass::HandleExitAnimationFinished);
		BindToAnimationFinished(Anim_Exit, ExitAnimationFinishedDelegate);

		PlayAnimation(Anim_Exit);
		return;
	}

	// 애니가 없으면 곧바로 걷는다
	ExecuteCompletionExitDelegate();
}

void ULoadingScreenWidget::HandleExitAnimationFinished()
{
	if (!bExitAnimationPlaying)
	{
		return;
	}

	bExitAnimationPlaying = false;
	ExecuteCompletionExitDelegate();
}

void ULoadingScreenWidget::ExecuteCompletionExitDelegate()
{
	// 재진입 방지
	FSimpleDelegate Local = MoveTemp(CompletionExitDelegate);
	CompletionExitDelegate.Unbind();
	Local.ExecuteIfBound();
}

void ULoadingScreenWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 값이 바뀐 프레임에만 그린다. Tick 은 시간 신호일 뿐이다
	if (Tracker.Tick(InDeltaTime, ProgressSettings))
	{
		ApplyDisplayProgress();
	}

	TickCompletionExit(InDeltaTime);
}

void ULoadingScreenWidget::ApplyDisplayProgress()
{
	const float Display = Tracker.GetDisplay();

	if (LoadingProgressBar)
	{
		LoadingProgressBar->SetPercent(Display);
	}

	// 정수 퍼센트가 바뀔 때만 문자열을 만듦
	int32 PercentInt = FMath::RoundToInt(Display * 100.0f);

	// 100% 는 완료 판정이 난 뒤에만 보여줌
	if (PercentInt >= 100 && !Tracker.IsVisuallyComplete(ProgressSettings))
	{
		PercentInt = 99;
	}

	if (Text_Percent && PercentInt != LastDisplayedPercent)
	{
		LastDisplayedPercent = PercentInt;
		Text_Percent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), PercentInt)));
	}

	if (SizeBox_FocusRotationRoot)
	{
		const float Angle = FMath::Lerp(DialStartAngle, DialStartAngle + DialTravelAngle, Display);
		SizeBox_FocusRotationRoot->SetRenderTransformAngle(Angle);
	}

	if (PreviewMID)
	{
		const float Sharpness = SharpnessCurve
			? SharpnessCurve->GetFloatValue(Display)
			: Display;
		PreviewMID->SetScalarParameterValue(BlurParameterName, 1.0f - Sharpness);

		if (BrightnessCurve)
		{
			// 커브가 없으면 안 건드림
			PreviewMID->SetScalarParameterValue(
				BrightnessParameterName, BrightnessCurve->GetFloatValue(Display));
		}
	}
}

float ULoadingScreenWidget::GetLoadingProgress() const
{
	return LoadingProgress;
}

float ULoadingScreenWidget::GetDisplayProgress() const
{
	return Tracker.GetDisplay();
}

void ULoadingScreenWidget::ResetLoadingSession()
{
	PendingThumbnail.Reset();

	bCompletionSoundPlayed = false;
	LastDisplayedPercent = -1;

	bCompletionExitPending = false;
	CompletionExitDelegate.Unbind();

	// 애니메이션과 사운드를 전부 끊는다.
	if (Anim_Exit)
	{
		UnbindAllFromAnimationFinished(Anim_Exit);
	}

	bExitAnimationPlaying = false;

	StopAllAnimations();

	if (PreviewHandle.IsValid())
	{
		PreviewHandle->CancelHandle();
		PreviewHandle.Reset();
	}

	if (Img_Preview)
	{
		Img_Preview->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Img_Backdrop)
	{
		Img_Backdrop->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 지우기만 한다. 다음 팁은 다음 위젯의 NativeOnInitialized 가 뽑는다
	ApplyTipVisibility(ESlateVisibility::Collapsed);

	Tracker.Reset();
	LoadingProgress = 0.0f;
	ApplyDisplayProgress();
}
