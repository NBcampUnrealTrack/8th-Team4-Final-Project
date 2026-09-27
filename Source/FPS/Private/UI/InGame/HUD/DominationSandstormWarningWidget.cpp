#include "UI/InGame/HUD/DominationSandstormWarningWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UDominationSandstormWarningWidget::SetWarning(const float SecondsRemaining, const float WarningDuration)
{
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(SecondsRemaining));
	if (CountdownText && DisplayedSeconds != Seconds)
	{
		DisplayedSeconds = Seconds;
		CountdownText->SetText(FText::AsNumber(Seconds));
	}
	if (CountdownProgress)
	{
		CountdownProgress->SetPercent(WarningDuration > 0.0f
			? FMath::Clamp(SecondsRemaining / WarningDuration, 0.0f, 1.0f)
			: 0.0f);
	}
}
