#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/SurrenderVoteTypes.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterSurrenderVoteScreen.generated.h"

class UCommonButtonBase;
class UPanelWidget;
class UShooterSurrenderVoterRow;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSurrenderVoteSubmitted, bool);
DECLARE_MULTICAST_DELEGATE(FOnSurrenderMenuRequested);

/** 팀 투표 스냅샷을 표시하고 로컬 플레이어의 한 표만 전달한다. */
UCLASS(Abstract)
class FPS_API UShooterSurrenderVoteScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UShooterSurrenderVoteScreen();

	void SetVoteState(const FSurrenderVoteState& NewVoteState);

	FOnSurrenderVoteSubmitted OnVoteSubmitted;
	FOnSurrenderMenuRequested OnMenuRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;

private:
	void HandleApproveClicked();
	void HandleRejectClicked();
	void RebuildVoterRows();
	void RefreshVoterRows();
	void RefreshApprovalSummary();
	void RefreshRemainingTime();
	bool HasSameVoterRoster(const FSurrenderVoteState& NewVoteState) const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_Voters;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_ApprovalSummary;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_RemainingTime;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_Approve;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_Reject;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Surrender")
	TSubclassOf<UShooterSurrenderVoterRow> VoterRowClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterSurrenderVoterRow>> VoterRows;

	FSurrenderVoteState VoteState;
	TArray<FString> VoterNames;
	FTimerHandle RemainingTimeTimerHandle;
};
