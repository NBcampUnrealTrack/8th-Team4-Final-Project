// ShooterAugmentSelectScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Augment/AugmentTypes.h"
#include "UI/Common/ShooterAugmentCard.h"
#include "ShooterAugmentSelectScreen.generated.h"

class UShooterRerollButton;
class UAugmentRegistrySubsystem;
class UHorizontalBox;
class UUniformGridPanel;
class UWidgetAnimation;
struct FStreamableHandle;

// 고른 번호를 서버에 보낸다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAugmentSelected, int32, OfferIndex);
// 리롤할 슬롯 번호를 서버에 보냄
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAugmentRerollRequested, int32, SlotIndex);
// 보류 요청
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentSelectDismissRequested);

/**
 * 증강 선택 화면으로 Modal 레이어에 올린다.
 */
UCLASS(Abstract)
class FPS_API UShooterAugmentSelectScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	// 서버가 제시한 목록을 넣고, 순서가 곧 인덱스이므로 재정렬 하지 않는다.
	// RerollsRemaining은 Offers와 길이가 같음
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetOffers(const TArray<FAugmentInventoryEntry>& InOffers,
	               const TArray<int32>& InRerollsRemaining);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetDismissAvailable(bool bAvailable);
	
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnAugmentSelected OnAugmentSelected;
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnAugmentRerollRequested OnAugmentRerollRequested;
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnAugmentSelectDismissRequested OnDismissRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// 준비 확인 -> 아이콘 요청 -> 카드 생성
	// 준비되지않으면 기다렸다 다시 들어온다
	void TryBuildCards();

	void RefreshOfferWidgets();

	UFUNCTION()
	void HandleRegistryReady();

	void HandleIconsLoaded();
	void HandleCardClicked(int32 InOfferIndex);
	void HandleRerollButtonClicked(int32 InSlotIndex);
	void HandlePendingCardClicked();
	
	UAugmentRegistrySubsystem* GetRegistry() const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterAugmentCard> Card_0;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterAugmentCard> Card_1;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterAugmentCard> Card_2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterRerollButton> RerollButton_0;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterRerollButton> RerollButton_1;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterRerollButton> RerollButton_2;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_PendingCard;
	
	// 화면 등장 연출
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Intro;

	TArray<FAugmentInventoryEntry> Offers;

	TArray<int32> RerollsRemaining;

	// 최초 1회 생성 후 파괴하지않고 값만 다시 채운다
	UPROPERTY()
	TArray<TObjectPtr<UShooterAugmentCard>> CachedCards;
	UPROPERTY()
	TArray<TObjectPtr<UShooterRerollButton>> CachedRerollButtons;

	// Deactivated에서 리셋된다
	TArray<FAugmentInventoryEntry> LastDisplayedOffers;

	TSharedPtr<FStreamableHandle> IconLoadHandle;
};
