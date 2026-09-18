// ShooterSessionBrowserScreen.h
#pragma once

#include "CoreMinimal.h"
#include "Session/RoomInfo.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterSessionBrowserScreen.generated.h"

class UCommonActivatableWidget;
class UScrollBox;
class UShooterButtonBase;
class UShooterRoomListEntry;
class USessionSubSystem;
class UTextBlock;
class UWidget;

/**
 * Menu 1번 칸. 방 찾기 · 목록 · 참가 요청
 */
UCLASS()
class FPS_API UShooterSessionBrowserScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void BP_ApplyStatus(bool bHasStatus);

private:
	void HandleRefreshClicked();
	void HandleCreateRoomClicked();
	void HandleBackClicked();
	void HandleLocalJoinClicked();
	void HandleRoomClicked(int32 SessionIndex);

	UFUNCTION()
	void HandleFindRoomsComplete(const TArray<FRoomInfo>& Rooms, bool bWasSuccessful);
	UFUNCTION()
	void HandleJoinRoomComplete(bool bWasSuccessful);

	USessionSubSystem* GetSessionSubsystem() const;
	void StartFindRooms();
	void RebuildRoomList(const TArray<FRoomInfo>& Rooms);
	void SetBusy(bool bNewBusy, const FText& StatusText);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Refresh;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_CreateRoom;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Back;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_Rooms;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UShooterButtonBase> Btn_LocalJoin;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Join;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UShooterRoomListEntry> RoomEntryWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> CreateRoomDialogClass;

	// 찾기·참가 진행 중. 목록과 버튼을 잠금
	bool bIsBusy = false;
};
