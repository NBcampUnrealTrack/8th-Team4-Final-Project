// ShooterRoomScreen.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "GameMode/Types/LobbyTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "UI/OutGame/Room/ShooterRoomTypes.h"
#include "ShooterRoomScreen.generated.h"

class ALobbyGameState;
class AShooterPlayerController;
class AShooterPlayerState;
class UImage;
class UPanelWidget;
class UShooterButtonBase;
class UShooterChatInputBox;
class UShooterConfirmDialog;
class UShooterRoomMenuScreen;
class UShooterOptionsScreen;
class UShooterRoomPlayerSlot;
class UTextBlock;

/**
 * 사설 방 화면
 */
UCLASS(meta = (DisableNativeTick))
class FPS_API UShooterRoomScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// 인게임 UTeamMatchUIInitComponent::OpenChat과 같은 방식으로 입력창을 Modal에 Push한다.
	void OpenChat();
	void Refresh();
	void BindToLobbyGameState(ALobbyGameState* LobbyGameState);
	void UnbindFromLobbyGameState();
	TArray<FShooterRoomSlotView> BuildSlotViews(const ALobbyGameState* LobbyGameState) const;
	FString BuildPresentationSignature(const TArray<FShooterRoomSlotView>& Views, const ALobbyGameState* LobbyGameState) const;
	void RefreshTeamPanels(const TArray<FShooterRoomSlotView>& Views);
	void RefreshModePanel(ELobbyMatchMode SelectedMatchMode, bool bIsHost);
	void RefreshActions(const ALobbyGameState* LobbyGameState, bool bIsHost);

	void RefreshLocalActions();

	// 슬롯 그리드
	void EnsureTeamSlots(
		UPanelWidget* Container,
		TArray<TObjectPtr<UShooterRoomPlayerSlot>>& OutSlots,
		EGameTeam Team,
		int32 Capacity);
	void ApplyTeamSlots(
		const TArray<TObjectPtr<UShooterRoomPlayerSlot>>& Slots,
		const TArray<FShooterRoomSlotView>& TeamViews,
		EGameTeam Team);
	void RebuildUnassignedEntries(const TArray<FShooterRoomSlotView>& UnassignedViews);
	void RefreshTeamHeaders(const TArray<FShooterRoomSlotView>& Views);
	void RefreshTalkingIndicators(const TArray<FShooterRoomSlotView>& Views);
	void HandleSlotJoinRequested(EGameTeam Team);

	// 모드 패널
	void StepMatchMode(int32 Delta);
	void RequestThumbnails();

	// 상태 바 · 방 정보 · 나가기
	FText BuildStatusText(const FLobbyStartState& StartState, bool bIsHost) const;
	void RefreshRoomHeader(const TArray<FShooterRoomSlotView>& Views, const ALobbyGameState* LobbyGameState);
	void LeaveToFrontend();

	UFUNCTION()
	void HandleLeaveConfirmed();
	UFUNCTION()
	void HandleLeaveCanceled();

	void ClearLeaveConfirmBindings();

	void HandleReadyClicked();
	void HandleStartClicked();
	void HandleModeNextClicked();
	void HandleLeaveClicked();

	// ESC 로 여는 방 메뉴. Settings 와 나가기를 고르게 한다
	void PresentRoomMenu();
	void RemoveRoomMenu();

	void PresentOptionsScreen();
	void HandleOptionsCloseRequested();

	UFUNCTION()
	void HandleLobbyPlayerListChanged();

	UFUNCTION()
	void HandleLobbyMatchModeChanged(ELobbyMatchMode SelectedMatchMode);

	void HandleLobbyStartFailed();

	ALobbyGameState* GetLobbyGameState() const;
	AShooterPlayerController* GetLocalShooterPlayerController() const;
	AShooterPlayerState* GetLocalShooterPlayerState() const;
	bool IsLocalPlayerHost() const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_TeamA;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_TeamB;
	// 팀을 고르지 않은 플레이어
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_Unassigned;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Ready;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Start;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Wrapper_Ready;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Wrapper_Start;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Ready;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Leave;
	
	// BindWidget -> BindWidgetOptional
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UShooterButtonBase> Btn_ModeNext;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ModeName;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MapName;
	// 팀 정원
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_TeamSize;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_MapThumbnail;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_RoomTitle;
	// 방 수용 인원 기준
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_PlayerCount;

	// 레디 수 / 팀 인원
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_TeamAReady;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_TeamBReady;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_UnassignedCount;

	// 대기방 채팅 입력창. 인게임 TeamMatchUIInitComponent의 ChatInputBoxClass와 같은 WBP를 지정한다.
	// 채팅 로그는 WBP_ShooterChatLog를 화면에 직접 배치하면 스스로 PC에 구독한다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Chat")
	TSubclassOf<UShooterChatInputBox> ChatInputBoxClass;

	// 입력창 WBP는 인게임 HUD 배치 기준으로 위치가 잡혀 있어 대기방 채팅 로그와 어긋난다.
	// WBP를 건드리면 인게임도 같이 바뀌므로, 대기방에서 Push할 때만 이만큼 옮긴다(px, +Y가 아래).
	UPROPERTY(EditDefaultsOnly, Category = "UI|Chat")
	FVector2D ChatInputScreenOffset = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UShooterRoomPlayerSlot> PlayerSlotWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UShooterRoomPlayerSlot> UnassignedEntryWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UShooterConfirmDialog> ConfirmDialogClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UShooterRoomMenuScreen> RoomMenuClass;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UShooterOptionsScreen> OptionsScreenClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterRoomPlayerSlot>> TeamSlotsA;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterRoomPlayerSlot>> TeamSlotsB;

	TWeakObjectPtr<ALobbyGameState> BoundLobbyGameState;
	TWeakObjectPtr<AShooterPlayerController> BoundPlayerController;

	FString CachedPresentationSignature;
	FTimerHandle ReconcileTimerHandle;

	TSharedPtr<FStreamableHandle> ThumbnailHandle;

	TWeakObjectPtr<UShooterConfirmDialog> LeaveConfirmDialog;
	TWeakObjectPtr<UShooterRoomMenuScreen> RoomMenuInstance;
	TWeakObjectPtr<UShooterOptionsScreen> OptionsScreenInstance;

	// 서버가 복제한 시작 가능 여부
	bool bStartAllowedByServer = false;

	// 시작을 누른 뒤 복제가 올 때까지의 로컬 잠금
	bool bLocalStartPending = false;

	bool bVoiceCaptureStartedByScreen = false;
};
