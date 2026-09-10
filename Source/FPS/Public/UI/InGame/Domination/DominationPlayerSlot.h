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
	void UpdateSlotPresentation(EDominationSlotState State, bool bIsLocalSlot);

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

	// PlayerState가 파괴돼도 이탈 슬롯을 그리려면 표시값이 남아 있어야 한다.
	FString CachedPlayerName;
	int32 CachedPlayerId = INDEX_NONE;
	EDominationCombatClass CachedDisplayClass = EDominationCombatClass::Unselected;

	bool bIsLocalPlayer = false;
	bool bForcedDisconnected = false;
};
