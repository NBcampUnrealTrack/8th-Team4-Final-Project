// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Loading/LoadingScreenWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void ULoadingScreenWidget::SetLoadingProgress(const float InProgress)
{
	const float ClampedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	LoadingProgress = ClampedProgress;

	Tracker.SetTarget(ClampedProgress);

	OnLoadingProgressChanged(ClampedProgress);
}



void ULoadingScreenWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	Tracker.Tick(InDeltaTime, ProgressSettings);
	ApplyDisplayProgress();
}

void ULoadingScreenWidget::ApplyDisplayProgress()
{
	const float Display = Tracker.GetDisplay();

	if (LoadingProgressBar)
	{
		LoadingProgressBar->SetPercent(Display);
	}

	const FText PercentText = FText::FromString(
		FString::Printf(TEXT("%.0f%%"), Display * 100.0f));

	if (LoadingPercentText)
	{
		LoadingPercentText->SetText(PercentText);
	}

	if (Text_Percent)
	{
		Text_Percent->SetText(PercentText);
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
	Tracker.Reset();
	LoadingProgress = 0.0f;
	ApplyDisplayProgress();
}
