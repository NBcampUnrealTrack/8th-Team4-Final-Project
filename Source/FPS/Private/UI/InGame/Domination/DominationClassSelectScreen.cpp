#include "UI/InGame/Domination/DominationClassSelectScreen.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "UI/InGame/Domination/DominationClassInfoPanel.h"
#include "UI/InGame/Domination/DominationClassPreviewStage.h"
#include "UI/InGame/Domination/DominationPlayerSlot.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationClassSelectScreen, Log, All);

UDominationClassSelectScreen::UDominationClassSelectScreen()
{
	InputConfig = EShooterWidgetInputMode::Menu;
}

void UDominationClassSelectScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	
	if (IsValid(ClassSelectBGM))
	{
		BGMComponent = UGameplayStatics::SpawnSound2D(
			this,
			ClassSelectBGM
		);
	}

	BuildIconCache();
	RebuildClassCards();
	BindToPlayerState();

	ReconcileSlots();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReconcileTimerHandle,
			this,
			&ThisClass::ReconcileSlots,
			0.25f,
			true);
	}

	SpawnPreviewStage();

	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	const bool bIsJoinInProgressSelection = IsValid(PlayerState)
		&& PlayerState->GetParticipationState() == EPlayerParticipationState::WaitingForNextSpawn;

	if (IsValid(Button_LockIn))
	{
		Button_LockIn->SetVisibility(
			bIsJoinInProgressSelection ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		Button_LockIn->OnClicked().AddUObject(this, &ThisClass::HandleLockInClicked);
	}

	if (IsValid(Text_Timer))
	{
		Text_Timer->SetVisibility(
			bIsJoinInProgressSelection ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (!bIsJoinInProgressSelection)
	{
		RefreshTimerText();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				TimerTextHandle,
				this,
				&ThisClass::RefreshTimerText,
				0.10f,
				true);
		}
	}
}

void UDominationClassSelectScreen::NativeOnDeactivated()
{
	if (IsValid(BGMComponent))
	{
		BGMComponent->FadeOut(0.3f, 0.0f);
		BGMComponent = nullptr;
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerTextHandle);
		World->GetTimerManager().ClearTimer(ReconcileTimerHandle);
	}

	if (IsValid(Button_LockIn))
	{
		Button_LockIn->OnClicked().RemoveAll(this);
	}

	bLockInRequestPending = false;

	DestroyPreviewStage();
	ClearSlotEntries();
	UnbindFromPlayerState();
	ClearClassCards();

	ClassIconCache.Empty();
	ClassPortraitCache.Empty();
	ClassWeaponArtCache.Empty();

	Super::NativeOnDeactivated();
}

void UDominationClassSelectScreen::HandleCombatClassChanged(
	const EDominationCombatClass SelectedClass)
{
	RefreshSelectionState();
	RefreshLockInButton();
	RefreshClassInfoPanel();
	RefreshAllSlots();
	RefreshPreviewStage();
}

void UDominationClassSelectScreen::HandlePreviewCombatClassChanged(
	const EDominationCombatClass PreviewClass)
{
	RefreshSelectionState();
	RefreshLockInButton();
	RefreshClassInfoPanel();
	RefreshAllSlots();
	RefreshPreviewStage();
}

void UDominationClassSelectScreen::HandleClassCardClicked(
	const EDominationCombatClass RequestedClass)
{
	// 확정 요청을 보낸 뒤에는 임시 선택도 바꾸지 못하게 함
	if (bLockInRequestPending)
	{
		return;
	}

	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	ADominationPlayerController* PlayerController = Cast<ADominationPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerState) || !IsValid(PlayerController))
	{
		return;
	}

	switch (PlayerState->GetParticipationState())
	{
	case EPlayerParticipationState::WaitingForMatchStart:
		PlayerController->SetPreviewCombatClass(RequestedClass);
		break;

	case EPlayerParticipationState::WaitingForNextSpawn:
		PlayerController->SelectCombatClass(RequestedClass);
		break;

	default:
		break;
	}
}

void UDominationClassSelectScreen::HandleClassCardHovered(
	const EDominationCombatClass HoveredClass,
	const bool bHovered)
{
	if (!bHovered || bLockInRequestPending)
	{
		CardHoverController.SetHovered(INDEX_NONE);
		return;
	}

	const int32 HoveredIndex = ClassCards.IndexOfByPredicate(
		[HoveredClass](const UDominationClassSelectCard* Card)
		{
			return IsValid(Card) && Card->GetCombatClass() == HoveredClass;
		});

	CardHoverController.SetHovered(HoveredIndex);
}

void UDominationClassSelectScreen::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CardHoverController.Tick(InDeltaTime, CardHoverSettings))
	{
		return;
	}

	const TArray<FDominationCardHoverState>& States = CardHoverController.GetStates();
	for (int32 Index = 0; Index < ClassCards.Num(); ++Index)
	{
		if (States.IsValidIndex(Index) && IsValid(ClassCards[Index]))
		{
			ClassCards[Index]->ApplyHoverState(States[Index]);
		}
	}
}

void UDominationClassSelectScreen::HandleLockInClicked()
{
	if (bLockInRequestPending)
	{
		return;
	}
	
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	ADominationPlayerController* PlayerController = Cast<ADominationPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerState)
		|| !IsValid(PlayerController)
		|| !PlayerState->HasPreviewCombatClass())
	{
		return;
	}

	bLockInRequestPending = true;
	RefreshLockInButton();

	// 확정 순간 hover 중이던 카드도 기본 배치로 되돌린다.
	CardHoverController.SetHovered(INDEX_NONE);

	if (IsValid(LockInSound))
	{
		UGameplayStatics::PlaySound2D(this, LockInSound);
	}
	if (IsValid(PreviewStage))
	{
		PreviewStage->PlayLockInEffect();
	}

	PlayerController->SelectCombatClass(PlayerState->GetPreviewCombatClass());
}

UTexture2D* UDominationClassSelectScreen::GetCachedClassIcon(
	const EDominationCombatClass CombatClass) const
{
	return ClassIconCache.FindRef(CombatClass);
}

UTexture2D* UDominationClassSelectScreen::GetCachedClassPortrait(
	const EDominationCombatClass CombatClass) const
{
	return ClassPortraitCache.FindRef(CombatClass);
}

UTexture2D* UDominationClassSelectScreen::GetCachedClassWeaponArt(
	const EDominationCombatClass CombatClass) const
{
	return ClassWeaponArtCache.FindRef(CombatClass);
}

EDominationCombatClass UDominationClassSelectScreen::GetDisplayCombatClass() const
{
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	if (!IsValid(PlayerState))
	{
		return EDominationCombatClass::Unselected;
	}

	return PlayerState->HasSelectedCombatClass()
		? PlayerState->GetCombatClass()
		: PlayerState->GetPreviewCombatClass();
}

void UDominationClassSelectScreen::RefreshClassInfoPanel()
{
	if (!IsValid(Panel_ClassInfo))
	{
		return;
	}

	const EDominationCombatClass DisplayClass = GetDisplayCombatClass();

	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	const FDominationCombatClassDefinition* Definition =
		(IsValid(ClassSettings) && DisplayClass != EDominationCombatClass::Unselected)
			? ClassSettings->FindDefinition(DisplayClass)
			: nullptr;

	// Definition이 nullptr이면 패널이 스스로 Collapsed한다.
	Panel_ClassInfo->SetDisplayClass(
		Definition,
		GetCachedClassIcon(DisplayClass));
}

void UDominationClassSelectScreen::BuildIconCache()
{
	ClassIconCache.Empty();
	ClassPortraitCache.Empty();
	ClassWeaponArtCache.Empty();

	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	if (!ensure(ClassSettings))
	{
		return;
	}

	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		if (Definition.CombatClass == EDominationCombatClass::Unselected)
		{
			continue;
		}

		if (UTexture2D* IconTexture = Definition.Icon.LoadSynchronous())
		{
			ClassIconCache.Add(Definition.CombatClass, IconTexture);
		}

		if (UTexture2D* PortraitTexture = Definition.Portrait.LoadSynchronous())
		{
			ClassPortraitCache.Add(Definition.CombatClass, PortraitTexture);
		}

		if (UTexture2D* WeaponArtTexture = Definition.WeaponArt.LoadSynchronous())
		{
			ClassWeaponArtCache.Add(Definition.CombatClass, WeaponArtTexture);
		}
	}
}

void UDominationClassSelectScreen::RebuildClassCards()
{
	ClearClassCards();

	if (!IsValid(Panel_ClassCards) || !ClassCardWidgetClass)
	{
		UE_LOG(LogDominationClassSelectScreen, Error,
			TEXT("Class selection UI is not configured on %s."),
			*GetNameSafe(this));
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return;
	}

	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	if (!IsValid(ClassSettings))
	{
		UE_LOG(LogDominationClassSelectScreen, Error,
			TEXT("Domination class settings are unavailable."));
		return;
	}

	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		if (Definition.CombatClass == EDominationCombatClass::Unselected)
		{
			continue;
		}

		UDominationClassSelectCard* ClassCard =
			CreateWidget<UDominationClassSelectCard>(
				PlayerController,
				ClassCardWidgetClass);
		if (!IsValid(ClassCard))
		{
			continue;
		}

		FDominationClassCardPresentation Presentation;
		Presentation.CombatClass = Definition.CombatClass;
		Presentation.DisplayName = Definition.DisplayName;
		Presentation.Description = Definition.Description;
		Presentation.WeaponArt = GetCachedClassWeaponArt(Definition.CombatClass);

		ClassCard->SetClassData(Presentation);
		ClassCard->OnClassCardClicked.AddUObject(
			this,
			&ThisClass::HandleClassCardClicked);
		ClassCard->OnClassCardHovered.AddUObject(
			this,
			&ThisClass::HandleClassCardHovered);

		UHorizontalBoxSlot* ClassCardSlot =
			Panel_ClassCards->AddChildToHorizontalBox(ClassCard);

		if (IsValid(ClassCardSlot))
		{
			ClassCardSlot->SetPadding(ClassCardPadding);
			ClassCardSlot->SetHorizontalAlignment(HAlign_Center);
			ClassCardSlot->SetVerticalAlignment(VAlign_Center);
		}

		ClassCards.Add(ClassCard);
	}

	CardHoverController.Reset(ClassCards.Num());
}

void UDominationClassSelectScreen::ClearClassCards()
{
	for (UDominationClassSelectCard* ClassCard : ClassCards)
	{
		if (IsValid(ClassCard))
		{
			ClassCard->OnClassCardClicked.RemoveAll(this);
			ClassCard->OnClassCardHovered.RemoveAll(this);
		}
	}

	ClassCards.Reset();
	CardHoverController.Reset(0);

	if (IsValid(Panel_ClassCards))
	{
		Panel_ClassCards->ClearChildren();
	}
}

void UDominationClassSelectScreen::SpawnPreviewStage()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !ensure(PreviewStageClass))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PreviewStage = World->SpawnActor<ADominationClassPreviewStage>(
		PreviewStageClass,
		PreviewStageSpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!IsValid(PreviewStage))
	{
		return;
	}

	if (IsValid(Image_CharacterPreview))
	{
		UMaterialInstanceDynamic* PreviewMaterial = PreviewStage->GetPreviewMaterial();
		if (IsValid(PreviewMaterial))
		{
			Image_CharacterPreview->SetBrushFromMaterial(PreviewMaterial);
			Image_CharacterPreview->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_CharacterPreview->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 화면을 다시 열었을 때 이미 고른 클래스가 있으면 바로 반영한다.
	RefreshPreviewStage();
}

void UDominationClassSelectScreen::DestroyPreviewStage()
{
	// 스테이지의 EndPlay가 Pawn과 무기까지 정리한다. 화면이 직접 지우지 않는다.
	if (IsValid(PreviewStage))
	{
		PreviewStage->Destroy();
	}
	PreviewStage = nullptr;
}

void UDominationClassSelectScreen::RefreshPreviewStage()
{
	if (!IsValid(PreviewStage))
	{
		return;
	}

	PreviewStage->SetPreviewClass(GetDisplayCombatClass());
}

void UDominationClassSelectScreen::ReconcileSlots()
{
	APlayerController* PlayerController = GetOwningPlayer();
	ADominationPlayerState* LocalPlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;

	if (!IsValid(LocalPlayerState) || !IsValid(GameState) || !PlayerSlotWidgetClass)
	{
		return;
	}

	// 로컬 팀이 아직 확정되지 않은 상태에서는 팀 필터를 수행하지 않음
	// None을 유효한 팀 값처럼 비교하지 않고, 다음 reconcile tick에서 다시 시도한다.
	const EGameTeam LocalTeam = LocalPlayerState->GetTeam();
	if (LocalTeam == EGameTeam::None)
	{
		return;
	}

	// PlayerArray에 현재 존재하는 모든 플레이어는 이탈 판정용 PresentSet
	// 그중 클래스 선택 대기 상태이면서 같은 팀인 플레이어만 슬롯 생성 대상은 EligibleList
	TSet<const ADominationPlayerState*> PresentSet;
	TArray<ADominationPlayerState*> EligibleList;
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		ADominationPlayerState* PlayerState = Cast<ADominationPlayerState>(Entry);
		if (!IsValid(PlayerState))
		{
			continue;
		}

		PresentSet.Add(PlayerState);

		if (PlayerState->GetParticipationState() == EPlayerParticipationState::WaitingForMatchStart
			&& PlayerState->GetTeam() == LocalTeam)
		{
			EligibleList.Add(PlayerState);
		}
	}

	bool bCreatedSlot = false;

	for (ADominationPlayerState* PlayerState : EligibleList)
	{
		const int32 PlayerId = PlayerState->GetPlayerId();
		if (PlayerId <= 0)
		{
			continue;
		}

		const bool bIsLocalPlayer = PlayerState == LocalPlayerState;

		// 재바인딩
		if (FDominationPlayerSlotEntry* ExistingEntry = SlotEntries.FindByPredicate(
			[PlayerId](const FDominationPlayerSlotEntry& Entry)
			{
				return Entry.PlayerId == PlayerId;
			}))
		{
			if (!ExistingEntry->PlayerState.IsValid())
			{
				BindSlotPlayerState(*ExistingEntry, PlayerState, bIsLocalPlayer);
			}
			continue;
		}

		UDominationPlayerSlot* PlayerSlot = CreateWidget<UDominationPlayerSlot>(
			PlayerController,
			PlayerSlotWidgetClass);
		if (!IsValid(PlayerSlot))
		{
			continue;
		}

		FDominationPlayerSlotEntry NewEntry;
		NewEntry.Slot = PlayerSlot;
		NewEntry.PlayerId = PlayerId;

		const int32 AddedIndex = SlotEntries.Add(MoveTemp(NewEntry));
		BindSlotPlayerState(SlotEntries[AddedIndex], PlayerState, bIsLocalPlayer);

		bCreatedSlot = true;
	}

	// PlayerArray에서 빠졌을 때만 이탈이다. 필터를 통과 못 한 것과 섞으면 안 된다.
	for (FDominationPlayerSlotEntry& Entry : SlotEntries)
	{
		const ADominationPlayerState* EntryPlayerState = Entry.PlayerState.Get();
		if (EntryPlayerState == nullptr || PresentSet.Contains(EntryPlayerState))
		{
			continue;
		}

		UnbindSlotPlayerState(Entry);

		if (IsValid(Entry.Slot))
		{
			Entry.Slot->NotifyDisconnected();
		}
	}

	// 새로운 Slot이 생성된 우리팀 경우에만 패널을 다시 구성하고 슬롯 갱신
	if (bCreatedSlot)
	{
		RebuildSlotPanel();

		// 늦게 발견한 팀원이 이미 Preview 상태일 수 있다. 그러면 변경 이벤트가 오지 않는다.
		RefreshAllSlots();
	}
}

void UDominationClassSelectScreen::RefreshAllSlots()
{
	for (const FDominationPlayerSlotEntry& Entry : SlotEntries)
	{
		if (!IsValid(Entry.Slot))
		{
			continue;
		}

		Entry.Slot->RefreshFromPlayerState();

		// 표시 클래스는 슬롯이 판정하고 아이콘 캐시는 화면이 갖고 있다.
		const EDominationCombatClass DisplayClass = Entry.Slot->GetDisplayClass();
		Entry.Slot->SetIconTexture(
			GetCachedClassWeaponArt(DisplayClass),
			GetCachedClassPortrait(DisplayClass));
	}
}

void UDominationClassSelectScreen::RebuildSlotPanel()
{
	if (!IsValid(Panel_PlayerSlots))
	{
		return;
	}

	SlotEntries.Sort([](const FDominationPlayerSlotEntry& Lhs, const FDominationPlayerSlotEntry& Rhs)
	{
		return Lhs.PlayerId < Rhs.PlayerId;
	});

	// 슬롯 크기와 간격은 WBP_PlayerSlot이 고정한다. 재배치는 신규가 생겼을 때만 한다.
	Panel_PlayerSlots->ClearChildren();
	for (const FDominationPlayerSlotEntry& Entry : SlotEntries)
	{
		if (IsValid(Entry.Slot))
		{
			Panel_PlayerSlots->AddChild(Entry.Slot);
		}
	}
}

void UDominationClassSelectScreen::ClearSlotEntries()
{
	for (FDominationPlayerSlotEntry& Entry : SlotEntries)
	{
		UnbindSlotPlayerState(Entry);
	}

	SlotEntries.Reset();

	if (IsValid(Panel_PlayerSlots))
	{
		Panel_PlayerSlots->ClearChildren();
	}
}

void UDominationClassSelectScreen::BindSlotPlayerState(
	FDominationPlayerSlotEntry& Entry,
	ADominationPlayerState* PlayerState,
	const bool bIsLocalPlayer)
{
	if (!IsValid(PlayerState))
	{
		return;
	}

	Entry.PlayerState = PlayerState;

	PlayerState->OnCombatClassChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleCombatClassChanged);
	PlayerState->OnPreviewCombatClassChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandlePreviewCombatClassChanged);

	if (IsValid(Entry.Slot))
	{
		// 기존 슬롯에 PlayerState를 다시 연결하고 DISCONNECTED 상태를 해제한다.
		Entry.Slot->SetPlayerState(PlayerState, bIsLocalPlayer);
	}
}

void UDominationClassSelectScreen::UnbindSlotPlayerState(FDominationPlayerSlotEntry& Entry)
{
	ADominationPlayerState* PlayerState = Entry.PlayerState.Get();

	// 내 PlayerState의 배선은 UnbindFromPlayerState가 따로 정리한다.
	if (IsValid(PlayerState) && PlayerState != BoundPlayerState.Get())
	{
		PlayerState->OnCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCombatClassChanged);
		PlayerState->OnPreviewCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePreviewCombatClassChanged);
	}

	Entry.PlayerState.Reset();
}

void UDominationClassSelectScreen::BindToPlayerState()
{
	UnbindFromPlayerState();

	APlayerController* PlayerController = GetOwningPlayer();
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	if (!IsValid(PlayerState))
	{
		return;
	}

	BoundPlayerState = PlayerState;
	PlayerState->OnCombatClassChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleCombatClassChanged);
	PlayerState->OnPreviewCombatClassChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandlePreviewCombatClassChanged);

	// 이미 복제되어 있을 수 있으므로 직접 갱신
	RefreshSelectionState();
	RefreshLockInButton();
	RefreshClassInfoPanel();
}

void UDominationClassSelectScreen::UnbindFromPlayerState()
{
	if (ADominationPlayerState* PlayerState = BoundPlayerState.Get())
	{
		PlayerState->OnCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCombatClassChanged);
		PlayerState->OnPreviewCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePreviewCombatClassChanged);
	}

	BoundPlayerState.Reset();
}

void UDominationClassSelectScreen::RefreshSelectionState()
{
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	const EDominationCombatClass PreviewClass = IsValid(PlayerState)
		? PlayerState->GetPreviewCombatClass()
		: EDominationCombatClass::Unselected;
	const bool bLocked = IsValid(PlayerState) && PlayerState->HasSelectedCombatClass();

	for (UDominationClassSelectCard* ClassCard : ClassCards)
	{
		if (IsValid(ClassCard))
		{
			ClassCard->SetSelectionState(PreviewClass, bLocked);
		}
	}
}

void UDominationClassSelectScreen::RefreshLockInButton()
{
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	const bool bLocked = IsValid(PlayerState) && PlayerState->HasSelectedCombatClass();
	const bool bHasPreview = IsValid(PlayerState) && PlayerState->HasPreviewCombatClass();

	if (IsValid(Button_LockIn))
	{
		Button_LockIn->SetIsEnabled(!bLocked && bHasPreview && !bLockInRequestPending);
	}

	UpdateLockInPresentation(bLocked, bHasPreview, bLockInRequestPending);
}

void UDominationClassSelectScreen::RefreshTimerText()
{
	if (!IsValid(Text_Timer))
	{
		return;
	}

	const UWorld* World = GetWorld();
	// ABaseRoundGameState에도 동명 함수가 있다. 반드시 ADominationGameState로 캐스팅한다.
	const ADominationGameState* DominationGameState = IsValid(World)
		? World->GetGameState<ADominationGameState>()
		: nullptr;
	if (!IsValid(DominationGameState))
	{
		Text_Timer->SetText(FText::GetEmpty());
		return;
	}

	const int32 TotalSeconds =
		FMath::Max(0, FMath::CeilToInt(DominationGameState->GetPhaseTimeRemaining()));
	Text_Timer->SetText(FText::FromString(
		FString::Printf(TEXT("%02d:%02d"), TotalSeconds / 60, TotalSeconds % 60)));
}
