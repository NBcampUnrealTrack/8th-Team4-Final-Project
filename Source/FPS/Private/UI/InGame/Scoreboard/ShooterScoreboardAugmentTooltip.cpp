// ShooterScoreboardAugmentTooltip.cpp
#include "UI/InGame/Scoreboard/ShooterScoreboardAugmentTooltip.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"

void UShooterScoreboardAugmentTooltip::SetData(const FShooterAugmentDisplay& InData)
{
	if (Text_Name)
	{
		Text_Name->SetText(InData.DisplayName);
	}
	if (Text_Description)
	{
		Text_Description->SetText(InData.Description);
	}
	
	if (Text_Tier)
	{
		// 스타일셋을 여기서 조회하지 않는다. 스코어보드가 이미 풀어서 넣어줬다
		Text_Tier->SetText(InData.TierDisplayName);
		Text_Tier->SetColorAndOpacity(InData.TierAccentColor);
	}
	
	if (Image_Icon == nullptr)
	{
		return;
	}
	
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
