// DominationAnnouncementBanner.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameMode/Types/DominationTypes.h"
#include "TimerManager.h"
#include "UI/InGame/HUD/DominationAnnouncementDetector.h"
#include "DominationAnnouncementBanner.generated.h"

class ADominationGameState;
class UImage;
class UTextBlock;
class UWidgetAnimation;

// 점령전 사건 발생 시 문구
UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UDominationAnnouncementBanner : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UDominationAnnouncementBanner(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleObjectiveFlowStateChanged();

	UFUNCTION()
	void HandleCaptureStateChanged(FDominationCaptureState CaptureState);

	void EvaluateGameState();
	void PresentAnnouncement(const FDominationAnnouncement& Announcement);
	void HandleDisplayExpired();

	static FDominationAnnouncementSnapshot BuildSnapshot(const ADominationGameState& GameState);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Accent;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Prefix;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Point;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Result;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_Show;

	UPROPERTY(EditDefaultsOnly, Category = "Announcement|Text")
	FText MatchStartText;
	UPROPERTY(EditDefaultsOnly, Category = "Announcement|Text")
	FText ObjectivePrefixText;
	UPROPERTY(EditDefaultsOnly, Category = "Announcement|Text")
	FText CaptureStartedAllyText;
	UPROPERTY(EditDefaultsOnly, Category = "Announcement|Text")
	FText CaptureStartedEnemyText;
	UPROPERTY(EditDefaultsOnly, Category = "Announcement|Text")
	FText CaptureCompletedAllyText;
	UPROPERTY(EditDefaultsOnly, Category = "Announcement|Text")
	FText CaptureCompletedEnemyText;

	UPROPERTY(EditDefaultsOnly, Category = "Announcement|Color")
	FLinearColor NeutralAccentColor = FLinearColor(0.93f, 0.89f, 0.80f);

	FDominationAnnouncementDetector Detector;
	TWeakObjectPtr<ADominationGameState> BoundGameState;
	FTimerHandle DisplayTimer;
};
