// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Session/SessionWidgetBase.h"
#include "LobbyWidget.generated.h"

class ALobbyGameState;
class UButton;
class ULobbyPlayerEntryWidget;
class UVerticalBox;

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
	virtual void NativeConstruct() override;
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
	void BindLobbyGameState();
	void UnbindLobbyGameState();
	void RefreshLobbyPlayerList();
	void UpdateLobbyButtons();
	bool IsLocalPlayerHost() const;
	FString BuildLobbyListSignature() const;

	UFUNCTION()
	void HandleLobbyPlayerListChanged();

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlayerListBox;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TSubclassOf<ULobbyPlayerEntryWidget> LobbyPlayerEntryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ALobbyGameState> BoundLobbyGameState;

	FString CachedLobbyListSignature;
	bool bSessionDelegatesBound = false;
};
