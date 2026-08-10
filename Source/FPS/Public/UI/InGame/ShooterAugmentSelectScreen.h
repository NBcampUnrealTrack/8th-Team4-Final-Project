// ShooterAugmentSelectScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Augment/AugmentTypes.h"
#include "UI/Common/ShooterAugmentCard.h"
#include "ShooterAugmentSelectScreen.generated.h"

class UAugmentRegistrySubsystem;
class UHorizontalBox;
class UWidgetAnimation;
struct FStreamableHandle;

// 고른 번호를 서버에 보낸다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAugmentSelected, int32, OfferIndex);

/**
 * 증강 선택 화면으로 Modal 레이어에 올린다.
 */
UCLASS(Abstract)
class FPS_API UShooterAugmentSelectScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnActivated() override;
	
	// 서버가 제시한 목록을 넣고, 순서가 곧 인덱스이므로 재정렬 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetOffers(const TArray<FAugmentInventoryEntry>& InOffers);
	
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnAugmentSelected OnAugmentSelected;

	/** 테스트 후 삭제 */
#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "UI|Debug")
	void DebugFillWithFirstOffers(int32 Count);
#endif
	
protected:
	virtual void NativeDestruct() override;
	
	// 준비 확인 -> 아이콘 요청 -> 카드 생성
	// 준비되지않으면 기다렸다 다시 들어온다
	void TryBuildCards();
	
	UFUNCTION()
	void HandleRegistryReady();
	
	void HandleIconsLoaded();
	void HandleCardClicked(int32 InOfferIndex);
	
	UAugmentRegistrySubsystem* GetRegistry() const;
	
	// ShooterAugmentCard가 채워지는 자리
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> Box_Cards;
	// 화면 등장 연출
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Intro;
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UShooterAugmentCard> CardClass;
	
	TArray<FAugmentInventoryEntry> Offers;
	
	TSharedPtr<FStreamableHandle> IconLoadHandle;
	

};
