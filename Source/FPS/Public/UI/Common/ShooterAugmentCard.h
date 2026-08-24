// ShooterAugmentCard.h
#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ShooterAugmentCard.generated.h"

class UAugmentDefinition;
class UCommonTextBlock;
class UImage;
class UShooterAugmentTierStyleSet;
class UWidgetAnimation;

// 서버가 준 배열의 몇번째 카드인지만 전달
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAugmentCardClicked, int32 /*OfferIndex*/);

/**
 * Frame / Hover / Pressed 는 등급별 CommonButtonStyle에서 정한다.
 */
UCLASS(Abstract)
class FPS_API UShooterAugmentCard : public UCommonButtonBase{
	GENERATED_BODY()
	
public:
	// 카드에 그릴 내용을 넣기위해선 Level이 필요하다.
	// OfferIndex는 서버가 준 배열에서의 위치로 클릭했을때 전달함
	void SetData(const UAugmentDefinition* InDefinition, int32 InLevel, int32 InOfferIndex);
	
	// TODO: 리롤을 실제 교체 연출로 확장 시
	// FadeOut -> SetData -> FadeIn 구조로 변경한다.
	void PlayRerollFeedback();
	
	FOnAugmentCardClicked OnAugmentCardClicked;
	
protected:
	virtual void NativeOnClicked() override;
	
	// Widget 구성 요소
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Name;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Description;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_TierLabel;
	// 강조표시할 애니메이션이 필요한 곳에서 킨다 ex) Legendary 등장 연출
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_AlertBadge;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Highlight;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Reroll;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TObjectPtr<UShooterAugmentTierStyleSet> TierStyles;

	int32 OfferIndex = INDEX_NONE;
};
