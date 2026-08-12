// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/SessionWidgetBase.h"
#include "LobbyWidget.generated.h"

class UButton;

UCLASS(BlueprintType, Blueprintable)
class FPS_API ULobbyWidget : public USessionWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetLobbyReady(bool bNewLobbyReady);

	UFUNCTION(BlueprintCallable)
	void StartRoomGame();

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent)
	void OnStartRoomGameResult(bool bWasSuccessful);

private:
	UFUNCTION()
	void ReadyButtonClicked();

	UFUNCTION()
	void StartButtonClicked();

	UFUNCTION()
	void HandleStartRoomGameComplete(bool bWasSuccessful);

	void BindSessionDelegates();
	void UnbindSessionDelegates();

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartButton;

	bool bSessionDelegatesBound = false;
};
