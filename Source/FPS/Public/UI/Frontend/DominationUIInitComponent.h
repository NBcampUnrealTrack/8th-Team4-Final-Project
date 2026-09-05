#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameplayTagContainer.h"
#include "Player/PlayerParticipationState.h"
#include "UI/Frontend/TeamMatchUIInitComponent.h"
#include "DominationUIInitComponent.generated.h"

class ADominationGameState;
class ADominationPlayerState;
class UDominationClassSelectScreen;
class UShooterAugmentSelectScreen;
class UShooterInGameMenuScreen;
class UShooterPlayerHUD;
class UShooterPrimaryGameLayout;
class UShooterSurrenderVoteScreen;

// 증강 보류 버튼
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentDismissStateChanged);

/**
 * Domination의 전투 HUD 준비와 매치 페이즈별 공통 화면 허용 여부를 담당한다.
 */
UCLASS(ClassGroup = (UI))
class FPS_API UDominationUIInitComponent : public UTeamMatchUIInitComponent
{
	GENERATED_BODY()

public:
	UDominationUIInitComponent();
	
	// B키를 눌러 선택 보류
	UFUNCTION(BlueprintCallable, Category = "Domination|UI")
	void ToggleAugmentSelect();
	UFUNCTION(BlueprintPure, Category = "Domination|UI")
	bool IsAugmentSelectDismissed() const;
	UPROPERTY(BlueprintAssignable, Category = "Domination|UI")
	FOnAugmentDismissStateChanged OnAugmentDismissStateChanged;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsAugmentPresentationAllowed() const override;
	virtual bool IsSurrenderRequestAllowed() const override;

	virtual void OnAugmentScreenCreated(UShooterAugmentSelectScreen* Screen) override;
	virtual void OnAugmentOffersPresented(const TArray<FAugmentInventoryEntry>& InOffers) override;
	virtual EAugmentSelectEntry GetAugmentEntryAnimation() const override;
	
	virtual TSubclassOf<UShooterInGameMenuScreen> GetInGameMenuWidgetClass() const override;
	virtual TSubclassOf<UShooterSurrenderVoteScreen> GetSurrenderVoteWidgetClass() const override;
	virtual TSubclassOf<UShooterAugmentSelectScreen> GetAugmentSelectWidgetClass() const override;
	virtual UShooterInGameMenuScreen* GetInGameMenuInstance() const override;
	virtual void SetInGameMenuInstance(UShooterInGameMenuScreen* Instance) override;
	virtual UShooterSurrenderVoteScreen* GetSurrenderVoteInstance() const override;
	virtual void SetSurrenderVoteInstance(UShooterSurrenderVoteScreen* Instance) override;
	virtual UShooterAugmentSelectScreen* GetAugmentSelectInstance() const override;
	virtual void SetAugmentSelectInstance(UShooterAugmentSelectScreen* Instance) override;

private:
	UFUNCTION()
	void HandleDominationFlowStateChanged();

	UFUNCTION()
	void HandleMatchStateChanged();

	UFUNCTION()
	void HandleParticipationStateChanged(EPlayerParticipationState ParticipationState);

	UFUNCTION()
	void HandleCombatClassChanged(EDominationCombatClass SelectedClass);
	
	// 새 라운드 진입 시 증강 선택 보류 상태를 초기화한다
	UFUNCTION()
	void HandleRewardRoundChanged(EGameTeam Team, int32 NewScore);
	UFUNCTION()
	void HandleAugmentDismissRequested();
	
	void SetAugmentSelectDismissed(bool bDismissed);

	void RefreshDominationUI();
	void RefreshClassSelectionPresentation(
		ADominationGameState* DominationGameState,
		ADominationPlayerState* PlayerState);
	void PresentClassSelectionScreen();
	void RemoveClassSelectionScreen();
	void TryCaptureMatchResult();
	void ScheduleInitializationRetry();
	void ClearInitializationRetry();
	void BindToGameState(ADominationGameState* DominationGameState);
	void UnbindFromGameState();
	void BindToPlayerState(ADominationPlayerState* PlayerState);
	void UnbindFromPlayerState();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterPlayerHUD> InitialWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI", meta = (AllowPrivateAccess = "true", Categories = "UI.Layer"))
	FGameplayTag InitialWidgetLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI|Class Selection", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UDominationClassSelectScreen> ClassSelectionWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI|System Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterInGameMenuScreen> InGameMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI|Surrender", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterSurrenderVoteScreen> SurrenderVoteClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|UI|Augment", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterAugmentSelectScreen> AugmentSelectClass;

	UPROPERTY(Transient)
	TObjectPtr<UShooterPlayerHUD> InitialWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UDominationClassSelectScreen> ClassSelectionWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UShooterInGameMenuScreen> InGameMenuInstance;

	UPROPERTY(Transient)
	TObjectPtr<UShooterSurrenderVoteScreen> SurrenderVoteInstance;

	UPROPERTY(Transient)
	TObjectPtr<UShooterAugmentSelectScreen> AugmentSelectInstance;

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	TWeakObjectPtr<ADominationPlayerState> BoundPlayerState;
	FTimerHandle InitializationRetryTimerHandle;
	
	// 현재 보류 중인지
	bool bAugmentSelectDismissed = false;
	// 이번 띄운 후보 묶음에서 한번이라도 보류했는지
	bool bHasDismissedThisSelection = false;
	
	TArray<FPrimaryAssetId> LastPresentedOfferIds;
	
	int32 LastObservedRewardRound = 0;
	// 보류 상태 초기화 규칙을 위한 매치 시작시 한번 감지
	EDominationMatchPhase LastObservedMatchPhase = EDominationMatchPhase::WaitingForPlayers;
};
