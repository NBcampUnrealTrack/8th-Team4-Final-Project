// ShooterAugmentCard.cpp
#include "UI/Common/ShooterAugmentCard.h"
#include "UI/Common/ShooterAugmentTierStyleSet.h"
#include "Augment/AugmentDefinition.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/RetainerBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/WidgetAnimation.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAugmentCard, Log, All);

void UShooterAugmentCard::NativeOnHovered()
{
	Super::NativeOnHovered();
	
	if (Anim_HoverPulse)
	{
		PlayAnimation(Anim_HoverPulse);
	}
	BP_OnCardHoverChanged(true);
	OnAugmentCardHovered.Broadcast(OfferIndex, true);
}

void UShooterAugmentCard::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	
	BP_OnCardHoverChanged(false);
	OnAugmentCardHovered.Broadcast(OfferIndex, false);
}

void UShooterAugmentCard::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (OfferIndex == INDEX_NONE)
	{
		UE_LOG(LogShooterAugmentCard, Error, TEXT("AugmentCard Is not Valid"));
		return;
	}

	OnAugmentCardClicked.Broadcast(OfferIndex);
}

void UShooterAugmentCard::SetData(const UAugmentDefinition* InDefinition, int32 InOfferIndex)
{
	OfferIndex = InOfferIndex;

	if (IsValid(InDefinition) == false)
	{
		UE_LOG(LogShooterAugmentCard, Error, TEXT("%d번 카드에 증강 정의 누락"), InOfferIndex);
		return;
	}

	Text_Name->SetText(InDefinition->DisplayName);

	// Icon은 선택 화면이 UI 번들을 미리 올려둔 것을 꺼내 쓴다
	if (UTexture2D* Icon = InDefinition->Icon.Get())
	{
		Image_Icon->SetBrushFromTexture(Icon);
	}

	const FAugmentLevelData* LevelData = InDefinition->GetLevelData();
	if (LevelData == nullptr)
	{
		UE_LOG(LogShooterAugmentCard, Error, TEXT("%s 에 수치가 누락"), *InDefinition->GetName());
		return;
	}

	Text_Description->SetText(LevelData->Description);

	if (!ensure(TierStyles))
	{
		return;
	}

	const FShooterAugmentTierStyle* TierStyle = TierStyles->FindStyle(InDefinition->Tier);
	if (!ensureMsgf(TierStyle, TEXT("TierStyle에 %s 정의가 없음"), *UEnum::GetValueAsString(InDefinition->Tier)))
	{
		return;
	}

	SetStyle(TierStyle->ButtonStyle);
	Text_TierLabel->SetText(TierStyle->TierDisplayName);

	CachedAccentColor = TierStyle->AccentColor;
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

void UShooterAugmentCard::PlayRejectedFeedback()
{
	if (Anim_Rejected)
	{
		PlayAnimation(Anim_Rejected);
	}
	BP_OnCardRejected();
}

void UShooterAugmentCard::ResetFeedback()
{
	StopAllAnimations();

	if (IsValid(OldCardSnapshot))
	{
		OldCardSnapshot->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetRerollWipeProgress(0.f);

	BP_OnCardFeedbackReset();
}

void UShooterAugmentCard::BeginRerollPresentation()
{
	if (!IsValid(OldCardSnapshot))
	{
		return;
	}

	OldCardSnapshot->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 정의를 다시 해석하지 않고 지금 그려져 있는 것을 복사한다.
	if (Image_OldIcon && Image_Icon)
	{
		Image_OldIcon->SetBrush(Image_Icon->GetBrush());
	}
	if (Text_OldName && Text_Name)
	{
		Text_OldName->SetText(Text_Name->GetText());
	}
	if (Text_OldDescription && Text_Description)
	{
		Text_OldDescription->SetText(Text_Description->GetText());
	}
	if (Text_OldTierLabel && Text_TierLabel)
	{
		Text_OldTierLabel->SetText(Text_TierLabel->GetText());
	}

	// 0 = OldCardData가 New를 덮은 상태
	SetRerollWipeProgress(0.f);

	BP_OnRerollSnapshotTaken(CachedAccentColor);
}

void UShooterAugmentCard::SetRerollWipeProgress(const float Progress)
{
	UMaterialInstanceDynamic* MID = RetainerBox_RerollOld
		? RetainerBox_RerollOld->GetEffectMaterial()
		: nullptr;

	if (MID)
	{
		MID->SetScalarParameterValue(RerollWipeParameterName, Progress);
	}

	BP_OnRerollWipeProgress(Progress);
}

void UShooterAugmentCard::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ShooterAugmentCard_NativeTick);

	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Anim_Reroll || !IsAnimationPlaying(Anim_Reroll))
	{
		return;
	}

	// Anim_Reroll 의 시간에 진행률을 맞춤
	const float Start = Anim_Reroll->GetStartTime();
	const float Span = Anim_Reroll->GetEndTime() - Start;
	if (Span <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float Elapsed = GetAnimationCurrentTime(Anim_Reroll) - Start;
	SetRerollWipeProgress(FMath::Clamp(Elapsed / Span, 0.f, 1.f));
}

void UShooterAugmentCard::SetRejectedProgress(float Progress)
{
	UMaterialInstanceDynamic* MID = RetainerBox_Glitch
		? RetainerBox_Glitch->GetEffectMaterial()
		: nullptr;

	if (MID)
	{
		MID->SetScalarParameterValue(DissolveParameterName, Progress);
	}
	
	BP_OnRejectedProgress(Progress);
}