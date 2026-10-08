#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Styling/SlateBrush.h"
#include "KillcamWidget.generated.h"

class APawn;
class UTextBlock;
class UProgressBar;
class UImage;
struct FShooterEliminationEvent;

UCLASS()
class FPS_API UKillcamWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetReplay(const FShooterEliminationEvent& Event, APawn* Killer, APawn* Victim);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void UpdateHealth(APawn* Pawn, UTextBlock* Text, UProgressBar* Bar) const;
	void UpdateReticle();
	bool IsScoreboardOrMenuActive() const;
	void UpdateOverlayVisibility();
	void EnsureSkipHint();
	void UpdateSkipHint();
	static UWidget* ResolveHideTarget(UWidget* Widget);
	void HideViewportWidget(UWidget* ViewportWidget, ULocalPlayer* Player);
	void RestoreViewportWidget(UWidget* ViewportWidget);
	TMap<TWeakObjectPtr<UWidget>, ESlateVisibility> HiddenWidgets;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_VictimName;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_KillerName;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_WeaponName;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_VictimHealth;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_KillerHealth;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Progress_VictimHealth;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Progress_KillerHealth;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Weapon;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Reticle;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SniperScope;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SkipHint;

	UPROPERTY(EditDefaultsOnly, Category = "Killcam|Skip")
	FText SkipHintText = NSLOCTEXT("Killcam", "SkipHint", "CLICK TO SKIP");

	UPROPERTY(EditDefaultsOnly, Category = "Killcam|Reticle")
	TMap<FGameplayTag, FSlateBrush> WeaponReticles;

	UPROPERTY(EditDefaultsOnly, Category = "Killcam|Reticle")
	FSlateBrush SniperScopeBrush;

	FGameplayTag DisplayedReticleTag;
	bool bDisplayedSniperScope = false;

	TWeakObjectPtr<APawn> ReplayKiller;
	TWeakObjectPtr<APawn> ReplayVictim;
};
