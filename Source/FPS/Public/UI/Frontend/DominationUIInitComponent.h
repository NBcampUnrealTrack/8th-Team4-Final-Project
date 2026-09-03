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

/**
 * Domination의 전투 HUD 준비와 매치 페이즈별 공통 화면 허용 여부를 담당한다.
 */
UCLASS(ClassGroup = (UI))
class FPS_API UDominationUIInitComponent : public UTeamMatchUIInitComponent
{
	GENERATED_BODY()

public:
	UDominationUIInitComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsAugmentPresentationAllowed() const override;
	virtual bool IsSurrenderRequestAllowed() const override;

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
};
