#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameplayTagContainer.h"
#include "Player/PlayerParticipationState.h"
#include "TimerManager.h"
#include "UI/Frontend/TeamMatchUIInitComponent.h"
#include "CombatUIInitComponent.generated.h"

class ADominationPlayerState;
class APlayerState;
class APawn;
class UDominationClassSelectScreen;
class UShooterAugmentSelectScreen;
class UShooterInGameMenuScreen;
class UShooterPlayerHUD;
class UShooterPrimaryGameLayout;
class UShooterSurrenderVoteScreen;
class UCommonActivatableWidget;
class UCommonActivatableWidgetContainerBase;

// Names retained so existing Domination widget bindings continue to resolve.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentDismissStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentPresentationChanged);

// Shared root/HUD/class-selection/augment presentation. Derived classes supply only mode policy.
UCLASS(Abstract, ClassGroup = (UI))
class FPS_API UCombatUIInitComponent : public UTeamMatchUIInitComponent
{
    GENERATED_BODY()
public:
    UCombatUIInitComponent();
    UFUNCTION(BlueprintCallable, Category = "Combat|UI")
    void ToggleAugmentSelect();
    UFUNCTION(BlueprintPure, Category = "Combat|UI")
    bool IsAugmentSelectDismissed() const;
    UFUNCTION(BlueprintPure, Category = "Combat|UI")
    bool IsAugmentPresentationSettled() const;
    UPROPERTY(BlueprintAssignable, Category = "Combat|UI")
    FOnAugmentDismissStateChanged OnAugmentDismissStateChanged;
    UPROPERTY(BlueprintAssignable, Category = "Combat|UI")
    FOnAugmentPresentationChanged OnAugmentPresentationChanged;
    void NotifyClassSelectionRequestFailed(const FText& Reason);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void RefreshCombatUI();
    ADominationPlayerState* GetCombatClassPlayerState() const;
    void SetAugmentSelectDismissed(bool bDismissed);
    void ResetAugmentPresentationHistory();
    virtual bool PrepareModeBindings()
        PURE_VIRTUAL(UCombatUIInitComponent::PrepareModeBindings, return false;);
    virtual bool CanInitializeCombatHUD(const ADominationPlayerState& ClassState) const
        PURE_VIRTUAL(UCombatUIInitComponent::CanInitializeCombatHUD, return false;);
    virtual bool ShouldPresentClassSelection(const ADominationPlayerState& ClassState) const
        PURE_VIRTUAL(UCombatUIInitComponent::ShouldPresentClassSelection, return false;);
    virtual bool IsAugmentModeActive() const
        PURE_VIRTUAL(UCombatUIInitComponent::IsAugmentModeActive, return false;);
    virtual void OnCombatHUDCreated(UShooterPlayerHUD*) {}
    virtual void OnClassSelectionPresented() {}
    virtual bool IsAugmentPresentationAllowed() const override;
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
    void HandleParticipationStateChanged(EPlayerParticipationState ParticipationState);
    UFUNCTION()
    void HandleCombatClassChanged(EDominationCombatClass SelectedClass);
    UFUNCTION()
    void HandleCombatPawnSet(APlayerState* ChangedState, APawn* NewPawn, APawn* OldPawn);
    UFUNCTION()
    void HandleAugmentDismissRequested();
    void HandleModalDisplayedWidgetChanged(UCommonActivatableWidget* DisplayedWidget);
    void BindModalContainer();
    void UnbindModalContainer();
    UCommonActivatableWidgetContainerBase* GetModalContainer() const;
    void PresentClassSelectionScreen();
    void RemoveClassSelectionScreen();
    void RemoveCombatHUD();
    void ScheduleInitializationRetry();
    void ClearInitializationRetry();
    void BindToPlayerState(ADominationPlayerState* ClassState);
    void UnbindFromPlayerState();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterPlayerHUD> InitialWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|UI", meta = (AllowPrivateAccess = "true", Categories = "UI.Layer"))
	FGameplayTag InitialWidgetLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|UI|Class Selection", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UDominationClassSelectScreen> ClassSelectionWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|UI|System Menu", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterInGameMenuScreen> InGameMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|UI|Surrender", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UShooterSurrenderVoteScreen> SurrenderVoteClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|UI|Augment", meta = (AllowPrivateAccess = "true"))
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

    TWeakObjectPtr<ADominationPlayerState> BoundPlayerState;
    TWeakObjectPtr<UCommonActivatableWidgetContainerBase> BoundModalContainer;
    FTimerHandle InitializationRetryTimerHandle;
    bool bAugmentSelectDismissed = false;
    bool bHasDismissedThisSelection = false;
    TArray<FPrimaryAssetId> LastPresentedOfferIds;
};
