// ShooterAugmentCard.cpp
#include "UI/Common/ShooterAugmentCard.h"
#include "UI/Common/ShooterAugmentTierStyleSet.h"
#include "Augment/AugmentDefinition.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAugmentCard, Log, All);

void UShooterAugmentCard::SetData(const UAugmentDefinition* InDefinition, int32 InOfferIndex)
{
	OfferIndex = InOfferIndex;

	if (IsValid(InDefinition) == false)
	{
		UE_LOG(LogShooterAugmentCard, Error, TEXT("%d번 카드에 증강 정의가 없어 카드를 그리지 않습니다"), InOfferIndex);
		return;
	}

	Text_Name->SetText(InDefinition->DisplayName);

	// Icon은 선택 화면이 UI 번들을 미리 올려둔 것을 꺼내 쓴다
	// 아이콘이 없어도 그려야 하므로 return 하지 않는다
	if (UTexture2D* Icon = InDefinition->Icon.Get())
	{
		Image_Icon->SetBrushFromTexture(Icon);
	}

	const FAugmentLevelData* LevelData = InDefinition->GetLevelData();
	if (LevelData == nullptr)
	{
		UE_LOG(LogShooterAugmentCard, Error, TEXT("%s 에 수치가 없어 카드를 그리지 않습니다"), *InDefinition->GetName());
		return;
	}

	Text_Description->SetText(LevelData->Description);

	if (TierStyles == nullptr)
	{
		UE_LOG(LogShooterAugmentCard, Error, TEXT("AugmentCard에 UI|TierStyles가 없습니다. 지정해주세요"));
		return;
	}

	const FShooterAugmentTierStyle* TierStyle = TierStyles->FindStyle(InDefinition->Tier);
	if (TierStyle == nullptr)
	{
		UE_LOG(LogShooterAugmentCard, Error, TEXT("AugmentStyle에 %s에 맞는 Style이 정의되지않았습니다."), *UEnum::GetValueAsString(InDefinition->Tier));
		return;
	}

	SetStyle(TierStyle->ButtonStyle);
	Text_TierLabel->SetText(TierStyle->TierDisplayName);
	
	BP_OnAccentChanged(TierStyle->AccentColor);

	if (IsValid(Image_AlertBadge))
	{
		Image_AlertBadge->SetVisibility(TierStyle->bShowAlertBadge
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	
	// 특수 애니메이션 진입 조건
	// 1. Anim_Highlight 할당
	// 2. 해당 TierStyle에서 bPlayHighlightIntro가 True
	if (Anim_Highlight != nullptr && TierStyle->bPlayHighlightIntro == true)
	{
		PlayAnimation(Anim_Highlight);
	}
}

void UShooterAugmentCard::PlayRerollFeedback()
{
	if (Anim_Reroll)
	{
		PlayAnimation(Anim_Reroll);
	}
}

void UShooterAugmentCard::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (OfferIndex == INDEX_NONE)
	{
		UE_LOG(LogShooterAugmentCard, Error, TEXT("동기화 되지않은 카드입니다."));
		return;
	}

	OnAugmentCardClicked.Broadcast(OfferIndex);
}
