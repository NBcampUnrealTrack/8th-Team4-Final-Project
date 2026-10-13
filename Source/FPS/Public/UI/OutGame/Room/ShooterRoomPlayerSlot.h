// ShooterRoomPlayerSlot.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/OutGame/Room/ShooterRoomTypes.h"
#include "ShooterRoomPlayerSlot.generated.h"

class UImage;
class UShooterButtonBase;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoomSlotJoinRequested, EGameTeam);

/**
 * 사설 방 화면의 한 칸.
 */
UCLASS(meta = (DisableNativeTick))
class FPS_API UShooterRoomPlayerSlot : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayer(const FShooterRoomSlotView& View);
	// bJoinable 이 false 면 칸은 비어 보이되 눌리지 않는다
	void SetEmpty(EGameTeam InSlotTeam, bool bJoinable = true);
	// 시그니처 비교와 무관하게 reconcile 마다 갱신
	void SetTalking(bool bIsTalking);

	int32 GetCachedPlayerId() const { return CachedPlayerId; }

	FOnRoomSlotJoinRequested OnJoinRequested;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void BP_ApplySlotState(bool bHasPlayer, const FShooterRoomSlotView& View, EGameTeam InSlotTeam);
	// 같은 사람의 레디가 바뀌었을 때 연출만
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void BP_PlayReadyTransition(bool bIsReady);
	// 애니 없이 레디 색만
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void BP_ApplyReadyColor(bool bIsReady);

	// 칸 전체를 덮는 투명 버튼
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Join;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Join;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_Profile;
	// 말하는 중 아이콘
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Img_VoiceTalking;

	// 지속 상태 배경. C++ 만 켜고 끔
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Image_Background_TeamA;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Image_Background_TeamB;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Image_BG_Ready_A;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Image_BG_Ready_B;

	EGameTeam SlotTeam = EGameTeam::None;

private:
	void HandleJoinClicked();
	void ApplyStateBackground(EShooterRoomSlotState State);
	void StopReadyTransition();

	// 연출 판정용. 칸이 아니라 사람을 기억함
	int32 CachedPlayerId = INDEX_NONE;
	bool bCachedReady = false;
	bool bCachedIsHost = false;

	UPROPERTY(Transient)
	FSlateBrush DefaultProfileBrush;
};
