// ShooterRoomPlayerSlot.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/OutGame/Room/ShooterRoomTypes.h"
#include "ShooterRoomPlayerSlot.generated.h"

class UShooterButtonBase;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoomSlotJoinRequested, EGameTeam);

/**
 * 사설 방 화면의 한 칸.
 */
UCLASS()
class FPS_API UShooterRoomPlayerSlot : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayer(const FShooterRoomSlotView& View);
	// bJoinable 이 false 면 칸은 비어 보이되 눌리지 않는다
	void SetEmpty(EGameTeam InSlotTeam, bool bJoinable = true);

	FOnRoomSlotJoinRequested OnJoinRequested;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void BP_ApplySlotState(bool bHasPlayer, const FShooterRoomSlotView& View, EGameTeam InSlotTeam);

	// 같은 사람의 레디가 바뀌었을 때만. 색만 굴리고 이름·왕관은 두는 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Lobby")
	void BP_PlayReadyTransition(bool bIsReady);

	// 칸 전체를 덮는 투명 버튼. 빈 칸일 때만 보임
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterButtonBase> Btn_Join;

	EGameTeam SlotTeam = EGameTeam::None;

private:
	void HandleJoinClicked();

	// 연출 판정용. 칸이 아니라 사람을 기억함
	int32 CachedPlayerId = INDEX_NONE;
	bool bCachedReady = false;
	bool bCachedIsHost = false;
};
