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
class UShooterConfirmDialog;
class UShooterRoomMenuScreen;
class UShooterOptionsScreen;
class UShooterRoomPlayerSlot;
class UTextBlock;

/**
 * 사설 방 화면
 */
UCLASS()
class FPS_API UShooterRoomScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

private:
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
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Leave;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_ModeNext;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ModeName;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MapName;
	// "{N}V{N}" 팀 정원. 방 수용 인원과 다름
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
};
