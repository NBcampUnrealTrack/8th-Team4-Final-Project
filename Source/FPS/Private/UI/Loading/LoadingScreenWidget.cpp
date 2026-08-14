// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Loading/LoadingScreenWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void ULoadingScreenWidget::SetLoadingProgress(const float InProgress)
{
	const float ClampedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	LoadingProgress = ClampedProgress;

	if (LoadingProgressBar)
	{
		LoadingProgressBar->SetPercent(ClampedProgress);
	}

	if (LoadingPercentText)
	{
		LoadingPercentText->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), ClampedProgress * 100.0f)));
	}

	OnLoadingProgressChanged(ClampedProgress);
}
