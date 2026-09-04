// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/LobbyTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "TimerManager.h"
#include "UI/Session/SessionWidgetBase.h"
#include "LobbyWidget.generated.h"

class ALobbyGameState;
class AShooterPlayerController;
class AShooterPlayerState;
class UButton;
class ULobbyPlayerEntryWidget;
class UTextBlock;
class UVerticalBox;

UCLASS(BlueprintType, Blueprintable)
class FPS_API ULobbyWidget : public USessionWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetLobbyReady(bool bNewLobbyReady);

	// 같은 버튼으로 레디를 걸고 풀 수 있도록 토글 진입점을 분리
	UFUNCTION(BlueprintCallable)
	void ToggleLobbyReady();

	// 플레이어가 원하는 팀을 직접 고를 수 있도록 추가
	UFUNCTION(BlueprintCallable)
	void SelectTeam(EGameTeam RequestedTeam);

	// Host-only selection. Clients receive the replicated result from LobbyGameState.
	UFUNCTION(BlueprintCallable, Category = "Lobby|Game Mode")
	void SelectGameMode(ELobbyMatchMode RequestedMatchMode);

	UFUNCTION(BlueprintPure, Category = "Lobby|Game Mode")
	ELobbyMatchMode GetSelectedGameMode() const;

	UFUNCTION(BlueprintCallable)
	void StartRoomGame();

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent)
	void OnStartRoomGameResult(bool bWasSuccessful);

	// Optional Blueprint styling hook for selected/unselected mode presentation.
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby|Game Mode")
	void OnLobbyGameModeSelectionChanged(ELobbyMatchMode SelectedMatchMode, bool bCanEditSelection);

private:
	UFUNCTION()
	void ReadyButtonClicked();

	UFUNCTION()
	void StartButtonClicked();

	UFUNCTION()
	void TeamAButtonClicked();

	UFUNCTION()
	void TeamBButtonClicked();

	UFUNCTION()
	void BombModeButtonClicked();

	UFUNCTION()
	void DominationModeButtonClicked();

	UFUNCTION()
	void HandleStartRoomGameComplete(bool bWasSuccessful);

	void BindSessionDelegates();
	void UnbindSessionDelegates();
	void BindLobbyGameState();
	void UnbindLobbyGameState();
	void RefreshLobbyPlayerList();
	void RetryInitialLobbyRefresh();
	void StopInitialLobbyRefreshRetry();
	void UpdateLobbyButtons();
	void UpdateGameModeSelection();
	bool IsLocalPlayerHost() const;
	AShooterPlayerState* GetLocalShooterPlayerState() const;
	AShooterPlayerController* GetLocalShooterPlayerController() const;
	FString BuildLobbyListSignature() const;

	UFUNCTION()
	void HandleLobbyPlayerListChanged();

	UFUNCTION()
	void HandleLobbyMatchModeChanged(ELobbyMatchMode SelectedMatchMode);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartButton;

	// 팀 선택 버튼. BP 에서 바인딩하지 않으면 기존처럼 자동 배정만 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TeamAButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TeamBButton;

	// Exact WBP names auto-bind these controls; no Blueprint OnClicked wiring is required.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BombModeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DominationModeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedGameModeText;

	// 레디 버튼 라벨을 C++ 에서 직접 토글하기 위한 선택 바인딩.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> PlayerListBox;

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TSubclassOf<ULobbyPlayerEntryWidget> LobbyPlayerEntryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ALobbyGameState> BoundLobbyGameState;

	FString CachedLobbyListSignature;
	FTimerHandle InitialLobbyRefreshRetryTimerHandle;
	int32 InitialLobbyRefreshRetryCount = 0;
	bool bSessionDelegatesBound = false;
};
