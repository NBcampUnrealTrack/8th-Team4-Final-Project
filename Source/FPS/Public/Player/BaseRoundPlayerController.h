#pragma once

#include "CoreMinimal.h"
#include "GameMode/SurrenderVoteTypes.h"
#include "Player/ShooterPlayerController.h"
#include "BaseRoundPlayerController.generated.h"

class UInputMappingContext;
class UShooterUIInitComponent;
class USurrenderVoteManager;

DECLARE_MULTICAST_DELEGATE(FOnSurrenderVoteStateChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSurrenderVoteStartResult, ESurrenderVoteStartResult);

UCLASS(Abstract)
class FPS_API ABaseRoundPlayerController : public AShooterPlayerController
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	bool IsRoundInputEnabled() const;

	virtual bool CanProcessGameplayInput() const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	virtual bool CanUseCombatInput() const;

	void SetRoundInputEnabled(bool bEnabled);

	// 로컬 HUD와 인게임 입력 계층이 준비된 뒤 서버의 첫 라운드 게이트를 연다.
	UFUNCTION(BlueprintCallable, Category = "Game Flow|Round")
	void NotifyClientReadyForGameFlow();

	const FSurrenderVoteState& GetSurrenderVoteState() const;
	void RequestStartSurrenderVote();
	void SubmitSurrenderVote(bool bApprove);

	FOnSurrenderVoteStateChanged OnSurrenderVoteStateChanged;
	FOnSurrenderVoteStartResult OnSurrenderVoteStartResult;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void RefreshLoadingScreenGameStateBinding(
		ULoadingScreenSubsystem* LoadingScreenSubsystem) override;

	UFUNCTION(Client, Reliable)
	void ClientSetRoundInputEnabled(bool bEnabled);

	UFUNCTION(Server, Reliable)
	void ServerNotifyClientReadyForGameFlow();

	UFUNCTION(Server, Reliable)
	void ServerRequestStartSurrenderVote();

	UFUNCTION(Server, Reliable)
	void ServerSubmitSurrenderVote(int32 VoteId, bool bApprove);

	UFUNCTION(Client, Reliable)
	void ClientNotifySurrenderVoteStartResult(ESurrenderVoteStartResult Result);

private:
	friend class USurrenderVoteManager;

	UFUNCTION()
	void OnRep_SurrenderVoteState();

	void SetSurrenderVoteState(const FSurrenderVoteState& NewState);

	void Input_ToggleShop();
	void Input_ToggleInGameMenu();
	UShooterUIInitComponent* GetUIInitComponent() const;

	void ApplyRoundInputEnabled(bool bEnabled);
	void SetRoundInputLockApplied(bool bApplied);
	void SetGameplayMappingContextEnabled(bool bEnabled);
	void SetSystemMappingContextEnabled(bool bEnabled);
	void SetMappingContextEnabled(
		UInputMappingContext* MappingContext,
		int32 Priority,
		bool bEnabled);

	bool bIsRoundInputEnabled = true;
	bool bIsRoundInputLockApplied = false;

	UPROPERTY(ReplicatedUsing = OnRep_SurrenderVoteState)
	FSurrenderVoteState SurrenderVoteState;
};
