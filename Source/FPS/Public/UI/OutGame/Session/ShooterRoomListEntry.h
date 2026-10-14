// ShooterRoomListEntry.h
#pragma once

#include "CoreMinimal.h"
#include "Session/RoomInfo.h"
#include "UI/Foundation/ShooterButtonBase.h"
#include "ShooterRoomListEntry.generated.h"

class UTextBlock;

/**
 * 방 목록 한 줄. 행 전체가 버튼
 */
UCLASS(meta = (DisableNativeTick))
class FPS_API UShooterRoomListEntry : public UShooterButtonBase
{
	GENERATED_BODY()

public:
	void SetRoomInfo(const FRoomInfo& InRoomInfo);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RoomName;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MapName;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MatchType;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_PlayerCount;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Ping;
	// 참가할 수 없을 때만 보임
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_JoinState;
};
