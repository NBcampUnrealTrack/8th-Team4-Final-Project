#include "UI/Killcam/KillcamWidget.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Interfaces/PlayerInterface.h"
#include "Player/ShooterPlayerState.h"
#include "UI/InGame/Weapons/ShooterScopeWidget.h"
#include "Weapon/WeaponBase.h"

void UKillcamWidget::NativeConstruct()
{
	Super::NativeConstruct();
	DisplayedReticleTag = FGameplayTag();
	if (Image_Reticle)
	{
		Image_Reticle->SetVisibility(ESlateVisibility::Hidden);
	}
	if (UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get(GetWorld()))
	{
		Viewport->OnWidgetAdded.AddUObject(this, &ThisClass::HideViewportWidget);
		Viewport->OnWidgetRemoved.AddUObject(this, &ThisClass::RestoreViewportWidget);
	}
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* Widget : Widgets)
	{
		if (IsValid(Widget) && Widget->IsInViewport())
		{
			HideViewportWidget(Widget, Widget->GetOwningLocalPlayer());
		}
	}
}

void UKillcamWidget::NativeDestruct()
{
	if (UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get(GetWorld()))
	{
		Viewport->OnWidgetAdded.RemoveAll(this);
		Viewport->OnWidgetRemoved.RemoveAll(this);
	}
	for (const auto& Entry : HiddenWidgets)
	{
		if (UWidget* Widget = Entry.Key.Get())
		{
			if (Widget->GetVisibility() == ESlateVisibility::Hidden)
			{
				Widget->SetVisibility(Entry.Value);
			}
		}
	}
	HiddenWidgets.Reset();
	Super::NativeDestruct();
}

void UKillcamWidget::HideViewportWidget(UWidget* Widget, ULocalPlayer* Player)
{
	if (!IsValid(Widget) || Widget == this || Widget->GetWorld() != GetWorld()
		|| (Player && Player != GetOwningLocalPlayer()) || HiddenWidgets.Contains(Widget)
		|| Widget->IsA<UShooterScopeWidget>())
	{
		return;
	}
	HiddenWidgets.Add(Widget, Widget->GetVisibility());
	Widget->SetVisibility(ESlateVisibility::Hidden);
}

void UKillcamWidget::RestoreViewportWidget(UWidget* Widget)
{
	if (IsValid(Widget))
	{
		if (const ESlateVisibility* SavedVisibility = HiddenWidgets.Find(Widget))
		{
			if (Widget->GetVisibility() == ESlateVisibility::Hidden)
			{
				Widget->SetVisibility(*SavedVisibility);
			}
			HiddenWidgets.Remove(Widget);
		}
	}
}

void UKillcamWidget::SetReplay(const FShooterEliminationEvent& Event, APawn* Killer, APawn* Victim)
{
	ReplayKiller = Killer;
	ReplayVictim = Victim;
	UpdateReticle();
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
	UpdateReticle();
	for (auto It = HiddenWidgets.CreateIterator(); It; ++It)
	{
		UWidget* Widget = It.Key().Get();
		if (!IsValid(Widget))
		{
			It.RemoveCurrent();
			continue;
		}
		if (Widget->GetVisibility() != ESlateVisibility::Hidden)
		{
			It.Value() = Widget->GetVisibility();
			Widget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	UpdateHealth(ReplayKiller.Get(), Text_KillerHealth, Progress_KillerHealth);
	UpdateHealth(ReplayVictim.Get(), Text_VictimHealth, Progress_VictimHealth);
}

void UKillcamWidget::UpdateReticle()
{
	if (!Image_Reticle)
	{
		return;
	}

	FGameplayTag WeaponTag;
	APawn* Killer = ReplayKiller.Get();
	if (IsValid(Killer) && Killer->Implements<UPlayerInterface>())
	{
		if (AWeaponBase* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(Killer); IsValid(Weapon))
		{
			WeaponTag = Weapon->GetWeaponTag();
		}
	}

	if (DisplayedReticleTag == WeaponTag)
	{
		return;
	}
	DisplayedReticleTag = WeaponTag;
	const FSlateBrush* Brush = WeaponReticles.Find(WeaponTag);
	if (!Brush || !Brush->GetResourceObject())
	{
		Image_Reticle->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	Image_Reticle->SetBrush(*Brush);
	Image_Reticle->SetVisibility(ESlateVisibility::HitTestInvisible);
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
