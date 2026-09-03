// ShooterScoreboardEntry.h 
#pragma once

#include "CoreMinimal.h"
#include "UI/InGame/Scoreboard/ShooterAugmentChip.h" 
#include "UI/OutGame/MatchResult/ShooterMatchResultTypes.h"
#include "Blueprint/UserWidget.h"
#include "ShooterScoreboardEntry.generated.h"

class UCommonTextBlock;
class UPanelWidget;

USTRUCT(BlueprintType)
struct FShooterScoreboardRowData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	FText PlayerName;
	UPROPERTY(BlueprintReadOnly)
	int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly)
	int32 Deaths = 0;
	UPROPERTY(BlueprintReadOnly)
	int32 Assists = 0;
	// 매핑에 없으면 빈 텍스트 출력
	UPROPERTY(BlueprintReadOnly)
	FText WeaponName;
	UPROPERTY(BlueprintReadOnly)
	TArray<FShooterAugmentDisplay> Augments;
	UPROPERTY(BlueprintReadOnly)
	int32 PingMS = 0;
	UPROPERTY(BlueprintReadOnly)
	bool bIsAlive = true;
};

UCLASS(Abstract)
class FPS_API UShooterScoreboardEntry : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetData(
		const FShooterScoreboardRowData& InData, bool bStatsAvailable, EShooterResultRowSide Side);
	
	FOnAugmentChipHovered OnAugmentHovered;
	FOnAugmentChipUnHovered OnAugmentUnHovered;
	
	// 이 Row가 그 칩을 갖고있나 체크
	bool OwnsAugmentChip(const UShooterAugmentChip* Chip) const;
	
protected:protected:
	virtual void NativeOnInitialized() override;

	// 결과 화면과 같은 방식으로 팀 색을 WBP 에 넘긴다
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Team")
	void BP_ApplyTeamStyle(EShooterResultRowSide Side);
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_PlayerName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Weapon;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Ping;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Kills;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Deaths;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Assists;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_Stats;
	// 사망 표시
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Icon_Dead;
	
	// 5개 고정이고 간격 크기는 디자인할때 맞추는게 좋기때문에 정적생성함
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UShooterAugmentChip> Chip_Augment0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UShooterAugmentChip> Chip_Augment1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UShooterAugmentChip> Chip_Augment2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UShooterAugmentChip> Chip_Augment3;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UShooterAugmentChip> Chip_Augment4;
	
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard|State",
	meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeadRowOpacity = 0.45f;

private:
	// NativeOnInitialized 에서 5개를 한 번만 만들고 이후 재사용한다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterAugmentChip>> AugmentChips;

	// 팀 색은 바뀔 때만 다시 칠한다. 
	TOptional<EShooterResultRowSide> CachedSide;
};