// ShooterMatchResultScreen.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultTypes.h"
#include "ShooterMatchResultScreen.generated.h"

class UCommonTextBlock;
class UPanelWidget;
class UButton;
class UWidgetSwitcher;
class UShooterMatchResultRowWidget;

// 닫아 달라는 요청으로 실제 제거는 서브시스템이 함
DECLARE_MULTICAST_DELEGATE(FOnMatchResultCloseRequested);

/**
 * 매치 결과 화면. 로비 위에 덮인다.
 */
UCLASS(Abstract)
class FPS_API UShooterMatchResultScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	// 왼쪽에 내 팀, 오른쪽에 상대 팀으로 승패와 무관하게 위치는 고정
	void SetResultData(const FShooterMatchResultSnapshot& InSnapshot);

	FOnMatchResultCloseRequested OnCloseRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleCloseClicked();

	// 승패로 갈리는 유일한 부분. 나머지는 전부 공통이다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> Switcher_OutcomeVisual;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Victory;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Panel_Defeat;

	// 왼쪽 = 내팀
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_MyTeamScore;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_MyTeamRows;

	// 오른쪽 = 상대팀
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_EnemyTeamScore;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_EnemyTeamRows;

	// 승자 쪽에만 활성화
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Highlight_MyTeam;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Highlight_EnemyTeam;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Close;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UShooterMatchResultRowWidget> RowWidgetClass;

	// bStatsAvailable == false이면 Hidden
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_StatsHeader;
};