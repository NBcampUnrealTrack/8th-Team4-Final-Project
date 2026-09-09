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
class URetainerBox;
class UMaterialInstanceDynamic;
class UWidget;

// 서버가 준 배열의 몇번째 카드인지만 전달
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAugmentCardClicked, int32 /*OfferIndex*/);
// 컨트롤러가 레이아웃 계산하기 위해 필요함
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnAugmentCardHovered, int32 /*OfferIndex*/, bool /*bHovered*/);

UCLASS(Abstract)
class FPS_API UShooterAugmentCard : public UCommonButtonBase{
	GENERATED_BODY()
	
public:
	// OfferIndex는 서버가 준 배열에서의 위치로 클릭했을때 전달함
	void SetData(const UAugmentDefinition* InDefinition, int32 InOfferIndex);
	
	void PlayRerollFeedback();
	
	// 고르지 않은 나머지 카드
	// 고른 카드는 화면의 SelectedCardPresenter 로 넘어가 Anim_InstallOutro 가 연출한다
	void PlayRejectedFeedback();
	void ResetFeedback();
	
	// 디졸브 진행률로 화면의 아웃트로 Alpha
	void SetRejectedProgress(float Progress);

	// 리롤로 새 데이터가 들어오기 직전에 화면이 부른다. 리롤 연출 위함
	void BeginRerollPresentation();
	void SetRerollWipeProgress(float Progress);
	
	// 옛 카드가 얼마나 지워졌는가. 0 = 전부 보임 / 1 = 전부 지워짐.
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Reroll")
	void BP_OnRerollWipeProgress(float Progress);
	// 스냅샷을 뜬 직후 1회. OldAccent 는 직전 등급의 강조색
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Reroll")
	void BP_OnRerollSnapshotTaken(FLinearColor OldAccent);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Selection")
	void BP_OnRejectedProgress(float Progress);
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Selection")
	void BP_OnCardRejected();
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Selection")
	void BP_OnCardFeedbackReset();
	// hover 진입시 스캔라인/글로우
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Selection")
	void BP_OnCardHoverChanged(bool bHovered);
	
	FOnAugmentCardClicked OnAugmentCardClicked;
	FOnAugmentCardHovered OnAugmentCardHovered;
	
	// 등급 확정 직후 한 번, 색 데이터만 넘긴다.
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Accent")
	void BP_OnAccentChanged(FLinearColor Accent);
	
protected:
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnClicked() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	// Widget 구성 요소
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Name;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Description;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_TierLabel;
	// 강조표시할 애니메이션이 필요한 곳에서 킨다 ex) Legendary 등장 연출
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_AlertBadge;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Highlight;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Reroll;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Rejected;
	// 짧은 스캔라인/글로우. hover 진입마다 재생된다
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HoverPulse;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URetainerBox> RetainerBox_Glitch;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Selection")
	FName DissolveParameterName = TEXT("GlitchStrength");
	
	// ㅡ Reroll ㅡ
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> OldCardSnapshot;
	// OldCardSnapshot 에 M_UI_Augment_RerollWipe 
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URetainerBox> RetainerBox_RerollOld;

	// OldCard 복제용
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_OldIcon;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_OldName;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_OldDescription;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_OldTierLabel;

	// 스캔 라인과 파티클
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_RerollScan;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ProgramParticles;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Reroll")
	FName RerollWipeParameterName = TEXT("ScanProgress");

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TObjectPtr<UShooterAugmentTierStyleSet> TierStyles;

	int32 OfferIndex = INDEX_NONE;

private:
	// 스냅샷에 넘겨줄 직전 등급의 강조색. SetData 가 갱신한다
	FLinearColor CachedAccentColor = FLinearColor::White;
};
