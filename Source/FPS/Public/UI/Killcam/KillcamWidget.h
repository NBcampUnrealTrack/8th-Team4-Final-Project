#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void UpdateHealth(APawn* Pawn, UTextBlock* Text, UProgressBar* Bar) const;

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

	TWeakObjectPtr<APawn> ReplayKiller;
	TWeakObjectPtr<APawn> ReplayVictim;
};
