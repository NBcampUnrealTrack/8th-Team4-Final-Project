// ShooterMatchResultRowWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultTypes.h"
#include "ShooterMatchResultRowWidget.generated.h"

class UCommonTextBlock;

/**
 * 결과 화면의 플레이어 한 줄로 표시만 한다.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UShooterMatchResultRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// bStatsAvailable이 false 면 K/D/A Collum을 Collapsed 로 둔다
	void SetRowData(
		const FShooterMatchResultRow& InRow, 
		bool bStatsAvailable, 
		EShooterResultRowSide Side);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_PlayerName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Kills;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Deaths;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Assists;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Stats;
	
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Team")
	void BP_ApplyTeamStyle(EShooterResultRowSide Side);
};