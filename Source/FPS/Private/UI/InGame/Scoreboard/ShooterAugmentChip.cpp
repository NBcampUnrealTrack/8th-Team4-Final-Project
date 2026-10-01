// ShooterAugmentChip.cpp
#include "UI/InGame/Scoreboard/ShooterAugmentChip.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"


void UShooterAugmentChip::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	
	if (bHasData)
	{
		OnChipHovered.Broadcast(CachedData, this);
	}
}

void UShooterAugmentChip::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	
	OnChipUnHovered.Broadcast(this);
}

void UShooterAugmentChip::SetData(const FShooterAugmentDisplay& InData)
{
	CachedData = InData;
	bHasData = true;

	if (Text_AugmentName)
	{
		Text_AugmentName->SetText(InData.DisplayName);
		Text_AugmentName->SetColorAndOpacity(InData.TierAccentColor);
	}
	
	if (Image_Icon)
	{
		// 스코어보드가 이미지 로드를 미리 걸어두고 이미 있으면 쓴다.
		// 아직 없으면 다음 갱신에 채워짐
		if (UTexture2D* Texture = InData.Icon.Get())
		{
			Image_Icon->SetBrushFromTexture(Texture);
			Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Icon->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	
	// 마우스를 올려둔 채로 내용이 바뀌는경우(증강 획득/교체)가 있기때문에
	// MouseEnter는 다시 안 오므로 여기서 직접 갱신
	if (IsHovered())
	{
		OnChipHovered.Broadcast(CachedData, this);
	}
}

void UShooterAugmentChip::SetEmpty()
{
	CachedData = FShooterAugmentDisplay();
	bHasData = false;
	
	// 빈 칸은 프레임만 남긴다. 프레임은 WBP 배경이라 손댈 게 없고
	// 아이콘/텍스트만 비우면 된다
	if (Text_AugmentName)
	{
		Text_AugmentName->SetText(FText::GetEmpty());
	}

	if (Image_Icon)
	{
		// Collapsed가 아니라 Hidden. 8칸 고정이라 칸이 줄면 행마다 폭이 달라짐
		Image_Icon->SetVisibility(ESlateVisibility::Hidden);
	}
	
	// 마우스를 올려둔 채로 비워진 경우 툴팁도 닫음
	if (IsHovered())
	{
		OnChipUnHovered.Broadcast(this);
	}
}
