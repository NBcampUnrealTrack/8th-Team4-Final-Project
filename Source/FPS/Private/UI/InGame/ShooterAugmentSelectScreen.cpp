// ShooterAugmentSelectScreen.cpp
#include "UI/InGame/ShooterAugmentSelectScreen.h"

#include "UI/Common/ShooterAugmentCard.h"
#include "UI/Common/ShooterRerollButton.h"
#include "UI/ShooterUIBundles.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "CommonButtonBase.h"
#include "UI/Common/ShooterAugmentSelectBanner.h"
#include "Components/Overlay.h"
#include "Animation/WidgetAnimation.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAugmentSelectScreen, Log, All);

namespace 
{
	bool AreOffersEqual(
		const TArray<FAugmentInventoryEntry>& A,
		const TArray<FAugmentInventoryEntry>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			if (A[i].DefinitionId != B[i].DefinitionId)
			{
				return false;
			}
		}
		return true;
	}
	
}

void UShooterAugmentSelectScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	CachedCards = { Card_0, Card_1, Card_2 };
	CachedRerollButtons = { RerollButton_0, RerollButton_1, RerollButton_2};
	
	for (UShooterAugmentCard* Card : CachedCards)
	{
		Card->OnAugmentCardClicked.AddUObject(this, &UShooterAugmentSelectScreen::HandleCardClicked);
		Card->OnAugmentCardHovered.AddUObject(this, &UShooterAugmentSelectScreen::HandleCardHovered);
	}
	
	for (UShooterRerollButton* Button : CachedRerollButtons)
	{
		Button->OnRerollButtonClicked.AddUObject(this, &ThisClass::HandleRerollButtonClicked);
	}
	
	if (Btn_PendingCard)
	{
		Btn_PendingCard->OnClicked().AddUObject(this, &ThisClass::HandlePendingCardClicked);
	}
}

void UShooterAugmentSelectScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (Banner)
	{
		Banner->SetPresentationActive(true);
	}
}

void UShooterAugmentSelectScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (CachedCards.IsEmpty() || Layout.GetNumCards() != CachedCards.Num())
	{
		return;
	}

	if (bInstallOutroPlaying)
	{
		return;
	}

	if (!Layout.HasValidBasis()
		&& !Layout.IsOutroPlaying()
		&& Layout.GetHoveredIndex() == INDEX_NONE)
	{
		Layout.SetBasis(BuildLayoutBasisFromGeometry(), LayoutSettings);
	}

	const bool bOutroFinished = Layout.Tick(InDeltaTime, LayoutSettings);

	ApplyLayoutStates();

	// Focus 완료가 곧 종료가 아니다. Presenter 로 넘기고 뒷구간을 재생한다
	if (bOutroFinished)
	{
		BeginInstallOutro();
	}
}

FAugmentLayoutBasis UShooterAugmentSelectScreen::BuildLayoutBasisFromGeometry() const
{
	FAugmentLayoutBasis Out;
	
	if (CachedCards.IsEmpty())
	{
		return Out;
	}
	
	// 1) 세 카드의 절대 중심을 모은다
	TArray<FVector2D> AbsCenters;
	AbsCenters.Reserve(CachedCards.Num());
	
	for (const UShooterAugmentCard* Card : CachedCards)
	{
		if (!IsValid(Card))
		{
			return FAugmentLayoutBasis();
		}
		
		const FGeometry& G = Card->GetCachedGeometry();
	
		if (G.GetLocalSize().X <= KINDA_SMALL_NUMBER
			|| G.GetLocalSize().Y <= KINDA_SMALL_NUMBER)
		{
			return FAugmentLayoutBasis();
		}
		
		AbsCenters.Add(G.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f)));
	}
	
	// 2) 앵커는 hand-off 지점, 즉 SelectedCardStage 의 중심이다.
	FVector2D AbsAnchor = AbsCenters[AbsCenters.Num() / 2];

	if (IsValid(SelectedCardStage))
	{
		const FGeometry& StageGeometry = SelectedCardStage->GetCachedGeometry();

		// Hidden 이면 크기가 잡히고 Collapsed 면 0 이다. 0 이면 위에서 잡은 폴백을 그대로 쓴다
		if (StageGeometry.GetLocalSize().X > KINDA_SMALL_NUMBER
			&& StageGeometry.GetLocalSize().Y > KINDA_SMALL_NUMBER)
		{
			AbsAnchor = StageGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
		}
	}


	// 3) 각 카드의 로컬 단위로 환산
	Out.SelectionTranslations.Reserve(CachedCards.Num());
	for (int32 i = 0; i < CachedCards.Num(); ++i)
	{
		const FGeometry& G = CachedCards[i]->GetCachedGeometry();
		
		const FVector2D LocalCenter = FVector2D(G.GetLocalSize()) * 0.5f;
		const FVector2D AnchorInLocal = G.AbsoluteToLocal(AbsAnchor);
		
		Out.SelectionTranslations.Add(AnchorInLocal - LocalCenter);
	}
	
	Out.bValid = true;
	return Out;
}

void UShooterAugmentSelectScreen::ApplyLayoutStates()
{
	const TArray<FAugmentCardLayoutState>& States = Layout.GetStates();
	const float Progress = Layout.GetRejectedProgress();
	const int32 Selected = Layout.GetSelectedIndex();
	
	for (int32 i = 0; i < CachedCards.Num(); ++i)
	{
		UShooterAugmentCard* Card = CachedCards[i];
		if (!IsValid(Card) || !States.IsValidIndex(i))
		{
			continue;
		}
		
		const FAugmentCardLayoutState& S = States[i];
		
		Card->SetRenderTranslation(S.Translation);
		Card->SetRenderScale(FVector2D(S.Scale, S.Scale));
		Card->SetRenderOpacity(S.Opacity);
		
		if (Selected != INDEX_NONE && i != Selected)
		{
			Card->SetRejectedProgress(Progress);
		}
	}
}

void UShooterAugmentSelectScreen::HandleCardHovered(int32 InOfferIndex, bool bHovered)
{
	if (bHovered)
	{
		SetHoveredCard(InOfferIndex);
	}
	else if (Layout.GetHoveredIndex() == InOfferIndex)
	{
		SetHoveredCard(INDEX_NONE);
	}
}

void UShooterAugmentSelectScreen::SetOffers(const TArray<FAugmentInventoryEntry>& InOffers,
                                            const TArray<int32>& InRerollsRemaining)
{
	bOffersChangedThisRefresh = !AreOffersEqual(InOffers, Offers);
	if (bOffersChangedThisRefresh)
	{
		ResetSelectionPresentation();
	}
	
	// 이전 아이콘 로드가 진행 중이면 취소
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	Offers = InOffers;
	RerollsRemaining = InRerollsRemaining;
	
	TryBuildCards();
}

void UShooterAugmentSelectScreen::SetEntryAnimation(EAugmentSelectEntry InEntry)
{
	EntryAnimation = InEntry;
}

void UShooterAugmentSelectScreen::SetDismissAvailable(bool bAvailable)
{
	if (Btn_PendingCard)
	{
		Btn_PendingCard->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShooterAugmentSelectScreen::SetHoveredCard(int32 InIndex)
{
	if (!Layout.HasValidBasis())
	{
		return;
	}
	
	if (IsOutroPlaying())
	{
		return;
	}
	
	const int32 NewHoveredIndex = Offers.IsValidIndex(InIndex) ? InIndex : INDEX_NONE;
	
	if (Layout.GetHoveredIndex() == NewHoveredIndex)
	{
		return;
	}
	
	Layout.SetHovered(NewHoveredIndex, LayoutSettings);
}

void UShooterAugmentSelectScreen::BeginSelectionLayout(int32 InSelectedIndex)
{
	Layout.BeginOutro(InSelectedIndex, LayoutSettings);
}

void UShooterAugmentSelectScreen::ResetLayout()
{
	Layout.Reset(CachedCards.Num(), LayoutSettings);
	
	// 디졸브 되돌리기
	for (UShooterAugmentCard* Card : CachedCards)
	{
		if (IsValid(Card))
		{
			Card->SetRejectedProgress(0.f);
		}
	}
	
	ApplyLayoutStates();
}

void UShooterAugmentSelectScreen::NotifySelectionOutroFinished()
{
	OnSelectionOutroFinished.Broadcast();
}

void UShooterAugmentSelectScreen::BeginInstallOutro()
{
	const int32 Selected = Layout.GetSelectedIndex();

	UAugmentRegistrySubsystem* Registry = GetRegistry();
	UAugmentDefinition* Def = (IsValid(Registry) && Offers.IsValidIndex(Selected))
		? Registry->FindDefinition(Offers[Selected].DefinitionId)
		: nullptr;

	// 하나라도 없으면 설치 연출이 없는 것 카드 연출을 검증할 수 있게 하는 폴백
	if (!IsValid(SelectedCardStage) || !IsValid(SelectedCardPresenter)
		|| !Anim_InstallOutro || !IsValid(Def))
	{
		NotifySelectionOutroFinished();
		return;
	}

	SelectedCardPresenter->SetData(Def, Selected);

	// Focus 종료 크기와 맞춘다.
	if (IsValid(StageScaleRoot))
	{
		StageScaleRoot->SetRenderScale(FVector2D(LayoutSettings.OutroSelectedScale));
	}

	SelectedCardStage->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Collapsed 가 아니라 Opacity 0 
	// 레이아웃에서 빠지면 나머지 두 장의 지오메트리가 흔들리고 화면 재사용 때 Visibility가 어긋날 수 있음
	if (CachedCards.IsValidIndex(Selected) && IsValid(CachedCards[Selected]))
	{
		CachedCards[Selected]->SetRenderOpacity(0.f);
	}

	// ── 뒷구간 재생 ──

	bInstallOutroPlaying = true;

	UnbindAllFromAnimationFinished(Anim_InstallOutro);
	InstallOutroFinishedDelegate.BindDynamic(this, &ThisClass::HandleInstallOutroFinished);
	BindToAnimationFinished(Anim_InstallOutro, InstallOutroFinishedDelegate);

	PlayAnimation(Anim_InstallOutro,
		/*StartAtTime=*/LayoutSettings.OutroDuration,
		/*NumLoopsToPlay=*/1,
		EUMGSequencePlayMode::Forward,
		/*PlaybackSpeed=*/1.f,
		/*bRestoreState=*/false);
}

void UShooterAugmentSelectScreen::HandleInstallOutroFinished()
{
	bInstallOutroPlaying = false;
	NotifySelectionOutroFinished();
}

void UShooterAugmentSelectScreen::ResetSelectionPresentation()
{
	if (Anim_InstallOutro)
	{
		StopAnimation(Anim_InstallOutro);
		UnbindAllFromAnimationFinished(Anim_InstallOutro);
	}

	bInstallOutroPlaying = false;

	if (IsValid(SelectedCardStage))
	{
		SelectedCardStage->SetVisibility(ESlateVisibility::Hidden);
	}

	// hand-off 에서 넣은 OutroSelectedScale 을 되돌린다.
	if (IsValid(StageScaleRoot))
	{
		StageScaleRoot->SetRenderScale(FVector2D(1.f));
	}

	// 감춰둔 카드의 Opacity 는 아래 ResetLayout() 이 되돌린다.
	// 카드 내부 연출도 같이 되돌린다.
	for (UShooterAugmentCard* Card : CachedCards)
	{
		if (IsValid(Card))
		{
			Card->ResetFeedback();
		}
	}
	
	ResetLayout();
	BP_OnSelectionPresentationReset();
}

bool UShooterAugmentSelectScreen::IsOutroPlaying() const
{
	return Layout.IsOutroPlaying() || bInstallOutroPlaying;
}

void UShooterAugmentSelectScreen::TryBuildCards()
{
	UAugmentRegistrySubsystem* Registry = GetRegistry();
	if (!IsValid(Registry))
	{
		return;
	}

	if (Registry->IsReady() == false)
	{
		Registry->OnRegistryReady.AddDynamic(this, &ThisClass::HandleRegistryReady);
		return;
	}

	TArray<FPrimaryAssetId> Ids;

	for (const auto& Offer : Offers)
	{
		Ids.Add(Offer.DefinitionId);
	}

	IconLoadHandle = UAssetManager::Get().ChangeBundleStateForPrimaryAssets(Ids, {FShooterUIBundles::Icon}, {});

	FStreamableDelegate OnLoaded = FStreamableDelegate::CreateUObject(this, &ThisClass::HandleIconsLoaded);

	// CallOrRegistry 구조
	if (!IconLoadHandle.IsValid() || IconLoadHandle->HasLoadCompleted())
	{
		FStreamableHandle::ExecuteDelegate(OnLoaded);
	}
	else
	{
		IconLoadHandle->BindCompleteDelegate(OnLoaded);
	}
}

void UShooterAugmentSelectScreen::RefreshOfferWidgets()
{
	if (IsOutroPlaying())
	{
		return;
	}
	
	UAugmentRegistrySubsystem* Registry = GetRegistry();
	if (!IsValid(Registry))
	{
		return;
	}
	
	const bool bWasEmpty = LastDisplayedOffers.IsEmpty();
	const bool bIsEmpty = Offers.IsEmpty();
	const bool bOfferCountChanged = LastDisplayedOffers.Num() != Offers.Num();
	
	TArray<int32> ChangedSlots;
	
	// Reroll 했을때 처리
	if (!bWasEmpty && !bIsEmpty && !bOfferCountChanged)
	{
		for (int32 i = 0; i < Offers.Num(); ++i)
		{
			if (LastDisplayedOffers[i].DefinitionId != Offers[i].DefinitionId)
			{
				ChangedSlots.Add(i);
			}
		}
	}
	
	for (int32 i = 0; i < CachedCards.Num(); ++i)
	{
		const bool bHasOffer = Offers.IsValidIndex(i);
		CachedCards[i]->SetVisibility(bHasOffer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (CachedRerollButtons.IsValidIndex(i))
		{
			CachedRerollButtons[i]->SetVisibility(
				bHasOffer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}

		if (!bHasOffer)
		{
			continue;
		}

		UAugmentDefinition* Def = Registry->FindDefinition(Offers[i].DefinitionId);
		if (IsValid(Def) == false)
		{
			CachedCards[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (CachedRerollButtons.IsValidIndex(i))
			{
				CachedRerollButtons[i]->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}
		
		if (ChangedSlots.Num() == 1 && ChangedSlots[0] == i)
		{
			CachedCards[i]->BeginRerollPresentation();
		}

		CachedCards[i]->SetData(Def, i);
		if (CachedRerollButtons.IsValidIndex(i))
		{
			CachedRerollButtons[i]->SetSlotData(i,
				RerollsRemaining.IsValidIndex(i) ? RerollsRemaining[i] : 0);
		}
	}
	
	if (bOffersChangedThisRefresh || Layout.GetNumCards() != CachedCards.Num())
	{
		ResetLayout();
	}
	
	if (!ensureMsgf(!Offers.IsEmpty(), TEXT("[ShooterAugmentSelectScreen] RefreshOfferWidgets가 빈 Offers로 호출되었습니다")))
	{
		return;
	}
	
	// 첫 묶음 상태인지 확인
	const bool bWholeBatchReplaced = !ChangedSlots.IsEmpty() && ChangedSlots.Num() != 1;
	
	if (bWasEmpty || bOfferCountChanged || bWholeBatchReplaced)
	{
		UWidgetAnimation* Anim = 
			(EntryAnimation == EAugmentSelectEntry::Resume && Anim_Resume)
			? Anim_Resume.Get()
			: Anim_Intro.Get();
		
		if (Anim)
		{
			PlayAnimation(Anim);
		}
	}
	else
	{
		for (const int32 ChangedSlot : ChangedSlots)
		{
			if (CachedCards.IsValidIndex(ChangedSlot))
			{
				CachedCards[ChangedSlot]->PlayRerollFeedback();
			}
		}
	}
	
	LastDisplayedOffers = Offers;
}

void UShooterAugmentSelectScreen::HandleRegistryReady()
{
	if (UAugmentRegistrySubsystem* Registry = GetRegistry())
	{
		Registry->OnRegistryReady.RemoveDynamic(this, &ThisClass::HandleRegistryReady);
	}

	TryBuildCards();
}

void UShooterAugmentSelectScreen::HandleIconsLoaded()
{
	RefreshOfferWidgets();
}

void UShooterAugmentSelectScreen::HandleCardClicked(int32 InOfferIndex)
{
	if (IsOutroPlaying() || !Offers.IsValidIndex(InOfferIndex))
	{
		return;
	}

	BeginSelectionLayout(InOfferIndex);

	if (Banner)
	{
		Banner->SetPresentationActive(false);
	}
	
	// 선택 카드에는 아무것도 재생하지 않는다. 탈락 카드만 돈다.
	// 선택 카드는 Focus -> Presenter hand-off -> Anim_InstallOutro 로 이어진다
	for (int32 i = 0; i < CachedCards.Num(); ++i)
	{
		if (i == InOfferIndex || !IsValid(CachedCards[i]))
		{
			continue;
		}
		CachedCards[i]->PlayRejectedFeedback();
	}
	
	for (UShooterRerollButton* Button : CachedRerollButtons)
	{
		if (IsValid(Button))
		{
			Button->SetIsEnabled(false);
		}
	}

	if (Anim_InstallOutro)
	{
		PlayAnimationTimeRange(Anim_InstallOutro,
			/*StartAtTime=*/0.f,
			/*EndAtTime=*/LayoutSettings.OutroDuration,
			/*NumLoopsToPlay=*/1,
			EUMGSequencePlayMode::Forward,
			/*PlaybackSpeed=*/1.f,
			/*bRestoreState=*/false);
	}

	// 같은 프레임에 즉시 전송한다. 연출 완료를 기다리면
	// 타임아웃 자동선택과 경합해 다른 증강이 들어간다
	OnAugmentSelected.Broadcast(InOfferIndex);
}

void UShooterAugmentSelectScreen::HandleRerollButtonClicked(int32 InSlotIndex)
{
	if (IsOutroPlaying())
	{
		return;
	}
	
	OnAugmentRerollRequested.Broadcast(InSlotIndex);
}

void UShooterAugmentSelectScreen::HandlePendingCardClicked()
{
	OnDismissRequested.Broadcast();
}

UAugmentRegistrySubsystem* UShooterAugmentSelectScreen::GetRegistry() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UAugmentRegistrySubsystem>() : nullptr;
}

void UShooterAugmentSelectScreen::NativeDestruct()
{
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	if (UAugmentRegistrySubsystem* Registry = GetRegistry())
	{
		Registry->OnRegistryReady.RemoveDynamic(this, &ThisClass::HandleRegistryReady);
	}
	
	LastDisplayedOffers.Reset();

	Super::NativeDestruct();
}

void UShooterAugmentSelectScreen::NativeOnDeactivated()
{
	if (Banner)
	{
		Banner->SetPresentationActive(false);
	}
	
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	LastDisplayedOffers.Reset();
	ResetSelectionPresentation();

	// 다음 Draft 데이터가 준비되기 전에 이전 Draft 카드가 노출되는 것을 막는다.
	for (UShooterAugmentCard* Card : CachedCards)
	{
		if (IsValid(Card))
		{
			Card->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	for (UShooterRerollButton* Button : CachedRerollButtons)
	{
		if (IsValid(Button))
		{
			Button->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	Super::NativeOnDeactivated();
}
