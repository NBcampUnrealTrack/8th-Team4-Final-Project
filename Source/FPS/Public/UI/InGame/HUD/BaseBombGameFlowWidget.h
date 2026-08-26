#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "Actor/DataJackTypes.h"
#include "GameMode/RoundTypes.h"
#include "BaseBombGameFlowWidget.generated.h"

class ABaseTeamPlayerState;
class ABombGameState;
class ADataJack;
class UTextBlock;
class UWidget;

UENUM(BlueprintType)
enum class EBombResultType : uint8
{
	None UMETA(DisplayName = "None"),
	Round UMETA(DisplayName = "Round"),
	Match UMETA(DisplayName = "Match")
};

UENUM(BlueprintType)
enum class EBombMatchResult : uint8
{
	None UMETA(DisplayName = "None"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat")
};

UCLASS(Abstract, Blueprintable)
class FPS_API UBaseBombGameFlowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBaseBombGameFlowWidget(const FObjectInitializer& ObjectInitializer);
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	int32 GetCurrentRoundNumber() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	EGameFlowPhase GetGameFlowPhase() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	float GetRoundTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	FText GetRoundTimeText() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	float GetPhaseTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	FText GetPhaseTimeText() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	EGameTeam GetRoundWinner() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	ERoundEndReason GetRoundEndReason() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	EGameTeam GetMatchWinner() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Result")
	EBombResultType GetResultType() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Result")
	EGameTeam GetLocalPlayerTeam() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Result")
	EGameTeam GetWinningTeamForCurrentResult() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Result")
	EBombMatchResult GetLocalResult() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Result")
	bool ShouldShowResult() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Result")
	FText GetResultTitleText() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Result")
	FText GetResultText() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Result")
	ESlateVisibility GetResultVisibility() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	EBombMatchResult GetLocalMatchResult() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	bool ShouldShowMatchResult() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	FText GetMatchResultText() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	ESlateVisibility GetMatchResultVisibility() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Match")
	int32 GetMaxRounds() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetTeamAScore() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetTeamBScore() const;
	
	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetLocalTeamScore() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetEnemyTeamScore() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	ADataJack* GetActiveDataJack() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	EDataJackState GetDataJackState() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	float GetDataJackCountdownRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	FText GetDataJackCountdownText() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	float GetDataJackCountdownProgress() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	float GetDataJackInteractionTimeRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	FText GetDataJackInteractionTimeText() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	float GetDataJackInteractionProgress() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	FVector GetDataJackWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	FName GetPlantedSiteId() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool ShouldShowRoundTimer() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool ShouldShowDataJackCountdown() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool ShouldShowDataJackMinimapIcon() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool ShouldShowDataJackWorldPing() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool DoesLocalPlayerCarryDataJack() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool IsDataJackCarriedByLocalTeam() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	bool IsLocalPlayerInteractingWithDataJack() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Game Flow|UI")
	void BP_OnGameFlowChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Game Flow|UI")
	void BP_OnResultChanged(EBombResultType NewResultType, EBombMatchResult NewResult, const FText& TitleText, const FText& ResultText);

	UFUNCTION(BlueprintImplementableEvent, Category = "Game Flow|UI")
	void BP_OnMatchResultChanged(EBombMatchResult NewMatchResult, const FText& ResultText);

protected:
	UFUNCTION()
	void HandleRoundStateChanged();

	UFUNCTION()
	void HandleTeamScoreChanged(EGameTeam Team, int32 NewScore);

	UFUNCTION()
	void HandleActiveDataJackChanged(ADataJack* NewActiveDataJack);

	UFUNCTION()
	void HandleLocalPlayerTeamChanged(EGameTeam PreviousTeam, EGameTeam NewTeam);

	UFUNCTION()
	void HandleDataJackChanged();

private:
	void BindToReplicatedState();
	void BindToActiveDataJack(ADataJack* NewActiveDataJack);
	bool BindToLocalPlayerState(ABaseTeamPlayerState* NewLocalPlayerState);
	void UnbindFromGameState();
	void UnbindFromDataJack();
	void UnbindFromLocalPlayerState();
	FText FormatTime(float TimeSeconds) const;
	ABombGameState* GetBombGameState() const;
	ABaseTeamPlayerState* GetLocalTeamPlayerState() const;
	bool IsLocalPlayerAttacker() const;
	void NotifyGameFlowChanged();
	void RefreshResult();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Match Result", meta = (BindWidget))
	TObjectPtr<UWidget> Panel_MatchResult;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Match Result", meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ResultTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Match Result", meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_MatchResult;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Match Result")
	FText RoundEndTitleText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Match Result")
	FText MatchEndTitleText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Match Result")
	FText VictoryText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Match Result")
	FText DefeatText;

private:
	TWeakObjectPtr<ABombGameState> BoundGameState;
	TWeakObjectPtr<ADataJack> BoundDataJack;
	TWeakObjectPtr<ABaseTeamPlayerState> BoundLocalPlayerState;
	bool bHasInitializedBindings = false;
	EBombResultType CachedResultType = EBombResultType::None;
	EBombMatchResult CachedResult = EBombMatchResult::None;
	FTimerHandle StateBindingTimerHandle;
};
