#include "UI/Killcam/KillcamWidget.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "Killcam/KillcamReplaySubsystem.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Interfaces/PlayerInterface.h"
#include "Player/ShooterPlayerState.h"
#include "Combat/CombatComponent.h"
#include "Tags/GameplayTags.h"
#include "Tags/UITags.h"
#include "UI/InGame/Weapons/ShooterScopeWidget.h"
#include "UI/InGame/Scoreboard/ShooterScoreboard.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Weapon/WeaponBase.h"

void UKillcamWidget::NativeConstruct()
{
	Super::NativeConstruct();
	DisplayedReticleTag = FGameplayTag();
	if (Image_Reticle)
	{
		Image_Reticle->SetVisibility(ESlateVisibility::Hidden);
	}
	if (Image_SniperScope)
	{
		Image_SniperScope->SetVisibility(ESlateVisibility::Hidden);
		SendSniperScopeToBack();
	}
	EnsureSkipHint();
	UpdateSkipHint();
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

UWidget* UKillcamWidget::ResolveHideTarget(UWidget* Widget)
{
	if (const UShooterPrimaryGameLayout* RootLayout = Cast<UShooterPrimaryGameLayout>(Widget))
	{
		return RootLayout->GetLayerContainer(UITags::UI_Layer_Game);
	}
	return Widget;
}

void UKillcamWidget::HideViewportWidget(UWidget* ViewportWidget, ULocalPlayer* Player)
{
	UWidget* Widget = ResolveHideTarget(ViewportWidget);
	if (!IsValid(Widget) || Widget == this || Widget->GetWorld() != GetWorld()
		|| (Player && Player != GetOwningLocalPlayer()) || HiddenWidgets.Contains(Widget)
		|| Widget->IsA<UShooterScopeWidget>())
	{
		return;
	}
	HiddenWidgets.Add(Widget, Widget->GetVisibility());
	Widget->SetVisibility(ESlateVisibility::Hidden);
}

void UKillcamWidget::RestoreViewportWidget(UWidget* ViewportWidget)
{
	UWidget* Widget = ResolveHideTarget(ViewportWidget);
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
	UpdateOverlayVisibility();
	UpdateReticle();
	UpdateSkipHint();
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

bool UKillcamWidget::IsScoreboardOrMenuActive() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	const UShooterPrimaryGameLayout* RootLayout = IsValid(UIManager) ? UIManager->GetRootLayout() : nullptr;
	if (!IsValid(RootLayout))
	{
		return false;
	}

	const UCommonActivatableWidgetContainerBase* GameMenuLayer = RootLayout->GetLayerContainer(UITags::UI_Layer_GameMenu);
	if (IsValid(GameMenuLayer) && Cast<UShooterScoreboard>(GameMenuLayer->GetActiveWidget()))
	{
		return true;
	}

	const FGameplayTag MenuLayerTags[] = { UITags::UI_Layer_Menu, UITags::UI_Layer_Modal };
	for (const FGameplayTag& LayerTag : MenuLayerTags)
	{
		const UCommonActivatableWidgetContainerBase* Layer = RootLayout->GetLayerContainer(LayerTag);
		if (IsValid(Layer) && IsValid(Layer->GetActiveWidget()))
		{
			return true;
		}
	}
	return false;
}

void UKillcamWidget::UpdateOverlayVisibility()
{
	const float DesiredOpacity = IsScoreboardOrMenuActive() ? 0.0f : 1.0f;
	if (!FMath::IsNearlyEqual(GetRenderOpacity(), DesiredOpacity))
	{
		SetRenderOpacity(DesiredOpacity);
	}
}

void UKillcamWidget::EnsureSkipHint()
{
	if (!Text_SkipHint)
	{
		UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget());
		if (!IsValid(RootCanvas) || !WidgetTree)
		{
			return;
		}

		Text_SkipHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_SkipHint"));
		FSlateFontInfo Font = Text_WeaponName->GetFont();
		Font.Size = 16;
		Text_SkipHint->SetFont(Font);
		Text_SkipHint->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f)));
		Text_SkipHint->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text_SkipHint->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Text_SkipHint->SetJustification(ETextJustify::Center);

		if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Text_SkipHint))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetPosition(FVector2D(0.0f, -240.0f));
			CanvasSlot->SetZOrder(10);
		}
	}

	Text_SkipHint->SetText(SkipHintText);
}

void UKillcamWidget::SendSniperScopeToBack()
{
	// 스코프 오버레이는 화면 전체를 덮으므로 킬러/피해자 이름, 체력바, 스킵 안내 등 킬캠 HUD 아래에 그린다.
	UPanelWidget* Parent = Image_SniperScope->GetParent();
	if (!IsValid(Parent))
	{
		return;
	}
	Parent->ShiftChild(0, Image_SniperScope);

	if (UCanvasPanelSlot* ScopeSlot = Cast<UCanvasPanelSlot>(Image_SniperScope->Slot))
	{
		// 캔버스는 ZOrder가 인덱스보다 우선하므로, 형제 중 최소 ZOrder 이하로 맞춰 맨 뒤를 보장한다.
		int32 MinZOrder = 0;
		for (UPanelSlot* SiblingSlot : Parent->GetSlots())
		{
			const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SiblingSlot);
			if (CanvasSlot && CanvasSlot != ScopeSlot)
			{
				MinZOrder = FMath::Min(MinZOrder, CanvasSlot->GetZOrder());
			}
		}
		ScopeSlot->SetZOrder(MinZOrder - 1);
	}
}

void UKillcamWidget::UpdateSkipHint()
{
	if (!Text_SkipHint)
	{
		return;
	}

	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UKillcamReplaySubsystem* Killcam = IsValid(LocalPlayer)
		? LocalPlayer->GetSubsystem<UKillcamReplaySubsystem>()
		: nullptr;
	const ESlateVisibility DesiredVisibility = IsValid(Killcam) && Killcam->CanSkipPlayback()
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Hidden;
	if (Text_SkipHint->GetVisibility() != DesiredVisibility)
	{
		Text_SkipHint->SetVisibility(DesiredVisibility);
	}
}

void UKillcamWidget::UpdateReticle()
{
	FGameplayTag WeaponTag;
	bool bShowSniperScope = false;
	APawn* Killer = ReplayKiller.Get();
	if (IsValid(Killer) && Killer->Implements<UPlayerInterface>())
	{
		UCombatComponent* Combat = IPlayerInterface::Execute_GetCombatComponent(Killer);
		if (AWeaponBase* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(Killer); IsValid(Weapon))
		{
			WeaponTag = Weapon->GetWeaponTag();
			bShowSniperScope = IsValid(Combat)
				&& Combat->IsAiming()
				&& WeaponTag == RogueTags::Weapon_Type_Sniper;
		}
	}

	if (DisplayedReticleTag == WeaponTag && bDisplayedSniperScope == bShowSniperScope)
	{
		return;
	}
	DisplayedReticleTag = WeaponTag;
	bDisplayedSniperScope = bShowSniperScope;

	if (Image_SniperScope)
	{
		if (bShowSniperScope && SniperScopeBrush.GetResourceObject())
		{
			Image_SniperScope->SetBrush(SniperScopeBrush);
			Image_SniperScope->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_SniperScope->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (!Image_Reticle)
	{
		return;
	}

	if (bShowSniperScope)
	{
		Image_Reticle->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

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
