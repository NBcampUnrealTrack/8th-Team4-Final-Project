// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class FPS_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void SetLoadingProgress(float InProgress);

	UFUNCTION(BlueprintPure, Category = "Loading")
	float GetLoadingProgress() const { return LoadingProgress; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
	void OnLoadingProgressChanged(float InProgress);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> LoadingProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LoadingPercentText;

	float LoadingProgress = 0.0f;
};
