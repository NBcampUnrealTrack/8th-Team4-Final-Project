// DominationPlayerSlot.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationPlayerSlot.generated.h"

class ADominationPlayerState;
class UCommonTextBlock;
class UImage;
class UTexture2D;

UENUM(BlueprintType)
enum class EDominationSlotState : uint8
{
	Waiting,
	Selecting,
	Locked,
	Disconnected
};

/**
 * 클래스 선택 화면 좌측의 팀원 슬롯
 */
UCLASS(Abstract)
class FPS_API UDominationPlayerSlot : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayerState(ADominationPlayerState* InPlayerState, bool bInIsLocalPlayer);
	void SetIconTexture(UTexture2D* IconTexture, UTexture2D* PortraitTexture);

	void RefreshFromPlayerState();

	void NotifyDisconnected();

	// 화면이 아이콘 캐시를 조회하려면 슬롯이 판정한 표시 클래스를 알아야 한다.
	EDominationCombatClass GetDisplayClass() const;
	int32 GetCachedPlayerId() const;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Class Selection")
	void BP_UpdateSlotPresentation(EDominationSlotState State, bool bIsLocalSlot);
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Class Selection")
	void BP_OnSlotStateChanged(EDominationSlotState NewState, EDominationSlotState OldState, bool bIsLocalSlot);

private:
	void RefreshClassNameText();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Portrait;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_WeaponArt;
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_PlayerName;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_ClassName;

	TWeakObjectPtr<ADominationPlayerState> BoundPlayerState;

	FString CachedPlayerName;
	int32 CachedPlayerId = INDEX_NONE;
	EDominationCombatClass CachedDisplayClass = EDominationCombatClass::Unselected;
	// 슬롯 상태 전환 연출
	EDominationSlotState LastPresentedState = EDominationSlotState::Disconnected;

	bool bIsLocalPlayer = false;
	bool bForcedDisconnected = false;
};
