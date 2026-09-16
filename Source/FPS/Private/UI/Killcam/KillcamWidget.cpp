#include "UI/Killcam/KillcamWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Player/ShooterPlayerState.h"

void UKillcamWidget::SetReplay(const FShooterEliminationEvent& Event, APawn* Killer, APawn* Victim)
{
	ReplayKiller = Killer;
	ReplayVictim = Victim;
	Text_KillerHealth->SetText(FText::FromString(TEXT("--")));
	Text_VictimHealth->SetText(FText::FromString(TEXT("--")));
	Progress_KillerHealth->SetPercent(0.0f);
	Progress_VictimHealth->SetPercent(0.0f);
	if (IsValid(Event.KillerPlayerState))
	{
		Text_KillerName->SetText(FText::FromString(Event.KillerPlayerState->GetPlayerName()));
	}
	if (IsValid(Event.VictimPlayerState))
	{
		Text_VictimName->SetText(FText::FromString(Event.VictimPlayerState->GetPlayerName()));
	}
	Text_WeaponName->SetText(FText::FromName(Event.WeaponTag.GetTagLeafName()));
	Image_Weapon->SetVisibility(ESlateVisibility::Hidden);
	for (const FDominationCombatClassDefinition& Definition : GetDefault<UDominationClassSettings>()->CombatClasses)
	{
		if (Definition.PrimaryWeaponTag == Event.WeaponTag)
		{
			if (UTexture2D* Texture = Definition.WeaponArt.LoadSynchronous())
			{
				Image_Weapon->SetBrushFromTexture(Texture, false);
				Image_Weapon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			break;
		}
	}
	UpdateHealth(ReplayKiller.Get(), Text_KillerHealth, Progress_KillerHealth);
	UpdateHealth(ReplayVictim.Get(), Text_VictimHealth, Progress_VictimHealth);
}

void UKillcamWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateHealth(ReplayKiller.Get(), Text_KillerHealth, Progress_KillerHealth);
	UpdateHealth(ReplayVictim.Get(), Text_VictimHealth, Progress_VictimHealth);
}

void UKillcamWidget::UpdateHealth(APawn* Pawn, UTextBlock* Text, UProgressBar* Bar) const
{
	if (!IsValid(Pawn))
	{
		return;
	}
	const AShooterPlayerState* PlayerState = Pawn->GetPlayerState<AShooterPlayerState>();
	if (!IsValid(PlayerState) || !IsValid(PlayerState->GetVitalSet()))
	{
		Text->SetText(FText::FromString(TEXT("--")));
		Bar->SetPercent(0.0f);
		return;
	}
	const URogueVitalAttributeSet* Vitals = PlayerState->GetVitalSet();
	const float Maximum = Vitals->GetMaxHealth();
	const float Health = FMath::Clamp(Vitals->GetHealth(), 0.0f, FMath::Max(0.0f, Maximum));
	float Percent = 0.0f;
	if (Maximum > 0.0f)
	{
		Percent = Health / Maximum;
	}
	Bar->SetPercent(Percent);
	Text->SetText(FText::Format(NSLOCTEXT("Killcam", "Health", "{0} / {1}"),
		FText::AsNumber(FMath::CeilToInt(Health)), FText::AsNumber(FMath::CeilToInt(Maximum))));
}
