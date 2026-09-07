#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/SurrenderVoteTypes.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterSurrenderVoteScreen.generated.h"

class UBorder;
class UPanelWidget;
class UShooterSurrenderVoterRow;
class UTextBlock;
class UWidget;

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
	UFUNCTION()
	void HandleApproveClicked();

	UFUNCTION()
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

	// 기존 CommonUI WBP와 자동 생성 UMG WBP를 모두 허용한다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Btn_Approve;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> Btn_Reject;

	// 기존 수동 WBP와의 에셋 호환을 위해 필드는 유지한다.
	// 현재 투표자 행은 이 화면이 직접 구성하므로 런타임 생성에 사용하지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Surrender")
	TSubclassOf<UShooterSurrenderVoterRow> VoterRowClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> VoterNameTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> VoterStatusTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> VoterStatusBadges;

	FSurrenderVoteState VoteState;
	TArray<FString> VoterNames;
	FTimerHandle RemainingTimeTimerHandle;
};
