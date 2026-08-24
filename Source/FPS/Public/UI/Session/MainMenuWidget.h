// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Session/RoomInfo.h"
#include "UI/Session/SessionWidgetBase.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class URoomEntryWidget;
class UScrollBox;
class USlider;
class USpinBox;
class UTextBlock;
class UWidget;

UCLASS(BlueprintType, Blueprintable)
class FPS_API UMainMenuWidget : public USessionWidgetBase
{
	GENERATED_BODY()

public:
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

private:
	UFUNCTION()
	void HostButtonClicked();

	UFUNCTION()
	void JoinButtonClicked();

	UFUNCTION()
	void LocalJoinButtonClicked();

	UFUNCTION()
	void CreateButtonClicked();

	UFUNCTION()
	void CancelCreateButtonClicked();

	UFUNCTION()
	void HandleMaxPlayersSliderChanged(float Value);

	UFUNCTION()
	void HandleCreateRoomComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleFindRoomsComplete(const TArray<FRoomInfo>& Rooms, bool bWasSuccessful);

	UFUNCTION()
	void HandleJoinRoomComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleLeaveRoomComplete(bool bWasSuccessful);

	void BindSessionDelegates();
	void UnbindSessionDelegates();
	void RebuildRoomList(const TArray<FRoomInfo>& Rooms);
	void ShowCreateRoomPanel();
	void HideCreateRoomPanel();

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HostButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> JoinButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LocalJoinButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelCreateButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> RoomNameInput;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpinBox> MaxPlayersSpinBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MaxPlayersSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxPlayersValueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CreateRoomPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RoomListArea;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> RoomListBox;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Room", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<URoomEntryWidget> RoomEntryWidgetClass;

	bool bSessionDelegatesBound = false;
};
