// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Loading/ShooterLoadingProgressTracker.h"
#include "LoadingScreenWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class FPS_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 새 로딩 세션
	void ResetLoadingSession();
	
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void SetLoadingProgress(float InProgress);

	UFUNCTION(BlueprintPure, Category = "Loading")
	float GetLoadingProgress() const;
	UFUNCTION(BlueprintPure, Category = "Loading")
	float GetDisplayProgress() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
	void OnLoadingProgressChanged(float InProgress);

	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	FShooterLoadingProgressSettings ProgressSettings;

private:
	void ApplyDisplayProgress();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> LoadingProgressBar;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LoadingPercentText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Percent;

	FShooterLoadingProgressTracker Tracker;

	float LoadingProgress = 0.0f;
};
