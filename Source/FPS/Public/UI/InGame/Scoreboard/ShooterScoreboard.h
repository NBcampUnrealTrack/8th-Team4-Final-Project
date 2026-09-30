// ShooterScoreboard.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "UI/InGame/Scoreboard/ShooterScoreboardEntry.h"
#include "ShooterScoreboard.generated.h"

class UShooterAugmentTierStyleSet;
class APlayerState;
class UCanvasPanel;
class UPanelWidget;
class UShooterScoreboardAugmentTooltip;
class UShooterWeaponDisplaySet;
struct FStreamableHandle;

UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UShooterScoreboard : public UShooterActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_MyTeamRows;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> Box_EnemyTeamRows;
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard")
	TSubclassOf<UShooterScoreboardEntry> RowWidgetClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> Canvas_TooltipLayer;
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard|Tooltip")
	TSubclassOf<UShooterScoreboardAugmentTooltip> TooltipClass;

	// 등급 스타일셋은 이 화면이 유일하게 소유
	// 칩·툴팁은 안 들고, ResolveAugments 가 풀어서 구조체에 넣어 넘긴다
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard")
	TObjectPtr<UShooterAugmentTierStyleSet> TierStyles;
	// 스타일 조회가 실패했을 때 쓸 색
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard")
	FLinearColor TierFallbackColor = FLinearColor::White;

	// 칩과 툴팁 사이 간격, 뷰포트 가장자리 여백
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard|Tooltip")
	float TooltipGap = 8.f;
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard|Tooltip")
	float TooltipViewportPadding = 12.f;
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard")
	TObjectPtr<UShooterWeaponDisplaySet> WeaponDisplaySet;
	
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard", meta = (ClampMin = "0.05"))
	float RefreshInterval = 0.25f;
	
private:
	void RefreshAll();
	
	void ApplyRows(const TArray<APlayerState*>& Players, UPanelWidget* Box, EShooterResultRowSide Side);
	// 팀을 모르면 박스 숨김
	void SetTeamBoxesVisible(bool bVisible);
	
	void ShowAugmentTooltip(const FShooterAugmentDisplay& Data, UShooterAugmentChip* Chip);
	void HideAugmentTooltip(UShooterAugmentChip* Chip);
	
	// 주인을 따지지 않고 강제로 닫음
	// 화면 비활성, 팀 미확정, 등 호버 중인 칩의 Row가 통쨰로 제거되는 경우 사용
	void ClearAugmentTooltip();
	
	FShooterScoreboardRowData BuildRowData(APlayerState* PlayerState);
	FText ResolveWeaponText(APlayerState* PlayerState) const;
	TArray<FShooterAugmentDisplay> ResolveAugments(APlayerState* PlayerState);
	
	void EnsureAugmentIconsLoaded(const TArray<FAugmentInventoryEntry>& Entries);
	
	UPROPERTY(Transient)
	TObjectPtr<UShooterScoreboardAugmentTooltip> TooltipInstance;
	
	TWeakObjectPtr<UShooterAugmentChip> HoveredChip;
	
	// 입퇴장 처리를 하려면 캐시가 필요함
	TMap<TObjectKey<APlayerState>, TWeakObjectPtr<UShooterScoreboardEntry>> RowCache;
	
	FTimerHandle RefreshTimerHandle;
	
	TSharedPtr<FStreamableHandle> IconLoadHandle;
	TArray<FPrimaryAssetId> LoadedIconIds;
	
	static constexpr bool bStatsAvailable = true;
};
