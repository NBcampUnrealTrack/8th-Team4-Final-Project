// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Session/RoomInfo.h"
#include "MenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class URoomEntryWidget;
class USessionSubSystem;
class UScrollBox;
class USpinBox;
class UTextBlock;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class FPS_API UMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 InMaxPlayers = 4, const FString& InMapName = TEXT("BombMap"), const FString& InMatchType = TEXT("FreeForAll"));

	UFUNCTION(BlueprintCallable)
	void HostRoom();

	UFUNCTION(BlueprintCallable)
	void HostRoomWithOptions(int32 InMaxPlayers, const FString& InRoomName, const FString& InMapName, const FString& InMatchType);

	UFUNCTION(BlueprintCallable)
	void RefreshRooms();

	UFUNCTION(BlueprintCallable)
	void JoinRoom(int32 SessionIndex);

	UFUNCTION(BlueprintCallable)
	void JoinLocalHost();

	UFUNCTION(BlueprintCallable)
	void SetLobbyReady(bool bNewLobbyReady);

	UFUNCTION(BlueprintCallable)
	void StartRoomGame();

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent)
	void OnCreateRoomResult(bool bWasSuccessful);

	UFUNCTION(BlueprintImplementableEvent)
	void OnFindRoomsResult(const TArray<FRoomInfo>& Rooms, bool bWasSuccessful);

	UFUNCTION(BlueprintImplementableEvent)
	void OnJoinRoomResult(bool bWasSuccessful);

	UFUNCTION(BlueprintImplementableEvent)
	void OnLeaveRoomResult(bool bWasSuccessful);

	UFUNCTION(BlueprintImplementableEvent)
	void OnStartRoomGameResult(bool bWasSuccessful);

	UFUNCTION(BlueprintImplementableEvent)
	void OnMenuStatusMessage(const FText& Message);

private:
	UFUNCTION()
	void HostButtonClicked();

	UFUNCTION()
	void JoinButtonClicked();

	UFUNCTION()
	void LocalJoinButtonClicked();

	UFUNCTION()
	void ReadyButtonClicked();

	UFUNCTION()
	void StartButtonClicked();

	UFUNCTION()
	void CreateButtonClicked();

	UFUNCTION()
	void CancelCreateButtonClicked();

	UFUNCTION()
	void HandleCreateRoomComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleFindRoomsComplete(const TArray<FRoomInfo>& Rooms, bool bWasSuccessful);

	UFUNCTION()
	void HandleJoinRoomComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleLeaveRoomComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleStartRoomGameComplete(bool bWasSuccessful);

	void CacheSessionSubsystem();
	void BindSessionDelegates();
	void UnbindSessionDelegates();
	void RebuildRoomList(const TArray<FRoomInfo>& Rooms);
	void ShowCreateRoomPanel();
	void HideCreateRoomPanel();
	void SetStatusMessage(const FText& Message);
	void MenuTearDown();

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> JoinButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LocalJoinButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelCreateButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomNameInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpinBox> MaxPlayersSpinBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CreateRoomPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RoomListArea;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> RoomListBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URoomEntryWidget> RoomEntryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<USessionSubSystem> SessionSubsystem;

	bool bSessionDelegatesBound = false;

	int32 MaxPlayers = 4;
	FString RoomName = TEXT("");
	FString MapName = TEXT("BombMap");
	FString MatchType = TEXT("FreeForAll");
};
