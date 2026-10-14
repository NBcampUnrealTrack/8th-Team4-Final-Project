#include "UI/InGame/Domination/DominationClassSelectScreen.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Interfaces/RogueCombatClassSelector.h"
#include "GameFramework/PlayerController.h"
#include "Player/DominationPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Settings/ShooterAudioSettings.h"
#include "Settings/ShooterGameUserSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UI/InGame/Domination/DominationClassInfoPanel.h"
#include "UI/InGame/Domination/DominationClassPreviewStage.h"
#include "UI/InGame/Domination/DominationPlayerSlot.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationClassSelectScreen, Log, All);

UDominationClassSelectScreen::UDominationClassSelectScreen()
{
	InputConfig = EShooterWidgetInputMode::Menu;

	// 채팅 입력창이 Pop된 뒤 포커스가 돌아올 곳이 화면 자신이어야 Enter 프리뷰를 다시 받는다.
	SetIsFocusable(true);
}

FReply UDominationClassSelectScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bVoiceCaptureStartedByScreen)
	{
		if (AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>();
			IsValid(PlayerController) && PlayerController->IsPushToTalkKey(InKeyEvent.GetKey()))
		{
			PlayerController->StartVoiceTalking();
			bVoiceCaptureStartedByScreen = true;
			return FReply::Handled();
		}
	}

	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		if (AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>())
		{
			PlayerController->OpenChatInput(ChatInputScreenOffset);
			return FReply::Handled();
		}
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UDominationClassSelectScreen::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bVoiceCaptureStartedByScreen)
	{
		if (AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>();
			IsValid(PlayerController) && PlayerController->IsPushToTalkKey(InKeyEvent.GetKey()))
		{
			PlayerController->StopVoiceTalking();
			bVoiceCaptureStartedByScreen = false;
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

void UDominationClassSelectScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	
	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->OnAudioSettingsChanged.AddUObject(this, &ThisClass::HandleAudioSettingsChanged);
	}

	if (IsValid(ClassSelectBGM))
	{
		BGMComponent = UGameplayStatics::SpawnSound2D(
			this,
			ClassSelectBGM,
			ResolveBGMComponentVolume());
		ApplyBGMSettings();
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

	if (AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>())
	{
		PlayerController->SetMatchChatChannelAllowed(false);
	}

	if (IsValid(Button_LockIn))
	{
		Button_LockIn->OnClicked().AddUObject(this, &ThisClass::HandleLockInClicked);
	}

	RefreshSelectionModePresentation();
}

void UDominationClassSelectScreen::NativeOnDeactivated()
{
	if (bVoiceCaptureStartedByScreen)
	{
		if (AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>())
		{
			PlayerController->StopVoiceTalking();
		}
		bVoiceCaptureStartedByScreen = false;
	}

	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->OnAudioSettingsChanged.RemoveAll(this);
	}

	if (IsValid(BGMComponent))
	{
		BGMComponent->FadeOut(0.3f, 0.0f);
		BGMComponent = nullptr;
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	TimerTextHandle.Invalidate();
	ReconcileTimerHandle.Invalidate();
	HoverUpdateTimerHandle.Invalidate();

	if (IsValid(Button_LockIn))
	{
		Button_LockIn->OnClicked().RemoveAll(this);
	}

	bLockInRequestPending = false;
	bAllowActiveClassReselection = false;
	ActiveReselectionPreviewClass = EDominationCombatClass::Unselected;
	SelectionFailure = FText::GetEmpty();

	DestroyPreviewStage();

	if (AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>())
	{
		PlayerController->SetMatchChatChannelAllowed(true);
	}

	ClearSlotEntries();
	UnbindFromPlayerState();
	ClearClassCards();

	ClassIconCache.Empty();
	ClassPortraitCache.Empty();
	ClassWeaponArtCache.Empty();
	ClassVoiceCache.Empty();

	Super::NativeOnDeactivated();
}

void UDominationClassSelectScreen::HandleAudioSettingsChanged()
{
	ApplyBGMSettings();
}

void UDominationClassSelectScreen::ApplyBGMSettings()
{
	if (!IsValid(BGMComponent))
	{
		return;
	}

	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		if (USoundClass* MusicClass = Settings->GetAudioChannelSoundClass(EShooterAudioChannel::Music))
		{
			BGMComponent->SoundClassOverride = MusicClass;
		}
	}

	BGMComponent->SetVolumeMultiplier(ResolveBGMComponentVolume());
}

float UDominationClassSelectScreen::ResolveBGMComponentVolume()
{
	UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (!IsValid(Settings))
	{
		return 1.0f;
	}

	const bool bHasMusicClass = IsValid(Settings->GetAudioChannelSoundClass(EShooterAudioChannel::Music));
	const bool bHasUserMix = IsValid(Settings->GetUserSettingsSoundMix());
	return bHasMusicClass && bHasUserMix
		? 1.0f
		: FMath::Clamp(Settings->GetAudioVolume(EShooterAudioChannel::Music), 0.0f, 2.0f);
}

void UDominationClassSelectScreen::HandleCombatClassChanged(
	const EDominationCombatClass SelectedClass)
{
	if (IsActiveClassReselectionMode())
	{
		ActiveReselectionPreviewClass = SelectedClass;
	}
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
	IRogueCombatClassSelector* ClassSelector = Cast<IRogueCombatClassSelector>(GetOwningPlayer());
	if (!IsValid(PlayerState) || !ClassSelector)
	{
		return;
	}

	SelectionFailure = FText::GetEmpty();
	if (IsActiveClassReselectionMode())
	{
		ActiveReselectionPreviewClass = RequestedClass;
		RefreshSelectionModePresentation();
		return;
	}

	switch (PlayerState->GetParticipationState())
	{
	case EPlayerParticipationState::WaitingForMatchStart:
		ClassSelector->SetPreviewCombatClass(RequestedClass);
		break;

	case EPlayerParticipationState::WaitingForNextSpawn:
		ClassSelector->SelectCombatClass(RequestedClass);
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
		StartHoverUpdates();
		return;
	}

	const int32 HoveredIndex = ClassCards.IndexOfByPredicate(
		[HoveredClass](const UDominationClassSelectCard* Card)
		{
			return IsValid(Card) && Card->GetCombatClass() == HoveredClass;
		});

	CardHoverController.SetHovered(HoveredIndex);
	StartHoverUpdates();
}

void UDominationClassSelectScreen::StartHoverUpdates()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(HoverUpdateTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			HoverUpdateTimerHandle,
			this,
			&ThisClass::UpdateHoverAnimation,
			FMath::Max(0.008f, HoverUpdateInterval),
			true);
	}
}

void UDominationClassSelectScreen::UpdateHoverAnimation()
{
	const float DeltaTime = FMath::Max(0.008f, HoverUpdateInterval);
	const bool bChanged = CardHoverController.Tick(DeltaTime, CardHoverSettings);

	const TArray<FDominationCardHoverState>& States = CardHoverController.GetStates();
	for (int32 Index = 0; Index < ClassCards.Num(); ++Index)
	{
		if (States.IsValidIndex(Index) && IsValid(ClassCards[Index]))
		{
			ClassCards[Index]->ApplyHoverState(States[Index]);
		}
	}

	if (!bChanged)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HoverUpdateTimerHandle);
		}
		HoverUpdateTimerHandle.Invalidate();
	}
}

void UDominationClassSelectScreen::HandleLockInClicked()
{
	if (bLockInRequestPending)
	{
		return;
	}
	
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	IRogueCombatClassSelector* ClassSelector = Cast<IRogueCombatClassSelector>(GetOwningPlayer());
	if (!IsValid(PlayerState) || !ClassSelector)
	{
		return;
	}

	const bool bActiveReselection = IsActiveClassReselectionMode();
	const EDominationCombatClass RequestedClass = bActiveReselection
		? ActiveReselectionPreviewClass
		: PlayerState->GetPreviewCombatClass();
	if (RequestedClass == EDominationCombatClass::Unselected)
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

	if (USoundBase* SelectionVoice =
			GetCachedSelectionVoice(RequestedClass))
	{
		UGameplayStatics::PlaySound2D(this, SelectionVoice);
	}
	
	if (IsValid(PreviewStage))
	{
		PreviewStage->PlayLockInEffect();
	}

	ClassSelector->SelectCombatClass(RequestedClass);
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

USoundBase* UDominationClassSelectScreen::GetCachedSelectionVoice(
	const EDominationCombatClass CombatClass) const
{
	return ClassVoiceCache.FindRef(CombatClass);
}

EDominationCombatClass UDominationClassSelectScreen::GetDisplayCombatClass() const
{
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	if (!IsValid(PlayerState))
	{
		return EDominationCombatClass::Unselected;
	}

	if (IsActiveClassReselectionMode()
		&& ActiveReselectionPreviewClass != EDominationCombatClass::Unselected)
	{
		return ActiveReselectionPreviewClass;
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
	ClassVoiceCache.Empty();

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

		if (USoundBase* Voice = Definition.SelectionVoice.LoadSynchronous())
		{
			ClassVoiceCache.Add(Definition.CombatClass, Voice);
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
		Presentation.Portrait = GetCachedClassPortrait(Definition.CombatClass);

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
	if (!bEnableCharacterPreview)
	{
		if (IsValid(Image_CharacterPreview))
		{
			Image_CharacterPreview->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !ensure(PreviewStageClass))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 스테이지가 로컬 커스터마이징 저장본을 읽을 LocalPlayer
	SpawnParams.Owner = GetOwningPlayer();

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
	const bool bActiveReselection = IsActiveClassReselectionMode();
	const EDominationCombatClass PreviewClass = bActiveReselection
		? ActiveReselectionPreviewClass
		: IsValid(PlayerState)
			? PlayerState->GetPreviewCombatClass()
			: EDominationCombatClass::Unselected;
	const bool bLocked = !bActiveReselection
		&& IsValid(PlayerState)
		&& PlayerState->HasSelectedCombatClass();

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
	const bool bActiveReselection = IsActiveClassReselectionMode();
	const bool bLocked = !bActiveReselection
		&& IsValid(PlayerState)
		&& PlayerState->HasSelectedCombatClass();
	const bool bHasPreview = bActiveReselection
		? ActiveReselectionPreviewClass != EDominationCombatClass::Unselected
		: IsValid(PlayerState) && PlayerState->HasPreviewCombatClass();
	if (IsValid(Button_LockIn))
	{
		Button_LockIn->SetIsEnabled(
			!bLockInRequestPending && (bActiveReselection ? bHasPreview : (!bLocked && bHasPreview)));
	}

	BP_UpdateLockInPresentation(bLocked, bHasPreview, bLockInRequestPending);
}

void UDominationClassSelectScreen::RefreshTimerText()
{
    if (!IsValid(Text_Timer))
    {
        return;
    }
    const IRogueCombatClassSelector* ClassSelector =
        Cast<IRogueCombatClassSelector>(GetOwningPlayer());
    const float Remaining = ClassSelector
        ? ClassSelector->GetCombatClassSelectionTimeRemaining() : -1.0f;
    if (Remaining < 0.0f)
    {
        Text_Timer->SetText(SelectionFailure);
        return;
    }
    const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(Remaining));
    Text_Timer->SetText(FText::FromString(
        FString::Printf(TEXT("%02d:%02d"), TotalSeconds / 60, TotalSeconds % 60)));
}

void UDominationClassSelectScreen::NotifySelectionFailed(const FText& Reason)
{
    bLockInRequestPending = false;
    SelectionFailure = Reason;
    RefreshSelectionState();
    RefreshLockInButton();
    RefreshSelectionModePresentation();
}

void UDominationClassSelectScreen::SetAllowActiveClassReselection(const bool bAllow)
{
    bAllowActiveClassReselection = bAllow;
    const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
    ActiveReselectionPreviewClass = bAllow && IsValid(PlayerState)
        && PlayerState->HasSelectedCombatClass()
        && PlayerState->GetParticipationState() == EPlayerParticipationState::Active
        ? PlayerState->GetCombatClass()
        : EDominationCombatClass::Unselected;
    bLockInRequestPending = false;
    SelectionFailure = FText::GetEmpty();
    RefreshSelectionModePresentation();
}

bool UDominationClassSelectScreen::IsActiveClassReselectionMode() const
{
    const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
    return bAllowActiveClassReselection
        && IsValid(PlayerState)
        && PlayerState->HasSelectedCombatClass()
        && PlayerState->GetParticipationState() == EPlayerParticipationState::Active;
}

void UDominationClassSelectScreen::RefreshSelectionModePresentation()
{
    const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
    const bool bWaitingForInitialSelection = IsValid(PlayerState)
        && PlayerState->GetParticipationState() == EPlayerParticipationState::WaitingForMatchStart;
    const bool bJoinInProgressSelection = IsValid(PlayerState)
        && PlayerState->GetParticipationState() == EPlayerParticipationState::WaitingForNextSpawn;
    const bool bActiveReselection = IsActiveClassReselectionMode();

    if (IsValid(Button_LockIn))
    {
        Button_LockIn->SetVisibility(
            (bWaitingForInitialSelection || bActiveReselection) && !bJoinInProgressSelection
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TimerTextHandle);
    }

    if (IsValid(Text_Timer))
    {
        const bool bShowStatusText = bWaitingForInitialSelection || !SelectionFailure.IsEmpty();
        Text_Timer->SetVisibility(bShowStatusText
            ? ESlateVisibility::HitTestInvisible
            : ESlateVisibility::Collapsed);
    }

    if (bWaitingForInitialSelection)
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
    else if (!SelectionFailure.IsEmpty() && IsValid(Text_Timer))
    {
        Text_Timer->SetText(SelectionFailure);
    }

    RefreshSelectionState();
    RefreshLockInButton();
    RefreshClassInfoPanel();
    RefreshPreviewStage();
}
