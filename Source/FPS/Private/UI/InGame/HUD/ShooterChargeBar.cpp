// ShooterChargeBar.cpp
#include "UI/InGame/HUD/ShooterChargeBar.h"
#include "Components/Widget.h"

void UShooterChargeBar::SetFilled(bool bFilled)
{
	if (!Image_Fill)
	{
		return;
	}
	// Collapsed를 사용하면 레이아웃에서 빠져 빈 칸 크기가 틀어질 수 있음
	Image_Fill->SetVisibility(bFilled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
