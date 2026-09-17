// ShooterAugmentSelectScreen.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Augment/AugmentTypes.h"
#include "UI/Common/ShooterAugmentCard.h"
#include "UI/InGame/AugmentCardLayoutController.h"
#include "ShooterAugmentSelectScreen.generated.h"

class UShooterAugmentSelectBanner;
class UShooterRerollButton;
class UAugmentRegistrySubsystem;
class UHorizontalBox;
class UUniformGridPanel;
class UWidgetAnimation;
class UCurveFloat;
class UOverlay;
class USoundBase;
struct FStreamableHandle;

// 고른 번호를 서버에 보낸다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAugmentSelected, int32, OfferIndex);
// 리롤할 슬롯 번호를 서버에 보냄
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAugmentRerollRequested, int32, SlotIndex);
// 보류 요청
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentSelectDismissRequested);
// 연출이 끝났다는 로컬 신호
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectionOutroFinishedSignature);

UENUM(BlueprintType)
enum class EAugmentSelectEntry : uint8
{
	// 현재 후보 묶음을 처음 표시할 때
	Intro,
	// 보류에서 복귀
	Resume
};

/**
 * 증강 선택 화면으로 Modal 레이어에 올린다.
 */
UCLASS(Abstract)
class FPS_API UShooterAugmentSelectScreen : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	// 서버가 제시한 목록을 넣고, 순서가 곧 인덱스이므로 재정렬 하지 않는다.
	// RerollsRemaining은 Offers와 길이가 같음
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetOffers(const TArray<FAugmentInventoryEntry>& InOffers,
	               const TArray<int32>& InRerollsRemaining);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetEntryAnimation(EAugmentSelectEntry InEntry);
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetDismissAvailable(bool bAvailable);
	
	void SetHoveredCard(int32 InIndex);
	// 선택 아웃트로 시작
	void BeginSelectionLayout(int32 InSelectedIndex);
	void ResetLayout();
	
	// 서버가 선택을 거부했거나 새 Offers가 도착했을 때 원래상태로 되돌림
	void ResetSelectionPresentation();
	
	bool IsOutroPlaying() const;
	
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnAugmentSelected OnAugmentSelected;
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnAugmentRerollRequested OnAugmentRerollRequested;
	UPROPERTY(BlueprintAssignable, Category = "UI")
	FOnAugmentSelectDismissRequested OnDismissRequested;
	UPROPERTY(BlueprintAssignable, Category = "UI|Selection")
	FOnSelectionOutroFinishedSignature OnSelectionOutroFinished;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// 선택 아웃트로 시작
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Selection")
	void BP_OnSelectionOutroStarted(int32 SelectedOfferIndex);
	// 아웃트로 되돌림
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|Selection")
	void BP_OnSelectionPresentationReset();
	
	// 준비 확인 -> 아이콘 요청 -> 카드 생성
	// 준비되지않으면 기다렸다 다시 들어온다
	void TryBuildCards();

	void RefreshOfferWidgets();

	UFUNCTION()
	void HandleRegistryReady();

	void HandleIconsLoaded();
	void HandleCardClicked(int32 InOfferIndex);
	void HandleRerollButtonClicked(int32 InSlotIndex);
	void HandlePendingCardClicked();
	
	UAugmentRegistrySubsystem* GetRegistry() const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterAugmentCard> Card_0;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterAugmentCard> Card_1;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterAugmentCard> Card_2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterRerollButton> RerollButton_0;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterRerollButton> RerollButton_1;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterRerollButton> RerollButton_2;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Btn_PendingCard;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UShooterAugmentSelectBanner> Banner;

	// Outro 연출 전용 스테이지
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> SelectedCardStage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StageScaleRoot;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UShooterAugmentCard> SelectedCardPresenter;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UShooterAugmentCard> InstalledCardPresenter;

	UPROPERTY(EditDefaultsOnly, Category = "Layout")
	FAugmentCardLayoutSettings LayoutSettings;

	// 버튼 소리로 두면 사라지는 카드에서도 나므로 화면 가드 뒤에서 재생
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> CardHoverSound;
	
	// 화면 등장 연출
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Intro;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Resume;

	// OutroDuration 을 이음매로 두 구간으로 재생한다.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_InstallOutro;
	
	EAugmentSelectEntry EntryAnimation = EAugmentSelectEntry::Intro;

	TArray<FAugmentInventoryEntry> Offers;

	TArray<int32> RerollsRemaining;

	// 최초 1회 생성 후 파괴하지않고 값만 다시 채운다
	UPROPERTY()
	TArray<TObjectPtr<UShooterAugmentCard>> CachedCards;
	UPROPERTY()
	TArray<TObjectPtr<UShooterRerollButton>> CachedRerollButtons;

	// Deactivated에서 리셋된다
	TArray<FAugmentInventoryEntry> LastDisplayedOffers;

	TSharedPtr<FStreamableHandle> IconLoadHandle;
	
private:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	// 가운데 자리까지의 이동량을 만듦
	FAugmentLayoutBasis BuildLayoutBasisFromGeometry() const;
	// 컨트롤러의 CurrentStates 를 위젯에 쓴다
	void ApplyLayoutStates();

	// Focus 가 끝나면 선택 카드를 Presenter 로 넘기고 뒷구간을 재생한다
	void BeginInstallOutro();

	UFUNCTION()
	void HandleInstallOutroFinished();
	void HandleCardHovered(int32 InOfferIndex, bool bHovered);
	void NotifySelectionOutroFinished();

	FWidgetAnimationDynamicEvent InstallOutroFinishedDelegate;

	// Focus 가 끝나고 Anim_InstallOutro 뒷구간이 도는 동안 true
	bool bInstallOutroPlaying = false;
	bool bOffersChangedThisRefresh = false;

	FAugmentCardLayoutController Layout;
};
