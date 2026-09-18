// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SessionWidgetBase.generated.h"

class USessionSubSystem;
class UTextBlock;

UCLASS(Abstract, BlueprintType, Blueprintable)
class FPS_API USessionWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 InMaxPlayers = 4, const FString& InMapName = TEXT("DominationMap"), const FString& InMatchType = TEXT("Domination"));

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent)
	void OnMenuStatusMessage(const FText& Message);

	void CacheSessionSubsystem();
	void SetStatusMessage(const FText& Message);
	void MenuTearDown();

protected:
	UPROPERTY(Transient)
	TObjectPtr<USessionSubSystem> SessionSubsystem;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	int32 MaxPlayers = 4;
	FString RoomName = TEXT("");
	FString MapName = TEXT("DominationMap");
	FString MatchType = TEXT("Domination");
};
